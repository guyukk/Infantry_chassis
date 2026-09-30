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

//当imu在校准,蜂鸣器的设置分频和强度
#define imu_start_buzzer()     buzzer_on(95, 10000)    
//当云台在校准,蜂鸣器的设置分频和强度
#define gimbal_start_buzzer()  buzzer_on(31, 19999)
//关闭蜂鸣器
#define cali_buzzer_off()      buzzer_off()

//获取stm32片内温度，计算imu的控制温度
#define cali_get_mcu_temperature()	get_temprate()      

#define cali_flash_read(address, buf, len) 	flash_read((address), (buf), (len))										//读取函数
#define cali_flash_write(address, buf, len) flash_write_single_address((address), (buf), (len))   //写入函数
#define cali_flash_erase(address, page_num) flash_erase_address((address), (page_num))            //擦除函数


#define get_remote_ctrl_point_cali()        get_remote_control_point()		//获取遥控器指针
#define gyro_cali_disable_control()         RC_unable()                 	//当imu在校准时候,失能遥控器
#define gyro_cali_enable_control()          RC_restart(SBUS_RX_BUF_NUM)		//使能遥控器

//计算陀螺仪零漂
#define gyro_cali_fun(cali_scale, cali_offset, time_count)  INS_cali_gyro((cali_scale), (cali_offset), (time_count))
//设置在INS task内的陀螺仪零漂
#define gyro_set_cali(cali_scale, cali_offset)              INS_set_cali_gyro((cali_scale), (cali_offset))

#define FLASH_USER_ADDR         ADDR_FLASH_SECTOR_9 //保存的flash页地址

/* 比较重要的常量 */
#define GYRO_CONST_MAX_TEMP   			45.0f  		//最大陀螺仪控制温度
#define GYRO_CALIBRATE_TIME   			20000  		//陀螺仪校准时间
#define CALIBRATE_END_TIME    			20000			//校准超时时间
#define RC_CALI_BUZZER_MIDDLE_TIME  10000			//校准时间只剩十秒时, 蜂鸣器高频鸣叫
#define RC_CALI_BUZZER_START_TIME   		0			//当开始校准的时候, 蜂鸣器低频鸣叫

#define RCCALI_BUZZER_CYCLE_TIME    400
#define RC_CALI_BUZZER_PAUSE_TIME   200
#define RC_CALI_VALUE_HOLE          600

#define CALI_FUNC_CMD_ON        1                   //设置校准
#define CALI_FUNC_CMD_INIT      0                   //已经校准过，设置校准值

#define CALIBRATE_CONTROL_TIME  1                   //1ms系统延时

#define CALI_SENSOR_HEAD_LEGHT  1

#define SELF_ID                 0                   //ID 
#define FIRMWARE_VERSION        12345               //硬件版本
#define CALIED_FLAG             0x55                //已完成校准的标志

#define rc_cali_buzzer_middle_on()  gimbal_start_buzzer()
#define rc_cali_buzzer_start_on()   imu_start_buzzer()
#define RC_CMD_LONG_TIME            2000    


//校准设备名称
typedef enum {
	CALI_HEAD = 0,    // 头部设备
	CALI_GIMBAL = 1,  // 云台
	CALI_GYRO = 2,    // 陀螺仪
	CALI_ACC = 3,     // 加速度计
	CALI_MAG = 4,     // 磁力计
	//add more...
	CALI_LIST_LENGHT, // 设备总数
} cali_id_e;

//校准设备管理结构体
typedef __packed struct {
	uint8_t name[3];     	// 设备名称(3字节)
	uint8_t cali_done;   	// 校准标志(0x55表示已校准)
	uint8_t flash_len :7;	// 数据长度(以4字节为单位)
	uint8_t cali_cmd :1; 	// 校准命令(1表示需要校准)
	uint32_t *flash_buf; 	// 指向校准数据的指针
	bool_t (*cali_hook)(uint32_t *data_ptr, bool_t cmd); // 校准钩子函数
} cali_sensor_t;

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
extern void calibrate_task(void const *pvParameters);


#endif
