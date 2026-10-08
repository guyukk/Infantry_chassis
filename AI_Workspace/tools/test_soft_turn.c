/* Compile actual firmware sources; only board I/O, RTOS and CMSIS trig are fixtures. */
#include <math.h>
#include <string.h>
#include "chassis_task.c"
#include "chassis_calculate.c"
#include "chassis_behaviour.c"
#include "CAN_receive.c"
#include "remote_control.c"
#include "fifo.h"

extern void USART6_IRQHandler(void);

volatile struct {
    uint32_t status, checks, failure, scenario, step, frames;
    fp32 turn_seconds, minimum_turn_speed;
} test_result;
CAN_HandleTypeDef hcan1, hcan2;
UART_HandleTypeDef huart1, huart3, huart6;
DMA_HandleTypeDef hdma_usart3_rx, hdma_usart1_tx, hdma_usart1_rx;
VTM_Data_t vtm_data;
static fp32 imu_angle[3];
static TickType_t fake_tick;
static CAN_RxHeaderTypeDef fake_rx_header;
static uint8_t fake_rx_data[8];
static int16_t sent_drive[4], sent_steer[4];
uint8_t serial_record[CHASSIS_SOFT_LOG_SIZE];
static uint8_t uart_busy;
static const uint8_t *active_dma;
static uint8_t run_task;
static uint32_t task_step;
static uint32_t uart_completion_irqs;

__asm void stop_test(void)
{
    BKPT 0
    BX lr
}
static void check(int condition, uint32_t failure)
{
    ++test_result.checks;
    if (!condition) { test_result.failure = failure; stop_test(); }
}
static int near(fp32 a, fp32 b, fp32 tolerance) { return fabsf(a - b) <= tolerance; }
static fp32 length(fp32 x, fp32 y) { return sqrtf(x*x + y*y); }
float32_t arm_cos_f32(float32_t x) { return cosf(x); }
float32_t arm_sin_f32(float32_t x) { return sinf(x); }
fp32 *get_INS_angle_point(void) { return imu_angle; }
void pm01_response_handle(CAN_RxHeaderTypeDef *header, uint8_t *data) { (void)header; (void)data; }
void RC_restart(uint16_t size) { (void)size; }
void RC_Init(uint8_t *first, uint8_t *second, uint16_t size)
{ (void)first; (void)second; (void)size; }
HAL_StatusTypeDef HAL_CAN_GetRxMessage(CAN_HandleTypeDef *can, uint32_t fifo,
    CAN_RxHeaderTypeDef *header, uint8_t *data)
{
    (void)can; (void)fifo; *header = fake_rx_header; memcpy(data, fake_rx_data, 8); return HAL_OK;
}
HAL_StatusTypeDef HAL_CAN_AddTxMessage(CAN_HandleTypeDef *can, const CAN_TxHeaderTypeDef *header,
    const uint8_t *data, uint32_t *mailbox)
{
    uint8_t i; int16_t *destination = can == &hcan1 ? sent_drive : sent_steer;
    (void)header; *mailbox = 0U;
    for (i=0; i<4; ++i) destination[i] = (int16_t)(((uint16_t)data[2*i] << 8) | data[2*i+1]);
    return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef *uart, const uint8_t *bytes, uint16_t size)
{
    if (uart_busy || uart->gState == HAL_UART_STATE_BUSY_TX) return HAL_BUSY;
    check(size == CHASSIS_SOFT_LOG_SIZE, 900);
    memcpy(serial_record, bytes, size); active_dma = bytes; ++test_result.frames;
    uart->gState = HAL_UART_STATE_BUSY_TX;
    /* Fixture for DMA completion waiting for the final UART TC interrupt. */
    uart->Instance->SR = USART_SR_TC;
    uart->Instance->CR1 |= USART_CR1_TCIE;
    return HAL_OK;
}
void HAL_UART_IRQHandler(UART_HandleTypeDef *uart)
{
    check((uart->Instance->SR & USART_SR_TC) && (uart->Instance->CR1 & USART_CR1_TCIE), 908);
    uart->Instance->CR1 &= ~USART_CR1_TCIE;
    uart->gState = HAL_UART_STATE_READY;
    ++uart_completion_irqs;
}
void usart6_init(uint8_t *first, uint8_t *second, uint16_t size)
{ (void)first; (void)second; (void)size; }
osStatus osDelay(uint32_t ticks) { fake_tick += ticks; return osOK; }
TickType_t xTaskGetTickCount(void) { return fake_tick; }
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t task) { (void)task; return 512U; }
void vTaskDelay(TickType_t ticks) { fake_tick += ticks; }

