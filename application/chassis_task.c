/* (C) COPYRIGHT 2019 DJI. F407 swerve command shaping before the motor PID loops. */
#include "arm_math.h"
#include "CAN_receive.h"
#include "chassis_behaviour.h"
#include "chassis_calculate.h"
#include "chassis_soft_turn_log.h"
#include "chassis_task.h"
#include "cmsis_os.h"
#include "robot_param.h"

#if INCLUDE_uxTaskGetStackHighWaterMark
uint32_t chassis_high_water;
#endif
chassis_move_t chassis_move;
fp32 chassis_follow_gimbal_yaw_offset = CHASSIS_FOLLOW_GIMBAL_YAW_OFFSET;
fp32 chassis_spin_offset = CHASSIS_SPIN_OFFSET;

static uint8_t chassis_feedback_update(chassis_move_t *move)
{
    uint8_t i;
    fp32 yaw = move->chassis_INS_angle[INS_YAW_ADDRESS_OFFSET];
    fp32 pitch = move->chassis_INS_angle[INS_PITCH_ADDRESS_OFFSET];
    fp32 roll = move->chassis_INS_angle[INS_ROLL_ADDRESS_OFFSET];
    if (!chassis_soft_turn_finite(yaw) || !chassis_soft_turn_finite(pitch) ||
        !chassis_soft_turn_finite(roll) || !chassis_soft_turn_finite(move->gimbal_radian_of_ecd))
        return 0U;
    for (i = 0; i < 4U; ++i)
    {
        move->chassis_3508[i].speed = move->chassis_3508[i].chassis_motor_measure->speed_rpm / MPS_to_RPM;
        move->chassis_3508[i].accel = move->chas_3508_pid[i].Dbuf[0] * CHASSIS_CONTROL_FREQUENCE;
        move->chassis_6020[i].angle = chassis_soft_turn_wrap(
            move->chassis_6020[i].chassis_motor_measure->ecd / GM6020_Angle_Ratio);
    }
    move->chassis_yaw = chassis_soft_turn_wrap(yaw);
    move->chassis_pitch = chassis_soft_turn_wrap(pitch);
    move->chassis_roll = chassis_soft_turn_wrap(roll);
    return 1U;
}

static void chassis_reset_control(chassis_move_t *move)
{
    uint8_t i;
    fp32 angle[4];
    for (i = 0; i < 4U; ++i)
    {
        angle[i] = move->chassis_6020[i].angle;
        PID_clear(&move->chas_3508_pid[i]);
        PID_clear(&move->chas_6020_angle_pid[i]);
        PID_clear(&move->chas_6020_speed_pid[i]);
        move->chassis_3508[i].speed_set = 0.0f;
        move->chassis_3508[i].give_current = 0;
        move->chassis_6020[i].angle_set = angle[i];
        move->chassis_6020[i].give_current = 0;
    }
    chassis_soft_turn_reset(&move->soft_turn, angle);
    move->vx_set = move->vy_set = move->wz_set = 0.0f;
    move->raw_target.vx = move->raw_target.vy = move->raw_target.wz = 0.0f;
}

