/**
  ****************************(C) COPYRIGHT 2019 DJI****************************
  * @file       chassis.h
  * @brief     	底盘控制任务
	*
	*     000000000000000     00               00    00     00   00         00 
	*           00     0      00                00    00   00     00       00  
	*       00  00000        00000000000000      00  000000000   00000000000000
	*       00  00          00           00    00    000000000     00     00   
	*      00000000000     00  000000    00     000     00           000000    
	*     00    00   0000     00    00   00      00   000000           00      
	*       00000000000       00    00   00           000000           00      
	*       0   00    0       0000000 00 00       00    00       00000000000000
	*       00000000000       00       000       00 00000000000        00      
	*           00            00                000 00000000000        00      
	*           00  00        00          0    000      00             00      
	*      000000000000        00        000  000       00          00 00      
	*       00        00        000000000000            00            00       
	********************************************************************************/
	
#ifndef CHASSIS_TASK_H
#define CHASSIS_TASK_H

#include "struct_typedef.h"
#include "CAN_receive.h"
#include "pid.h"
#include "remote_control.h"
#include "user_lib.h"
#include "INS_task.h"
#include "chassis_soft_turn.h"

#define CHASSIS_TASK_INIT_TIME 357	//任务开始空闲一段时间

#define CHASSIS_X_CHANNEL			1		//前后的遥控器通道号码
#define CHASSIS_Y_CHANNEL			0		//左右的遥控器通道号码
#define CHASSIS_WZ_CHANNEL		2		//在特殊模式下，可以通过遥控器控制旋转
#define CHASSIS_MODE_CHANNEL 	0		//选择底盘状态 开关通道号

//遥控器前进摇杆（max 660）转化成车体前进速度（m/s）的比例
#define CHASSIS_VX_RC_SEN 	0.004f	//0.006f

//遥控器左右摇杆（max 660）转化成车体左右速度（m/s）的比例
#define CHASSIS_VY_RC_SEN 	0.004f	//0.006f

//摇杆死区
#define CHASSIS_RC_DEADLINE 25

#define CHASSIS_CONTROL_TIME_MS 1
#define CHASSIS_CONTROL_TIME 0.001f
#define CHASSIS_CONTROL_FREQUENCE 1000.0f

//底盘前后左右控制按键
#define CHASSIS_FRONT_KEY		KEY_PRESSED_OFFSET_W
#define CHASSIS_BACK_KEY 		KEY_PRESSED_OFFSET_S
#define CHASSIS_LEFT_KEY 		KEY_PRESSED_OFFSET_A
#define CHASSIS_RIGHT_KEY		KEY_PRESSED_OFFSET_D

#define MAX_WHEEL_SPEED							3.0f	//单个底盘电机最大速度

/* 底盘6020和3508最大can发送电流值 */
#define MAX_6020_CAN_CURRENT 16000.0f
#define MAX_3508_CAN_CURRENT 16000.0f

/* 舵轮解算用到的数据 */
#define M3508_RR						19.20320855f				//M3508电机减速比(M3508 Reduction Ratio)：3591 / 187
#define GM6020_Angle_Ratio	1303.63813886f			//把计算出的弧度Radian转换为GM6020电机角度Angle的系数：8191 / (2 * PI)
#define Wheel_Radius				0.0567f							//车轮的半径，单位：米
#define Wheel_Perimeter			0.35625661f					//车轮周长：2.0f * PI * Wheel_Radius，单位：米
#define MPS_to_RPM					3234.16461897f			//把M3508线速度m/s -> 转速r/min：60.0f * M3508_RR / Wheel_Perimeter
#define RPM_to_Icmd					1.55125592f					//把M3508电机的转速RPM -> 控制电流Icmd(适用于开环)：(187 /3591) * (16384 / 550)

/* 底盘3508电机PID */	/***************待定***************/
#define M3505_MOTOR_SPEED_PID_KP 				30000.0f
#define M3505_MOTOR_SPEED_PID_KI 				10.0f
#define M3505_MOTOR_SPEED_PID_KD				0.0f
#define M3505_MOTOR_SPEED_PID_MAX_OUT 	16000.0f
#define M3505_MOTOR_SPEED_PID_MAX_IOUT	100.0f

