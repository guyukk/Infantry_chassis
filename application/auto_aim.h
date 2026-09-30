#ifndef _AUTO_AIM_H
#define _AUTO_AIM_H
#include "main.h"
#include "gimbal_task.h"
#include "gimbal_behaviour.h"
#include "bsp_usart.h"
#include "message_task.h"
#include "shoot.h"
#define PI_RAD PI/180
//这里是将视觉的单位坐标【0，1】分到360度，然后转换成弧度，再除以频率防止回环延迟导致过充，就是1*180*PI/180/100
//#define AUTO_AIM_PPL 1.0f/15.0f
#define AUTO_AIM_PPL 1.0f/5.0f
#define yaw_before 0.2f
#define pitch_before 0.2f

#define noise 0.15f
//中断一次的需要的时间
#define IRQHandler_TIME 0.000014f 
//打弹延时
#define DELAY_TIME -1.0f/CONTINUE_TRIGGER_SPEED
//包的大小
#define PACKET_SIZE 16

#define FILTER_WINDOW_SIZE 10  // 滤波窗口大小，可根据需要调整

#define FILTER_ORDER 5  // 滤波器阶数
#define SAMPLE_RATE 300  // 采样率 (Hz)
#define CUTOFF_FREQ 5  // 截止频率 (Hz)

// 常量定义，与打包代码保持一致
#define START_BYTE 's'
#define END_BYTE 'e'
#define SCALE_YAW_PITCH (100.0f / (32768 - 1))  // 与打包代码一致
#define SCALE_DISTANCE (1000.0f / (32768 - 1))  // 与打包代码一致

typedef struct {
    int16_t yaw;          // 偏航角
    int16_t pitch;        // 俯仰角
    int16_t distance;     // 距离
    int16_t shoot_delay; // 射击延迟
} ReceivedData;

extern fp32 yaw_set;
extern fp32 yaw_target;

extern void auto_aim(uint8_t* data_send_from_pc);
extern void send_to_minipc(void);
#endif