static void chassis_init(chassis_move_t *move)
{
    const static fp32 drive_pid[3] = {M3505_MOTOR_SPEED_PID_KP, M3505_MOTOR_SPEED_PID_KI, M3505_MOTOR_SPEED_PID_KD};
    const static fp32 angle_pid[3] = {GM6020_MOTOR_ANGLE_PID_KP, GM6020_MOTOR_ANGLE_PID_KI, GM6020_MOTOR_ANGLE_PID_KD};
    const static fp32 steer_pid[3] = {GM6020_MOTOR_SPEED_PID_KP, GM6020_MOTOR_SPEED_PID_KI, GM6020_MOTOR_SPEED_PID_KD};
    uint8_t i;
    move->chassis_mode = CHASSIS_VECTOR_NO_MOVE;
    move->chassis_RC = get_remote_control_point();
    move->chassis_INS_angle = get_INS_angle_point();
    for (i = 0; i < 4U; ++i)
    {
        move->chassis_3508[i].chassis_motor_measure = get_chassis_motor_measure_point(i);
        move->chassis_6020[i].chassis_motor_measure = get_chassis_motor_measure_point(i + 4U);
        PID_init(&move->chas_3508_pid[i], PID_USUAL, drive_pid,
            M3505_MOTOR_SPEED_PID_MAX_OUT, M3505_MOTOR_SPEED_PID_MAX_IOUT);
        PID_init(&move->chas_6020_angle_pid[i], PID_USUAL, angle_pid,
            GM6020_MOTOR_ANGLE_PID_MAX_OUT, GM6020_MOTOR_ANGLE_PID_MAX_IOUT);
        PID_init(&move->chas_6020_speed_pid[i], PID_USUAL, steer_pid,
            GM6020_MOTOR_SPEED_PID_MAX_OUT, GM6020_MOTOR_SPEED_PID_MAX_IOUT);
    }
    move->vx_max_speed = NORMAL_MAX_CHASSIS_SPEED_X;
    move->vx_min_speed = -NORMAL_MAX_CHASSIS_SPEED_X;
    move->vy_max_speed = NORMAL_MAX_CHASSIS_SPEED_Y;
    move->vy_min_speed = -NORMAL_MAX_CHASSIS_SPEED_Y;
    chassis_wheel_angle_offset_init();
    (void)chassis_feedback_update(move);
    chassis_reset_control(move);
}

void chassis_rc_to_control_vector(fp32 *vx_set, fp32 *vy_set, chassis_move_t *move)
{
    int16_t vx_channel, vy_channel;
    if (move == NULL || vx_set == NULL || vy_set == NULL) return;
    rc_deadband_limit(move->chassis_RC->rc.ch[CHASSIS_X_CHANNEL], vx_channel, CHASSIS_RC_DEADLINE);
    rc_deadband_limit(move->chassis_RC->rc.ch[CHASSIS_Y_CHANNEL], vy_channel, CHASSIS_RC_DEADLINE);
    *vx_set = vx_channel * CHASSIS_VX_RC_SEN;
    *vy_set = vy_channel * CHASSIS_VY_RC_SEN;
    if (move->chassis_RC->key.v & CHASSIS_FRONT_KEY) *vx_set = move->vx_max_speed;
    else if (move->chassis_RC->key.v & CHASSIS_BACK_KEY) *vx_set = move->vx_min_speed;
    if (move->chassis_RC->key.v & CHASSIS_LEFT_KEY) *vy_set = move->vy_max_speed;
    else if (move->chassis_RC->key.v & CHASSIS_RIGHT_KEY) *vy_set = move->vy_min_speed;
}

static uint8_t chassis_set_contorl(chassis_move_t *move, fp32 dt)
{
    fp32 vector[2], rotation_offset;
    chassis_twist_t output;
    uint8_t fast_stop = move->chassis_mode == CHASSIS_VECTOR_NO_MOVE;
    chassis_behaviour_control_set(&move->raw_target.vx, &move->raw_target.vy,
        &move->raw_target.wz, move);
    move->raw_target.wz = -move->raw_target.wz;
    if (!fast_stop)
    {
        rotation_offset = move->chassis_mode == CHASSIS_VECTOR_SPIN ?
            chassis_spin_offset : chassis_follow_gimbal_yaw_offset;
        if (!chassis_soft_turn_finite(rotation_offset)) return 0U;
        vector[0] = move->raw_target.vx;
        vector[1] = move->raw_target.vy;
        vector_rotate(move->gimbal_radian_of_ecd + rotation_offset, vector);
        move->raw_target.vx = vector[0];
        move->raw_target.vy = vector[1];
    }
    if (!chassis_soft_turn_shape_twist(&move->soft_turn, &move->raw_target,
        move->chassis_yaw, dt, fast_stop, &output)) return 0U;
    move->vx_set = output.vx;
    move->vy_set = output.vy;
    move->wz_set = output.wz;
    return 1U;
}

