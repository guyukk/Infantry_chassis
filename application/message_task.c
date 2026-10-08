#include "message_task.h"
#include "main.h"
#include "cmsis_os.h"
#include "bsp_usart.h"
#include "arm_math.h"
#include "CAN_receive.h"
#include "user_lib.h"
#include "remote_control.h"
#include "INS_task.h"
#include "pid.h"
#include "chassis_task.h"
#include "chassis_behaviour.h"
#include "protocol.h"


static void send_to_gimbal(void);


/**
  * @brief          云台任务，间隔 GIMBAL_CONTROL_TIME 1ms
  * @param[in]      pvParameters: 空
  * @retval         none
  */
void message_task(void const *pvParameters)
{
    // 初始化延迟
    vTaskDelay(MESSAGE_TASK_INIT_TIME * 5);

    while(1) {
        /* 本工程固件为底盘板（chassis_board）：向云台板发送底盘状态。 */
        send_to_gimbal();
				vTaskDelay(MESSAGE_TASK_INIT_TIME * 2);
    }
}

//目前发送给云台的数据：imu的yaw值和pitch值和roll值、底盘行为模式、机器人ID、底盘速度vx、vy、wz
//'s'为帧头，'e'为帧尾
static void send_to_gimbal(void)
{
	static fp32_to_bytes yaw_now, roll_now, pitch_now;		//imu的yaw值和pitch值和roll值
	static uint8_t chassis_behaviour_now;									//底盘行为模式
	static uint8_t Robot_ID;															//机器人ID
	static fp32_to_bytes	chassis_vx_now;									//底盘速度vx
	static fp32_to_bytes	chassis_vy_now;									//底盘速度vy
	static fp32_to_bytes	chassis_wz_now;									//底盘速度wz

	//length = 2(帧头和帧尾) + 4*3 + 1 + 1 + 4*3 = 28
	const static uint8_t length = 28;
	static uint8_t data_gimbal[length];
  
	yaw_now.fp32 = chassis_move.chassis_yaw;
  pitch_now.fp32 = chassis_move.chassis_pitch;
  roll_now.fp32 = chassis_move.chassis_roll;
	extern chassis_behaviour_e chassis_behaviour_mode;
	chassis_behaviour_now = (uint8_t)chassis_behaviour_mode;
	extern robot_status_t robot_status;
	Robot_ID = robot_status.robot_id;
	chassis_vx_now.fp32 = chassis_move.vx;
	chassis_vy_now.fp32 = chassis_move.vy;
	chassis_wz_now.fp32 = chassis_move.wz;

	/*******************数据段*******************/
  data_gimbal[0] = 's';		//帧头
	
	// 提取每个字节
  data_gimbal[1] = yaw_now.bytes[0];
  data_gimbal[2] = yaw_now.bytes[1];
  data_gimbal[3] = yaw_now.bytes[2];
  data_gimbal[4] = yaw_now.bytes[3];
	
	data_gimbal[5] = pitch_now.bytes[0];
  data_gimbal[6] = pitch_now.bytes[1];
  data_gimbal[7] = pitch_now.bytes[2];
  data_gimbal[8] = pitch_now.bytes[3];
	
  data_gimbal[9]  = roll_now.bytes[0];
  data_gimbal[10] = roll_now.bytes[1];
  data_gimbal[11] = roll_now.bytes[2];
  data_gimbal[12] = roll_now.bytes[3];
	
	data_gimbal[13] = chassis_behaviour_now;
	
	data_gimbal[14] = Robot_ID;
	
  data_gimbal[15] = chassis_vx_now.bytes[0];
  data_gimbal[16] = chassis_vx_now.bytes[1];
  data_gimbal[17] = chassis_vx_now.bytes[2];
  data_gimbal[18] = chassis_vx_now.bytes[3];
	
	data_gimbal[19] = chassis_vy_now.bytes[0];
  data_gimbal[20] = chassis_vy_now.bytes[1];
  data_gimbal[21] = chassis_vy_now.bytes[2];
  data_gimbal[22] = chassis_vy_now.bytes[3];
	
  data_gimbal[23] = chassis_wz_now.bytes[0];
  data_gimbal[24] = chassis_wz_now.bytes[1];
  data_gimbal[25] = chassis_wz_now.bytes[2];
  data_gimbal[26] = chassis_wz_now.bytes[3];	
	
  data_gimbal[27] = 'e';		//帧尾
	/*******************************************/

  usart1_tx_dma_enable(data_gimbal, length);
}

