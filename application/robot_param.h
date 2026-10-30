/**
	****************************(C) COPYRIGHT 2025 PRINTK****************************
	* @file       robot_param.h
	* @brief
	* @note
	* @history
	*  Version    Date            Author          Modification
	*  V1.0.0     3-26-2025       滕勇军          1. done
	********************************************************************************/

#ifndef ROBOT_PARAM_H
#define ROBOT_PARAM_H

#include "struct_typedef.h"

/* 本工程固件配置：普通步兵 + 舵轮底盘 + 底盘板，无其他车型/板型选项。 */


/* 舵轮转向轴中心半间距，单位 m；保持原有轮位映射。 */
#define HALF_LENGTH 0.149015f
#define HALF_WIDTH 0.149015f

/* 四个舵向 6020 电机的初始角度，顺序对应 0 到 3 号电机。 */
#define CHASSIS_6020_INIT_ANGLE_0 (2.691592450f)
#define CHASSIS_6020_INIT_ANGLE_1 (0.028407477f)
#define CHASSIS_6020_INIT_ANGLE_2 (0.389999986f)
#define CHASSIS_6020_INIT_ANGLE_3 (-2.73318529f)

/* 云台 yaw 反馈方向与本底盘安装方向相反。 */
#define GIMBAL_YAW_RADIAN_REVERSE 1

/* 底盘回正、跟随云台和小陀螺模式的角度偏移。 */
#define CHASSIS_FOLLOW_GIMBAL_YAW_OFFSET (0.00f)
#define CHASSIS_SPIN_OFFSET (0.50f)

/* 舵轮位置到电机索引的映射。 */
#define WHEEL_RF 1
#define WHEEL_LF 2
#define WHEEL_LB 3
#define WHEEL_RB 0

/* 底盘跟随 yaw 灵敏度、小陀螺角速度及最大平移速度。 */
#define CHASSIS_WZ_RC_SEN 0.004945f
#define CHASSIS_SPIN_SPEED 4.0f
#define NORMAL_MAX_CHASSIS_SPEED_X 2.6f
#define NORMAL_MAX_CHASSIS_SPEED_Y 2.6f

/* Soft-turn limits: m/s, m/s^2, rad/s and seconds. */
#define CHASSIS_SOFT_MAX_TRANSLATION 2.6f
#define CHASSIS_SOFT_VECTOR_ACCEL 2.0f
#define CHASSIS_SOFT_RELEASE_DECEL 3.0f
#define CHASSIS_SOFT_YAW_ACCEL 6.0f
#define CHASSIS_SOFT_STEER_RATE 4.0f
#define CHASSIS_SOFT_WHEEL_ACCEL 3.0f
#define CHASSIS_SOFT_FAST_DECEL 5.0f
#define CHASSIS_SOFT_MAX_WHEEL_SPEED 3.0f
#define CHASSIS_SOFT_FLIP_HYSTERESIS 0.0872664626f
#define CHASSIS_SOFT_STOP_SPEED 0.05f
#define CHASSIS_SOFT_STOP_TIME 0.100f

/* USART6 is also the referee port. Enable only for an instrumented test build. */
#ifndef CHASSIS_SOFT_TURN_TELEMETRY
#define CHASSIS_SOFT_TURN_TELEMETRY 0
#endif

#endif