static void receive_rc(int16_t forward, uint16_t key, uint8_t mode)
{
    uint8_t bytes[18] = {0};
    uint16_t channel[4] = {1024, (uint16_t)(forward + 1024), 1024, 1024};
    bytes[0] = (uint8_t)channel[0];
    bytes[1] = (uint8_t)((channel[0] >> 8) | (channel[1] << 3));
    bytes[2] = (uint8_t)((channel[1] >> 5) | (channel[2] << 6));
    bytes[3] = (uint8_t)(channel[2] >> 2);
    bytes[4] = (uint8_t)((channel[2] >> 10) | (channel[3] << 1));
    bytes[5] = (uint8_t)((channel[3] >> 7) | (mode << 4) | (RC_SW_MID << 6));
    bytes[14] = (uint8_t)key; bytes[15] = (uint8_t)(key >> 8);
    bytes[16] = 0; bytes[17] = 4; /* channel 4 centered */
    sbus_to_rc(bytes, &rc_ctrl);
}

void vTaskDelayUntil(TickType_t *wake, TickType_t increment)
{
    uint8_t i;
    check(increment == 1U, 901);
    *wake += increment; fake_tick = *wake;
    if (huart6.Instance->CR1 & USART_CR1_TCIE) USART6_IRQHandler();
    if (!run_task) return;
    test_result.step = task_step;
    for (i=0; i<4; ++i)
    {
        if (task_step < 3U || (task_step >= 40U && task_step < 50U))
            check(sent_drive[i] == 0 && sent_steer[i] == 0, 902);
    }
    if (task_step == 3U || task_step == 50U)
    {
        check(near(chassis_move.vx_set, 0.002f, 0.00002f), 903);
        check(chassis_move.soft_turn.speed[0] > 0.0f && fabsf(sent_drive[0]) < 100.0f, 904);
    }
    if (task_step == 60U) check(chassis_move.soft_turn.braking, 905);
    if (task_step == 68U)
    {
        check(chassis_move.chassis_mode == CHASSIS_VECTOR_SPIN, 906);
        check(near(chassis_move.wz_set, -0.006f, 0.00001f), 907);
    }
    ++task_step;
    if (task_step == 3U) motor_feedback_seen = 0xFFU;
    if (task_step == 40U) receive_rc(701, 0U, RC_SW_DOWN);
    if (task_step == 50U) receive_rc(660, 0U, RC_SW_DOWN);
    if (task_step == 60U) receive_rc(660, CHASSIS_STOP_KEYBOARD, RC_SW_DOWN);
    if (task_step == 68U) receive_rc(660, 0U, RC_SW_UP);
    if (task_step >= 100U) { test_result.status = 0x600DU; stop_test(); }
}

static void exercise(chassis_soft_turn_t *state, chassis_twist_t target, fp32 yaw, fp32 dt)
{
    chassis_twist_t output;
    fp32 old_x=state->world_vx, old_y=state->world_vy, old_w=state->wz;
    fp32 old_angle[4], old_speed[4], angle[4], speed[4], budget;
    uint8_t i;
    memcpy(old_angle,state->angle,sizeof(old_angle)); memcpy(old_speed,state->speed,sizeof(old_speed));
    check(chassis_soft_turn_shape_twist(state,&target,yaw,dt,0U,&output), 100);
    dt = chassis_soft_turn_dt(dt);
    budget = target.vx == 0.0f && target.vy == 0.0f ? 3.0f : 2.0f;
    check(length(state->world_vx-old_x,state->world_vy-old_y) <= budget*dt+0.000002f, 101);
    check(length(state->world_vx,state->world_vy) <= 2.60001f, 102);
    check(fabsf(state->wz-old_w) <= 6.0f*dt+0.000002f, 103);
    chas_inv_cal(output.vx,output.vy,output.wz,angle,speed);
    check(chassis_soft_turn_shape_wheels(state,angle,speed,old_angle,old_speed,dt,0U), 104);
    for (i=0; i<4; ++i)
    {
        check(fabsf(chassis_soft_turn_wrap(state->angle[i]-old_angle[i])) <= 4.0f*dt+0.000002f, 105);
        check(fabsf(state->speed[i]-old_speed[i]) <= 3.0f*dt+0.000002f, 106);
        check(fabsf(state->speed[i]) <= 3.00001f, 107);
        check(state->speed[i]*old_speed[i] >= 0.0f, 108);
    }
}