/* 底盘6020电机PID */	/***************待定***************/
#define GM6020_MOTOR_ANGLE_PID_KP 				160.0f
#define GM6020_MOTOR_ANGLE_PID_KI 				0.01f
#define GM6020_MOTOR_ANGLE_PID_KD					0.0f
#define GM6020_MOTOR_ANGLE_PID_MAX_OUT 		120.0f
#define GM6020_MOTOR_ANGLE_PID_MAX_IOUT		10.0f


//YAW回正模式PID

//yaw 速度环 PID参数以及 PID最大输出，积分输出
#define GM6020_MOTOR_SPEED_PID_KP        65.0f
#define GM6020_MOTOR_SPEED_PID_KI        0.0f
#define GM6020_MOTOR_SPEED_PID_KD        80.0f
#define GM6020_MOTOR_SPEED_PID_MAX_OUT   16000.0f
#define GM6020_MOTOR_SPEED_PID_MAX_IOUT  0.0f

/* 底盘旋转跟随PID */	/***************待定***************/


#ifndef myabs
	#define myabs(x) (((x) >= 0) ? (x) : (-(x)))
#endif


#define rc_deadband_limit(input, output, dealine)	  \
{                                                		\
	if ((input) > (dealine) || (input) < -(dealine))	\
	{                                             		\
		(output) = (input);                          		\
	}                                             		\
	else                                          		\
	{                                             		\
		(output) = 0;                          					\
	}                                          				\
}

typedef enum
{
	CHASSIS_VECTOR_NO_MOVE = 0,					//底盘不动
  CHASSIS_VECTOR_FOLLOW_GIMBAL_YAW = 1,   //底盘会跟随云台相对角度
  CHASSIS_VECTOR_SPIN = 2,								//底盘有旋转速度控制
} chassis_mode_e;

typedef struct
{
  const MOTOR_MEASURE_t *chassis_motor_measure;
  fp32 accel;
  fp32 speed;
  fp32 speed_set;
	fp32 angle;
	fp32 angle_set;
  int16_t give_current;
	
} chassis_motor_t;

typedef struct
{
	fp32 now[4], initial[4];
}	wheel_angle_offset_t;

typedef struct
{
	const RC_ctrl_t *chassis_RC;              	//底盘使用的遥控器指针
	const fp32 *chassis_INS_angle;            	//取陀螺仪解算出的欧拉角指针
	chassis_mode_e chassis_mode;              	//底盘控制状态机
	chassis_motor_t chassis_3508[4];						//底盘3508电机数据
	chassis_motor_t chassis_6020[4];						//底盘6020电机数据
	wheel_angle_offset_t wheel_angle_offset;		//底盘6020电机角度偏移
	pid_type_def chas_3508_pid[4];         			//底盘3508电机速度pid
	pid_type_def chas_6020_angle_pid[4];        //底盘6020电机角度pid
	pid_type_def chas_6020_speed_pid[4];        //底盘6020电机角度pid
	chassis_soft_turn_t soft_turn;
	chassis_twist_t raw_target;
	
	fp32 vx;                          //底盘速度 前进方向 前为正，单位 m/s
	fp32 vy;                          //底盘速度 左右方向 左为正  单位 m/s
	fp32 wz;                          //底盘旋转角速度，逆时针为正 单位 rad/s
	fp32 vx_set;											//底盘设定速度 前进方向 前为正，单位 m/s
	fp32 vy_set;											//底盘设定速度 左右方向 左为正，单位 m/s
	fp32 wz_set;											//底盘设定旋转角速度，逆时针为正，单位 rad/s
	fp32 gimbal_radian_of_ecd;				//云台yaw电机反馈的弧度值
	uint8_t gimbal_behaviour;					//云台当前行为模式
	uint8_t gimbal_shoot_mode;				//云台当前射击模式
	
	fp32 vx_max_speed;  //前进方向最大速度 单位m/s
	fp32 vx_min_speed;  //后退方向最大速度 单位m/s
	fp32 vy_max_speed;  //左方向最大速度 单位m/s
	fp32 vy_min_speed;  //右方向最大速度 单位m/s
	fp32 chassis_yaw;   //陀螺仪和云台电机叠加的yaw角度
	fp32 chassis_pitch; //陀螺仪和云台电机叠加的pitch角度
	fp32 chassis_roll;  //陀螺仪和云台电机叠加的roll角度
	
} chassis_move_t;


extern void chassis_task(void const *pvParameters);
extern void chassis_rc_to_control_vector(fp32 *vx_set, fp32 *vy_set, chassis_move_t *chassis_move_rc_to_vector);

//底盘运动数据
extern chassis_move_t chassis_move;


#endif
