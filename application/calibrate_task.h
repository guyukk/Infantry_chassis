/*****************************(C) COPYRIGHT 2019 DJI****************************
  * @file       calibrate_task.h
  * @brief      校准设备，包括云台,陀螺仪,加速度计,磁力计,底盘.云台校准是主要计算零点
  *             和最大最小相对角度.云台校准是主要计算零漂.加速度计和磁力计校准还没有实现
  *             因为加速度计还没有必要去校准,而磁力计还没有用.底盘校准是使M3508进入快速
  *             设置ID模式.
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

#ifndef CALIBRATE_TASK_H
#define CALIBRATE_TASK_H

#include "struct_typedef.h"

//头部
typedef __packed struct {
	uint8_t self_id;           // 自身ID
	uint16_t firmware_version; // 固件版本
	int8_t temperature;        // IMU控制温度
	fp32 latitude;             // 纬度
	//注:"temperature" 和 "latitude"不应该在head_cali_t, 因为不想创建一个新的设备就放这了
} head_cali_t;

//云台
typedef struct {
	uint16_t yaw_offset;      // 偏航轴零点偏移
	uint16_t pitch_offset;    // 俯仰轴零点偏移
	fp32 yaw_max_angle;       // 偏航轴最大角度
	fp32 yaw_min_angle;       // 偏航轴最小角度
	fp32 pitch_max_angle;     // 俯仰轴最大角度
	fp32 pitch_min_angle;     // 俯仰轴最小角度
} gimbal_cali_t;

//陀螺仪、加速度计、磁力计
typedef struct {
    fp32 offset[3]; // x,y,z轴偏移
    fp32 scale[3];  // x,y,z轴比例
} imu_cali_t;


extern void cali_param_init(void);
extern int8_t get_control_temperature(void);
extern void get_flash_latitude(float *latitude);

#endif
