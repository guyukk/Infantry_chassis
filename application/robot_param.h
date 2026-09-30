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

#define Infantry_robot 1
#define steering_wheel 1

#define HALF_LENGTH 14.9015f
#define HALF_WIDTH 14.9015f

#define CHASSIS_6020_INIT_ANGLE_0 (-0.75f)
#define CHASSIS_6020_INIT_ANGLE_1 (2.14f)
#define CHASSIS_6020_INIT_ANGLE_2 (1.60f)
#define CHASSIS_6020_INIT_ANGLE_3 (2.13f)

#define GIMBAL_YAW_RADIAN_REVERSE 1

#define CHASSIS_RETURN_TARGET (-3.13f)
#define CHASSIS_RETURN_OFFSET (0.00f)
#define CHASSIS_FOLLOW_GIMBAL_YAW_OFFSET (0.00f)
#define CHASSIS_SPIN_OFFSET (0.50f)

#define WHEEL_RF 1
#define WHEEL_LF 0
#define WHEEL_LB 3
#define WHEEL_RB 2

#define CHASSIS_WZ_RC_SEN 4.945e-5f
#define CHASSIS_SPIN_SPEED 0.04f
#define NORMAL_MAX_CHASSIS_SPEED_X 2.6f
#define NORMAL_MAX_CHASSIS_SPEED_Y 2.6f

#endif
