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


/* 底盘外形尺寸：长、宽均为 29.803，宏保存半长和半宽。 */
#define HALF_LENGTH 14.9015f
#define HALF_WIDTH 14.9015f

/* 四个舵向 6020 电机的初始角度，顺序对应 0 到 3 号电机。 */
#define CHASSIS_6020_INIT_ANGLE_0 (-0.75f)
#define CHASSIS_6020_INIT_ANGLE_1 (2.14f)
#define CHASSIS_6020_INIT_ANGLE_2 (1.60f)
#define CHASSIS_6020_INIT_ANGLE_3 (2.13f)

/* 云台 yaw 反馈方向与本底盘安装方向相反。 */
#define GIMBAL_YAW_RADIAN_REVERSE 1

/* 底盘回正、跟随云台和小陀螺模式的角度偏移。 */
#define CHASSIS_RETURN_TARGET (-3.13f)
#define CHASSIS_RETURN_OFFSET (0.00f)
#define CHASSIS_FOLLOW_GIMBAL_YAW_OFFSET (0.00f)
#define CHASSIS_SPIN_OFFSET (0.50f)

/* 舵轮位置到电机索引的映射。 */
#define WHEEL_RF 1
#define WHEEL_LF 0
#define WHEEL_LB 3
#define WHEEL_RB 2

/* 底盘跟随 yaw 灵敏度、小陀螺角速度及最大平移速度。 */
#define CHASSIS_WZ_RC_SEN 4.945e-5f
#define CHASSIS_SPIN_SPEED 0.04f
#define NORMAL_MAX_CHASSIS_SPEED_X 2.6f
#define NORMAL_MAX_CHASSIS_SPEED_Y 2.6f

#endif