static uint8_t chassis_control_loop(chassis_move_t *move, fp32 dt)
{
    fp32 wheel_angle[4], wheel_speed[4], measured_angle[4], measured_speed[4];
    fp32 adjusted_target;
    uint8_t i;
    chas_inv_cal(move->vx_set, move->vy_set, move->wz_set, wheel_angle, wheel_speed);
    for (i = 0; i < 4U; ++i)
    {
        measured_angle[i] = move->chassis_6020[i].angle;
        measured_speed[i] = move->chassis_3508[i].speed;
    }
    if (!chassis_soft_turn_shape_wheels(&move->soft_turn, wheel_angle, wheel_speed,
        measured_angle, measured_speed, dt, move->chassis_mode == CHASSIS_VECTOR_NO_MOVE)) return 0U;
    for (i = 0; i < 4U; ++i)
    {
        move->chassis_3508[i].speed_set = move->soft_turn.speed[i];
        move->chassis_6020[i].angle_set = move->soft_turn.angle[i];
        PID_calc(&move->chas_3508_pid[i], move->chassis_3508[i].speed,
            move->chassis_3508[i].speed_set);
        adjusted_target = move->chassis_6020[i].angle + chassis_soft_turn_wrap(
            move->chassis_6020[i].angle_set - move->chassis_6020[i].angle);
        PID_calc(&move->chas_6020_angle_pid[i], move->chassis_6020[i].angle, adjusted_target);
        PID_calc(&move->chas_6020_speed_pid[i],
            move->chassis_6020[i].chassis_motor_measure->speed_rpm,
            move->chas_6020_angle_pid[i].out);
        move->chassis_3508[i].give_current = (int16_t)move->chas_3508_pid[i].out;
        move->chassis_6020[i].give_current = (int16_t)move->chas_6020_speed_pid[i].out;
    }
    return 1U;
}

void chassis_task(void const *pvParameters)
{
    TickType_t wake_tick, previous_tick, now_tick;
    uint32_t fault_generation = 0U;
    uint8_t inhibited = 1U, rc_fault_latched = 0U, invalid;
    fp32 dt;
    (void)pvParameters;
    vTaskDelay(CHASSIS_TASK_INIT_TIME);
    chassis_init(&chassis_move);
    wake_tick = previous_tick = xTaskGetTickCount();
    while (1)
    {
        now_tick = xTaskGetTickCount();
        dt = chassis_soft_turn_dt((fp32)(now_tick - previous_tick) / configTICK_RATE_HZ);
        previous_tick = now_tick;
        /* The RC error checker repairs rc_ctrl; release only after a new frame. */
        invalid = RC_data_is_error();
        if (invalid)
        {
            rc_fault_latched = 1U;
            fault_generation = get_remote_control_generation();
        }
        else if (rc_fault_latched && get_remote_control_generation() != fault_generation)
            rc_fault_latched = 0U;
        invalid = invalid || rc_fault_latched;
        invalid = !chassis_feedback_update(&chassis_move) || invalid;
        invalid = !chassis_motor_feedback_ready() || invalid;
        if (!invalid)
        {
            if (inhibited) chassis_reset_control(&chassis_move);
            inhibited = 0U;
            chassis_behaviour_mode_set(&chassis_move);
            invalid = !chassis_set_contorl(&chassis_move, dt);
            if (!invalid) invalid = !chassis_control_loop(&chassis_move, dt);
        }
        if (invalid)
        {
            chassis_reset_control(&chassis_move);
            inhibited = 1U;
        }
        CAN_cmd_CHAS_6020(chassis_move.chassis_6020[0].give_current,
            chassis_move.chassis_6020[1].give_current, chassis_move.chassis_6020[2].give_current,
            chassis_move.chassis_6020[3].give_current);
        CAN_cmd_CHAS_3508(chassis_move.chassis_3508[0].give_current,
            chassis_move.chassis_3508[1].give_current, chassis_move.chassis_3508[2].give_current,
            chassis_move.chassis_3508[3].give_current);
        chassis_soft_turn_log_publish(&chassis_move,
            (uint32_t)now_tick * (1000U / configTICK_RATE_HZ), invalid);
#if INCLUDE_uxTaskGetStackHighWaterMark
        chassis_high_water = uxTaskGetStackHighWaterMark(NULL);
#endif
        /* Avoid replaying several PID iterations after a missed deadline. */
        if ((TickType_t)(xTaskGetTickCount() - wake_tick) >= CHASSIS_CONTROL_TIME_MS)
            wake_tick = xTaskGetTickCount();
        vTaskDelayUntil(&wake_tick, pdMS_TO_TICKS(CHASSIS_CONTROL_TIME_MS));
    }
}
