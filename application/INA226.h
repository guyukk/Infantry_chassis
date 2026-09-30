#ifndef __INA226_H
#define __INA226_H

#include "main.h"

//从设备地址
#define INA226_ADDR				0x40
#define INA226_ADDR_W			0x80	//0x40 << 1) | 0x00
#define INA226_ADDR_R			0x81	//0x40 << 1) | 0x01

//寄存器地址
#define INA226_REG_CONFIG      		0x00	//Configuration Register
#define INA226_REG_SHUNT_V     		0x01  //Shunt Voltage Register
#define INA226_REG_BUS_V       		0x02  //Bus Voltage Register
#define INA226_REG_POWER       		0x03  //Power Register
#define INA226_REG_CURRENT     		0x04  //Current Register
#define INA226_REG_CALIBRATION 		0x05  //Calibration Register
#define INA226_REG_MASK_ENABLE 		0x06  //Mask/Enable Register
#define INA226_REG_ALERT_LIMIT 		0x07  //Alert Limit Register
#define INA226_REG_MANU_ID				0xFE  //Manufacturer ID Register
#define INA226_REG_DIE_ID      		0xFF  //Die ID Register

#define INA226_R_Shunt						0.1f
#define INA226_Current_LSB				30e-5f
#define INA226_CALI_VALUE					(0.00512f / (INA226_Current_LSB * INA226_R_Shunt))
#define INA226_Power_LSB					(25.0f * INA226_Current_LSB)


void INA226_Init(void);
void INA226_GetData(float *p_Data, uint16_t *p_default_data);


#endif
