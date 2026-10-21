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


/*电机数据结构体*/
typedef struct
{
	uint16_t	ecd;			//当前角度
	int16_t		speed_rpm;      //当前转速
	int16_t		given_current;  //当前实际扭矩电流
	uint8_t		temperate;      //当前温度
	int16_t		last_ecd;       //上一次的角度
	
} MOTOR_MEASURE_t;

typedef struct
{
	uint8_t last_status;
	uint32_t last_error;
	uint32_t last_mailbox;
	uint32_t success_count;
	uint32_t failure_count;
} CAN_6020_tx_diag_t;

extern volatile CAN_6020_tx_diag_t can_6020_tx_diag;
extern volatile uint32_t can_6020_last_feedback_ms[4];
extern uint8_t CAN_6020_feedback_is_fresh(uint32_t timeout_ms);


/* 本工程固件为底盘板（chassis_board）：CAN1 收发 3508，CAN2 收发 6020。 */
#define CHASSIS_3508_CAN hcan1  //2p
#define CHASSIS_6020_CAN hcan2  //4p


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

extern void CAN_cmd_CHAS_3508(int16_t motor1, int16_t motor2, int16_t motor3, int16_t motor4);
extern void CAN_cmd_CHAS_6020(int16_t motor5, int16_t motor6, int16_t motor7, int16_t motor8);
extern MOTOR_MEASURE_t *get_chassis_motor_measure_point(uint8_t i);


#endif