static void test_motion(void)
{
    chassis_soft_turn_t state;
    chassis_twist_t target = {2.6f,0.0f,0.0f};
    fp32 angles[4], minimum=10.0f, magnitude, yaw=0.0f, dt;
    uint32_t n;
    uint8_t i;
    for (i=0;i<4;++i) angles[i]=chassis_soft_turn_wrap(-chassis_move.wheel_angle_offset.now[i]);
    chassis_soft_turn_reset(&state,angles);
    test_result.scenario=1;
    for (n=0;n<1400;++n) { test_result.step=n; exercise(&state,target,0.0f,0.001f); }
    check(near(state.world_vx,2.6f,0.00001f),110);
    target.vx=0.0f; target.vy=2.6f;
    test_result.scenario=2;
    for(n=0;n<1840;++n)
    {
        test_result.step=n; exercise(&state,target,0.0f,0.001f);
        magnitude=length(state.world_vx,state.world_vy);
        if(magnitude<minimum) minimum=magnitude;
        if(length(state.world_vx,state.world_vy-2.6f)<0.00001f && test_result.turn_seconds==0.0f)
            test_result.turn_seconds=(n+1)*0.001f;
    }
    test_result.minimum_turn_speed=minimum;
    check(near(test_result.turn_seconds,1.839f,0.005f),111);
    check(near(minimum,2.6f/sqrtf(2.0f),0.001f),112);
    target.vx=target.vy=0.0f; test_result.scenario=3;
    for(n=0;n<1000;++n) { test_result.step=n; exercise(&state,target,0.0f,0.001f); }
    check(state.world_vx==0.0f && state.world_vy==0.0f && state.holding,113);
    target.vx=2.6f; test_result.scenario=4;
    for(n=0;n<1400;++n) exercise(&state,target,0.0f,0.001f);
    target.vx=-2.6f;
    for(n=0;n<2700;++n) { test_result.step=n; exercise(&state,target,0.0f,0.001f); }
    check(near(state.world_vx,-2.6f,0.00001f),114);
    test_result.scenario=5;
    for(n=0;n<2000;++n)
    {
        test_result.step=n;
        dt=(n%4==0)?0.001f:(n%4==1)?0.002f:(n%4==2)?0.005f:0.020f;
        yaw=chassis_soft_turn_wrap(yaw+4.0f*chassis_soft_turn_dt(dt));
        target.vx=(n%400<200)?2.6f:0.0f;
        target.vy=(n%400<200)?0.0f:-2.6f;
        target.wz=(n%600<300)?-4.0f:0.0f;
        exercise(&state,target,yaw,dt);
    }
    test_result.scenario=6; target.vx=target.vy=2.6f; target.wz=0.0f;
    chassis_soft_turn_reset(&state,angles);
    for(n=0;n<1500;++n) exercise(&state,target,0.0f,0.001f);
    check(near(length(state.world_vx,state.world_vy),2.6f,0.00001f),115);
}

