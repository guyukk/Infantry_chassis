/**
	* 图传模块的英文名称叫 Video_Transmission_Module
	*/

#ifndef __VIDEO_TRANSMISSION_MODULE_H
#define __VIDEO_TRANSMISSION_MODULE_H
#include "main.h"

// 遥控数据帧长度(协议固定21字节)
#define REMOTE_FRAME_LEN    21
// 帧头固定值(协议定义)
#define REMOTE_HEAD1        0xA9
#define REMOTE_HEAD2        0x53
// 摇杆/拨轮通道值范围
#define CHANNEL_MIN         364
#define CHANNEL_MID         1024
#define CHANNEL_MAX         1684
// 按键状态宏定义
#define KEY_RELEASE         0
#define KEY_PRESS           1
// 档位切换开关状态
#define GEAR_C              0
#define GEAR_N              1
#define GEAR_S              2

#pragma pack(1)  // 结构体按1字节对齐，避免内存填充
// 遥控数据解包结构体，完全对应协议帧结构
typedef struct
{
    // 帧头(2字节固定值)
    uint8_t head1;          // 偏移0，长度8bit，固定0xA9
    uint8_t head2;          // 偏移8，长度8bit，固定0x53
    
    // 摇杆/拨轮通道(4个11bit通道)
    uint16_t ch0 : 11;      // 偏移16，右摇杆水平
    uint16_t ch1 : 11;      // 偏移27，右摇杆垂直
    uint16_t ch2 : 11;      // 偏移38，左摇杆垂直
    uint16_t ch3 : 11;      // 偏移49，左摇杆水平
    
    // 档位切换开关(2bit)
    uint8_t  gear_switch : 2;  // 偏移60，0:C 1:N 2:S
    
    // 自定义按键(3个1bit按键)
    uint8_t  pause_key : 1;    // 偏移62，暂停键
    uint8_t  custom_l_key : 1;  // 偏移63，自定义左键
    uint8_t  custom_r_key : 1;  // 偏移64，自定义右键
    
    // 拨轮(11bit)
    uint16_t wheel : 11;       // 偏移65，拨轮值
    
    // 扳机键(1bit)
    uint8_t  trigger_key : 1;  // 偏移76，扳机键
    
    // 鼠标三轴(3个16bit有符号数，小端序)
    int16_t  mouse_x;          // 偏移80，鼠标X轴速度
    int16_t  mouse_y;          // 偏移96，鼠标Y轴速度
    int16_t  mouse_z;          // 偏移112，鼠标滚轮速度
    
    // 鼠标按键(3个2bit按键)
    uint8_t  mouse_l_key : 2;  // 偏移128，鼠标左键
    uint8_t  mouse_r_key : 2;  // 偏移130，鼠标右键
    uint8_t  mouse_m_key : 2;  // 偏移132，鼠标中键
    
    // 键盘按键(16bit，每bit对应一个按键，小端序)
    uint16_t keyboard_key;     // 偏移136，键盘按键位图
    
    // 预留/CRC(协议中后续字节，仅占位不校验)
    uint16_t crc;              // 偏移152，CRC校验位
} VTM_Data_t;
#pragma pack()

// 全局遥控数据结构体(外部可调用)
extern VTM_Data_t vtm_data;

// 函数声明
uint8_t vtm_data_unpack(uint8_t *buf, uint16_t buf_len, VTM_Data_t *data);
void vtm_data_init(void);

#endif
