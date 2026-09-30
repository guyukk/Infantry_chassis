#include "INA226.h"
#include "i2c.h"


/**
  * @brief          INA226写寄存器
  * @param[in]      reg_addr，寄存器地址
  * @param[in]      data，需要写入的数据
  * @retval         none
  */
static void INA226_WriteReg(uint8_t reg_addr, uint16_t data)
{
	uint8_t buffer[3] = {reg_addr, ((data >> 8) & 0xFF), (data & 0xFF)};
	HAL_I2C_Master_Transmit(&hi2c2, INA226_ADDR_W, buffer, 3, HAL_MAX_DELAY);
}

/**
  * @brief          INA226读寄存器
  * @param[in]      reg_addr，寄存器地址
  * @param[in]      data，读取到的数据的指针
  * @retval         none
  */
static void INA226_ReadReg(uint8_t reg_addr, uint16_t *p_data)
{
	uint8_t data_receive[2];
	HAL_I2C_Master_Transmit(&hi2c2, INA226_ADDR_W, &reg_addr, 1, HAL_MAX_DELAY);
	HAL_I2C_Master_Receive(&hi2c2, INA226_ADDR_R, data_receive, 2, HAL_MAX_DELAY);
	*p_data = (data_receive[0] << 8) | data_receive[1];
}

/**
  * @brief          INA226初始化，配置寄存器保持默认即可
  * @param[in]      none
  * @retval         none
  */
void INA226_Init(void)
{
	INA226_WriteReg(INA226_REG_CONFIG, 0x4327);
	INA226_WriteReg(INA226_REG_CALIBRATION, (uint16_t)INA226_CALI_VALUE);
	HAL_Delay(10);
}

uint8_t get_default_data = 0;
/**
  * @brief          INA226获取 采样电阻电压 总线电压 总线电流 功率	制造商ID 产品ID
  * @param[in]      p_Data，功率芯片返回数据指针
  * @param[in]      p_ID，功率芯片返回ID指针
  * @retval         none
  */
void INA226_GetData(float *p_Data, uint16_t *p_default_data)
{
	uint16_t data_receive[8];
	//采样电阻电压
	INA226_ReadReg(INA226_REG_SHUNT_V, (data_receive	+ 0));
	p_Data[0] = ((float)data_receive[0]) * 2.5f;		//单位：uV
	//总线电压
	INA226_ReadReg(INA226_REG_BUS_V, (data_receive	+	1));
	p_Data[1] = ((float)data_receive[1]) * 1.25e-3f;		//单位：V
	//总线电流
	INA226_ReadReg(INA226_REG_CURRENT, (data_receive	+	2));
	p_Data[2] = (float)data_receive[2] * INA226_Current_LSB;		//单位：A
	//功率
	INA226_ReadReg(INA226_REG_POWER, (data_receive	+	3));
	p_Data[3] = ((float)data_receive[3]) * INA226_Power_LSB;		//单位：W
	
	if(!get_default_data)
	{
		//制造商ID
		INA226_ReadReg(INA226_REG_MANU_ID, (data_receive	+	4));
		p_default_data[0] = data_receive[4];
		//产品ID
		INA226_ReadReg(INA226_REG_DIE_ID, (data_receive	+	5));
		p_default_data[1] = data_receive[5];
		//配置信息
		INA226_ReadReg(INA226_REG_CONFIG, (data_receive	+	6));
		p_default_data[2] = data_receive[6];
		//校准信息
		INA226_ReadReg(INA226_REG_CALIBRATION, (data_receive	+	7));
		p_default_data[3] = data_receive[7];
		
		get_default_data = 1;
	}
}