static void test_modules(void)
{
    chassis_soft_turn_t state;
    fp32 angle[4]={0}, speed[4]={6,3,1.5f,0}, feedback[4]={0}, measured[4]={0};
    fp32 old_angle[4], old_speed[4];
    uint32_t n;
    uint8_t i;
    test_result.scenario=7;
    chassis_soft_turn_reset(&state,feedback);
    for(n=0;n<250;++n) check(chassis_soft_turn_shape_wheels(&state,angle,speed,feedback,measured,0.005f,0U),200);
    check(near(state.wheel_scale,0.5f,0.00001f),201);
    check(near(state.speed[0],3.0f,0.00001f)&&near(state.speed[1],1.5f,0.00001f)&&near(state.speed[2],0.75f,0.00001f),202);
    for(i=0;i<4;++i) { angle[i]=1.57079632679f; speed[i]=2.6f; }
    chassis_soft_turn_reset(&state,feedback);
    check(chassis_soft_turn_shape_wheels(&state,angle,speed,feedback,measured,0.001f,0U),203);
    check(fabsf(state.speed[0])<0.000001f && near(state.angle[0],0.004f,0.00001f),204);
    test_result.scenario=8;
    for(i=0;i<4;++i) angle[i]=89.0f*0.01745329252f;
    chassis_soft_turn_reset(&state,feedback);
    check(chassis_soft_turn_shape_wheels(&state,angle,speed,feedback,measured,0.001f,0U),205);
    for(n=0;n<100;++n)
    {
        for(i=0;i<4;++i) angle[i]=(n%2?89.0f:91.0f)*0.01745329252f;
        check(chassis_soft_turn_shape_wheels(&state,angle,speed,feedback,measured,0.001f,0U),206);
        check(!state.reversed[0],207);
    }
    for(i=0;i<4;++i) angle[i]=96.0f*0.01745329252f;
    check(chassis_soft_turn_shape_wheels(&state,angle,speed,feedback,measured,0.001f,0U),208);
    check(state.reversed[0],209);
    for(i=0;i<4;++i) angle[i]=89.0f*0.01745329252f;
    check(chassis_soft_turn_shape_wheels(&state,angle,speed,feedback,measured,0.001f,0U),210);
    check(state.reversed[0],211);
    for(i=0;i<4;++i) angle[i]=84.0f*0.01745329252f;
    check(chassis_soft_turn_shape_wheels(&state,angle,speed,feedback,measured,0.001f,0U),220);
    check(!state.reversed[0],221);
    test_result.scenario=9;
    for(i=0;i<4;++i) { feedback[i]=3.14059265f; angle[i]=-3.14059265f; }
    chassis_soft_turn_reset(&state,feedback);
    check(chassis_soft_turn_shape_wheels(&state,angle,speed,feedback,measured,0.001f,0U),212);
    check(fabsf(chassis_soft_turn_wrap(state.angle[0]-feedback[0]))<0.0021f,213);
    test_result.scenario=10;
    for(i=0;i<4;++i) { feedback[i]=0.2f*(i+1); measured[i]=1.0f; state.speed[i]=2.0f; }
    memcpy(old_angle,feedback,sizeof(old_angle));
    for(n=0;n<500;++n)
    {
        memcpy(old_speed,state.speed,sizeof(old_speed));
        check(chassis_soft_turn_shape_wheels(&state,angle,speed,feedback,measured,0.001f,1U),214);
        for(i=0;i<4;++i)
        {
            check(near(state.angle[i],old_angle[i],0.00001f),215);
            check(fabsf(state.speed[i]-old_speed[i])<=0.005002f,216);
            feedback[i]+=0.0001f; /* A drifting sensor must not move the captured brake target. */
        }
    }
    check(state.speed[0]==0.0f && !state.holding,217);
    memset(measured,0,sizeof(measured));
    for(n=0;n<101;++n) check(chassis_soft_turn_shape_wheels(&state,angle,speed,feedback,measured,0.001f,1U),218);
    check(state.holding,219);
}

