/**
  ****************************(C) COPYRIGHT 2019 DJI****************************
  * @file       can_receive.c/h
  * @brief      这里是CAN中断接收函数，接收电机数据,CAN发送函数发送电机电流控制电机.
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

#ifndef CAN_RECEIVE_H
#define CAN_RECEIVE_H

#include "main.h"
#include "struct_typedef.h"
#include "remote_control.h"
#include "robot_param.h"


/*电机数据结构体*/
typedef struct
{
	uint16_t	ecd;			//当前角度
	int16_t		speed_rpm;      //当前转速
	int16_t		given_current;  //当前实际扭矩电流
	uint8_t		temperate;      //当前温度
	int16_t		last_ecd;       //上一次的角度
	
} MOTOR_MEASURE_t;


/* 本工程固件为底盘板（chassis_board）：CAN1 收发 3508，CAN2 收发 6020。 */
#define CHASSIS_3508_CAN hcan1  //2p
#if CHASSIS_6020_ONLY_TEST
#define CHASSIS_6020_CAN hcan1  // 仅台架：6020 接在已验证能接收反馈的 CAN1
#else
#define CHASSIS_6020_CAN hcan2  // 正常底盘：6020 使用 CAN2
#endif


/* CAN线发送与接收的ID */
//CAN1操控和接收 3508, CAN2操控和接收 6020
typedef enum
{
	CAN1_CHAS_3508_ID = 0x200,
	CAN1_3508_M1_ID = 0x201,
	CAN1_3508_M2_ID = 0x202,
	CAN1_3508_M3_ID = 0x203,
	CAN1_3508_M4_ID = 0x204,
	
	CAN2_CHAS_6020_ID = 0x1FE,
	CAN2_6020_M1_ID = 0x205,
	CAN2_6020_M2_ID = 0x206,
	CAN2_6020_M3_ID = 0x207,
	CAN2_6020_M4_ID = 0x208,

} CHAS_ID_e;

/* Watch: 提交给 CAN2 的最近一次四路命令及 HAL 状态。HAL_OK 仅表示进入发送邮箱。 */
typedef struct
{
	uint8_t rc_blocked;
	uint8_t hal_status;
	uint16_t std_id;
	int16_t command[4];
	uint32_t submit_ok_count;
	uint32_t submit_error_count;
	uint32_t can_error_code;
	uint32_t can_esr;
	uint32_t can_tsr;
} CHAS_6020_TX_DEBUG_t;

extern volatile CHAS_6020_TX_DEBUG_t chassis_6020_tx_debug;

/* Watch: 0x205-0x208 每路实际收到的反馈帧数和最近接收时刻。 */
typedef struct
{
	uint32_t count[4];
	uint32_t last_tick_ms[4];
	uint8_t last_bus[4];  // 1: CAN1, 2: CAN2
	uint32_t read_error_count;
} CHAS_6020_RX_DEBUG_t;

extern volatile CHAS_6020_RX_DEBUG_t chassis_6020_rx_debug;

extern void CAN_cmd_CHAS_3508(int16_t motor1, int16_t motor2, int16_t motor3, int16_t motor4);
extern void CAN_cmd_CHAS_6020(int16_t motor5, int16_t motor6, int16_t motor7, int16_t motor8);
extern MOTOR_MEASURE_t *get_chassis_motor_measure_point(uint8_t i);


#endif