static void test_input_units_log(void)
{
    chassis_soft_turn_t state;
    chassis_twist_t target={0}, output;
    fp32 angles[4]={0}, raw_angle[4], raw_speed[4], vx,vy,x,y,w;
    uint8_t i;
    uint32_t n, uart_frames;
    union { uint32_t bits; fp32 value; } invalid;
    test_result.scenario=11;
    check(near(chassis_soft_turn_dt(0.0f),0.001f,0.000001f)&&near(chassis_soft_turn_dt(0.1f),0.005f,0.000001f),300);
    chassis_soft_turn_reset(&state,angles); invalid.bits=0x7FC00000U; target.vx=invalid.value;
    check(!chassis_soft_turn_shape_twist(&state,&target,0,0.001f,0,&output),301);
    target.vx=0; invalid.bits=0x7F800000U;
    check(!chassis_soft_turn_shape_twist(&state,&target,invalid.value,0.001f,0,&output),302);
    check(near(chassis_soft_turn_wrap(31.4159265f),0.0f,0.00001f),303);
    chassis_move.chassis_RC=&rc_ctrl;
    chassis_move.vx_max_speed=chassis_move.vy_max_speed=2.6f;
    chassis_move.vx_min_speed=chassis_move.vy_min_speed=-2.6f;
    rc_ctrl.key.v=CHASSIS_RIGHT_KEY;
    for(n=0;n<6000;++n) { chassis_rc_to_control_vector(&vx,&vy,&chassis_move); check(vy==-2.6f,304); }
    rc_ctrl.key.v=0; rc_ctrl.rc.ch[0]=25; rc_ctrl.rc.ch[1]=-25;
    chassis_rc_to_control_vector(&vx,&vy,&chassis_move); check(vx==0&&vy==0,305);
    test_result.scenario=12;
    check(near(HALF_LENGTH*CHASSIS_SPIN_SPEED,14.9015f*0.04f,0.000001f),312);
    check(near(HALF_WIDTH*CHASSIS_WZ_RC_SEN,14.9015f*4.945e-5f,0.0000001f),313);
    for(n=0;n<80;++n)
    {
        vx=(n%9-4.0f)*0.5f; vy=(n%7-3.0f)*0.5f; w=(n%5-2.0f)*0.01f;
        chas_inv_cal(vx,vy,w*100.0f,raw_angle,raw_speed);
        for(i=0;i<4;++i)
        {
            x=vx+((i==0||i==1)?-1.0f:1.0f)*w*14.9015f;
            y=vy+((i==0||i==3)?1.0f:-1.0f)*w*14.9015f;
            check(near(raw_speed[i],length(x,y),0.00002f),306);
            check(fabsf(chassis_soft_turn_wrap(raw_angle[i]-(atan2f(y,x)-chassis_move.wheel_angle_offset.now[i])))<0.00001f,307);
        }
    }
    test_result.scenario=13;
    chassis_move.raw_target.vx=1.25f; chassis_move.chassis_roll=-0.125f;
    chassis_soft_turn_log_publish(&chassis_move,20,0);
    check(serial_record[0]=='S'&&serial_record[1]=='T'&&serial_record[2]==1,308);
    uart_frames=test_result.frames;
    chassis_soft_turn_log_publish(&chassis_move,39,0); check(test_result.frames==uart_frames,309);
    uart_busy=1; chassis_move.raw_target.vx=2.5f;
    chassis_soft_turn_log_publish(&chassis_move,40,1);
    check(!memcmp(active_dma,serial_record,CHASSIS_SOFT_LOG_SIZE),310);
    uart_busy=0; USART6_IRQHandler();
    check(uart_completion_irqs == 1U && huart6.gState == HAL_UART_STATE_READY,314);
    chassis_soft_turn_log_publish(&chassis_move,60,1);
    check(test_result.frames==uart_frames+1&&serial_record[3]&1,311);
}

void test_entry(void)
{
    uint8_t i;
    huart6.Instance=USART6;
    huart6.gState=HAL_UART_STATE_READY;
    test_result.minimum_turn_speed=0;
    chassis_wheel_angle_offset_init();
    test_motion(); test_modules(); test_input_units_log();
    test_result.scenario=14;
    /* Real CAN callback must wait for all eight feedback packets. */
    motor_feedback_seen=0;
    for(i=0;i<8;++i)
    {
        fp32 angle=chassis_soft_turn_wrap(-chassis_move.wheel_angle_offset.initial[i%4]);
        uint16_t encoder=(uint16_t)((angle<0?angle+6.28318530718f:angle)*GM6020_Angle_Ratio);
        memset(fake_rx_data,0,sizeof(fake_rx_data));
        if(i>=4) { fake_rx_data[0]=(uint8_t)(encoder>>8); fake_rx_data[1]=(uint8_t)encoder; }
        fake_rx_header.StdId=0x201+i;
        HAL_CAN_RxFifo0MsgPendingCallback(i<4?&hcan1:&hcan2);
        check(chassis_motor_feedback_ready()==(i==7),400);
    }
    motor_feedback_seen=0; receive_rc(660,0,RC_SW_DOWN);
    fake_tick=0xFFFFFE8BU; /* The real control loop crosses the uint32 tick boundary. */
    run_task=1; task_step=0; chassis_task(0);
    check(0,499);
}
