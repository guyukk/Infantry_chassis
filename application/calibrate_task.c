/*****************************(C) COPYRIGHT 2019 DJI****************************
  * @file       calibrate_task.c
  * @brief      校准设备，包括云台,陀螺仪,加速度计,磁力计,底盘.
	*							云台校准是主要计算零漂和最大最小相对角度.
	*							加速度计和磁力计校准还没有实现,因为加速度计还没有必要去校准.
	*							磁力计还没有用.
	*							底盘校准是使M3508进入快速设置ID模式.
	*
  * @note       第一步：遥控器两个开关都打到下
	*							
	*							第二步：两个摇杆打成\../形状并保持2秒
	*							
	*							第三步：摇杆打成./\.: 开始陀螺仪校准
	*							        
	*							        摇杆打成'\/': 开始云台校准
	*							        
	*							        摇杆打成/''\: 开始底盘校准
	*
  *             数据在flash中，包括校准数据和名字 name[3] 和 校准标志位 cali_flag
  *             例如head_cali有八个字节,但它需要12字节在flash,如果它从0x080A0000开始
  *             0x080A0000-0x080A0007: head_cali数据
  *             0x080A0008: 名字name[0]
  *             0x080A0009: 名字name[1]
  *             0x080A000A: 名字name[2]
  *             0x080A000B: 校准标志位 cali_flag,当校准标志位为0x55,意味着head_cali已经校准了
	*
  *             *添加新设备*
  *             1.在.h文件的cali_id_e枚举中的//add more...下面添加新设备的名字
	*
  *             2.在.h文件中添加新的数据结构体(可命名为xxx_cali_t),长度必须是4字节倍数
	*
  *             3.在.c文件中的 "FLASH_WRITE_BUF_LENGHT"里添加"sizeof(xxx_cali_t)"
	*							
	*							4.在.c文件中添加新的校准钩子函数bool_t cali_xxx_hook(uint32_t *cali, bool_t cmd), 记得在上方函数声明区中声明一下
	*							
	*							5.在.c文件中的"cali_name[CALI_LIST_LENGHT][3]"添加新名字
  *             
	*							6.在.c文件中变量声明区中添加变量 xxx_cali_t xxx_cail
	*
	*							7.在cali_sensor_buf[CALI_LIST_LENGHT]添加校准数据地址
	*
	*							8.在cali_sensor_size[CALI_LIST_LENGHT]添加校准数据的长度
	*
	*							9.在void *cali_hook_fun[CALI_LIST_LENGHT]末尾添加函数指针
	*
	*							10.在RC_cmd_to_calibrate()函数里添加相关判断和执行语句(关键)
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

#include "calibrate_task.h"
#include "string.h"
#include "cmsis_os.h"
#include "iwdg.h"
#include "bsp_adc.h"
#include "bsp_buzzer.h"
#include "bsp_flash.h"

#include "can_receive.h"
#include "remote_control.h"
#include "INS_task.h"
#include "gimbal_task.h"


/* 函数声明区 */
static void RC_cmd_to_calibrate(void);
static void cali_data_read(void);
static void cali_data_write(void);
static bool_t cali_head_hook(uint32_t *cali, bool_t cmd);
static bool_t cali_gyro_hook(uint32_t *cali, bool_t cmd);
static bool_t cali_gimbal_hook(uint32_t *cali, bool_t cmd);


#if INCLUDE_uxTaskGetStackHighWaterMark
	uint32_t calibrate_task_stack;
#endif

/* 变量声明区 */
static const RC_ctrl_t *calibrate_RC;   //遥控器结构体指针常量
static head_cali_t     head_cali;       //头部校准数据结构体变量(成员包括系统ID、固件版本、温度、纬度)
static gimbal_cali_t   gimbal_cali;     //云台校准数据结构体变量
static imu_cali_t      accel_cali;      //加速度计校准数据结构体变量
static imu_cali_t      gyro_cali;       //陀螺仪校准数据结构体变量
static imu_cali_t      mag_cali;        //磁力计校准数据结构体变量

/* 每个设备在Flash中占用的总空间为: 校准数据长度 + 4字节(3字节名称 + 1字节标志位) */
#define FLASH_WRITE_BUF_LENGHT  (sizeof(head_cali_t) + sizeof(gimbal_cali_t) + sizeof(imu_cali_t) * 3  + CALI_LIST_LENGHT * 4)

//临时存储所有待写入 Flash 的校准数据(包括设备校准数据、设备名称、校准标志位等)
static uint8_t flash_write_buf[FLASH_WRITE_BUF_LENGHT];

//校准设备管理数组，用于管理所有需要校准的设备
cali_sensor_t cali_sensor[CALI_LIST_LENGHT];

//设备名称对照，定义每个设备的缩写名称(3字节固定长度)，用于 Flash 存储时标识设备
static const uint8_t cali_name[CALI_LIST_LENGHT][3] = {"HD", "GM", "GYR", "ACC", "MAG"};

//校准数据地址数组，用于存储每个设备校准数据的地址
static uint32_t *cali_data_addr_buf[CALI_LIST_LENGHT] = {
	(uint32_t *)&head_cali, (uint32_t *)&gimbal_cali,
	(uint32_t *)&gyro_cali, (uint32_t *)&accel_cali,
	(uint32_t *)&mag_cali};

//存储每个校准数据结构的长度，单位：4字节长度
//以head_cali_t为例子，其长度为：8字节/4字节 = 2
static uint8_t cali_sensor_size[CALI_LIST_LENGHT] = {
	sizeof(head_cali_t) / 4, sizeof(gimbal_cali_t) / 4,
	sizeof(imu_cali_t) / 4, sizeof(imu_cali_t) / 4, sizeof(imu_cali_t) / 4};

//这是一个函数指针数组，每个元素指向对应设备的校准函数
//使用 void * 是为了兼容不同类型的函数指针
void *cali_hook_fun[CALI_LIST_LENGHT] = {cali_head_hook, cali_gimbal_hook, cali_gyro_hook, NULL, NULL,};


/**
  * @brief          校准参数初始化
  * @param[in]      none
  * @retval         none
  */
void cali_param_init(void)
{
	uint8_t i = 0;

	for (i = 0; i < CALI_LIST_LENGHT; i++)
	{
		cali_sensor[i].flash_len = cali_sensor_size[i];
		cali_sensor[i].flash_buf = cali_data_addr_buf[i];
		cali_sensor[i].cali_hook = (bool_t(*)(uint32_t *, bool_t))cali_hook_fun[i];
	}

	cali_data_read();
	
	//在系统启动时, 若设备之前被校准过, 则加载之前的校准数据进行初始化, 确保设备能正常工作
	for (i = 0; i < CALI_LIST_LENGHT; i++)
	{
		if (cali_sensor[i].cali_done == CALIED_FLAG)
		{
			if (cali_sensor[i].cali_hook != NULL)
			{
				cali_sensor[i].cali_hook(cali_data_addr_buf[i], CALI_FUNC_CMD_INIT);
			}
		}
	}
}

/**
  * @brief          获取imu控制温度, 单位℃
  * @param[in]      none
  * @retval         imu控制温度
  */
int8_t get_control_temperature(void)
{
	return head_cali.temperature;
}

/**
  * @brief          获取纬度,默认22.0f
  * @param[out]     latitude:fp32指针 
  * @retval         none
  */
void get_flash_latitude(float *latitude)
{
	if (latitude == NULL)
	{
		return;
	}
	if (cali_sensor[CALI_HEAD].cali_done == CALIED_FLAG)
	{
		*latitude = head_cali.latitude;
	}
	else
	{
		*latitude = 22.0f;
	}
}

/**
  * @brief          使用遥控器开始校准，例如陀螺仪、云台、底盘
  * @param[in]      none
  * @retval         none
  */
static void RC_cmd_to_calibrate(void)
{
	static const uint8_t BEGIN_FLAG   = 1;
	static const uint8_t GIMBAL_FLAG  = 2;
	static const uint8_t GYRO_FLAG    = 3;
	
	static uint32_t calibrate_systemTick;		//用于记录校准过程中的时间戳(单位：系统滴答，通常是毫秒)
	static uint32_t rc_cmd_systemTick = 0;	//记录校准开始的系统时间（用于超时判断）
	static uint16_t buzzer_time       = 0;	//蜂鸣器鸣叫计时（用于提示校准状态）
	static uint16_t rc_cmd_time       = 0;	//记录摇杆保持特定位置的时间
	static uint8_t  rc_action_flag    = 0;	//校准状态机，标记当前校准阶段

	//如果已经在校准，就返回
	for (uint8_t i = 0; i < CALI_LIST_LENGHT; i++)
	{
		if (cali_sensor[i].cali_cmd)
		{
			buzzer_time = 0;
			rc_cmd_time = 0;
			rc_action_flag = 0;
			
			return;
		}
	}
	
	if (rc_action_flag == 0 && rc_cmd_time > RC_CMD_LONG_TIME)
	{
		//启动校准
		rc_cmd_systemTick = xTaskGetTickCount();	//记录开始时间
		rc_action_flag = BEGIN_FLAG;							//进入准备状态
		rc_cmd_time = 0;													//重置遥控器位置计时
	}
	else if (rc_action_flag == GIMBAL_FLAG && rc_cmd_time > RC_CMD_LONG_TIME)
	{
		//云台校准
		rc_action_flag = 0;
		rc_cmd_time = 0;
		cali_sensor[CALI_GIMBAL].cali_cmd = 1;
		cali_buzzer_off();
	}
	else if (rc_action_flag == GYRO_FLAG && rc_cmd_time > RC_CMD_LONG_TIME)
	{
		//陀螺仪校准
		rc_action_flag = 0;
		rc_cmd_time = 0;
		cali_sensor[CALI_GYRO].cali_cmd = 1;
		//更新IMU温度
		head_cali.temperature = (int8_t)(cali_get_mcu_temperature()) + 10;
		//温度限幅
		if (head_cali.temperature > (int8_t)(GYRO_CONST_MAX_TEMP))
		{
			head_cali.temperature = (int8_t)(GYRO_CONST_MAX_TEMP);
		}
		cali_buzzer_off();
	}
	
	/* 遥控器位置判断 */
	if (calibrate_RC->rc.ch[0] < -RC_CALI_VALUE_HOLE && calibrate_RC->rc.ch[1] < -RC_CALI_VALUE_HOLE && calibrate_RC->rc.ch[2] > RC_CALI_VALUE_HOLE && calibrate_RC->rc.ch[3] < -RC_CALI_VALUE_HOLE && switch_is_mid(calibrate_RC->rc.s[0]) && switch_is_mid(calibrate_RC->rc.s[1]) && rc_action_flag == 0)
	{
		//两个摇杆打成 \../,保持2s
		rc_cmd_time++;
	}
	else if (calibrate_RC->rc.ch[0] > RC_CALI_VALUE_HOLE && calibrate_RC->rc.ch[1] > RC_CALI_VALUE_HOLE && calibrate_RC->rc.ch[2] < -RC_CALI_VALUE_HOLE && calibrate_RC->rc.ch[3] > RC_CALI_VALUE_HOLE && switch_is_mid(calibrate_RC->rc.s[0]) && switch_is_mid(calibrate_RC->rc.s[1]) && rc_action_flag != 0)
	{
		//两个摇杆打成'\/',保持2s
		rc_cmd_time++;
		rc_action_flag = GIMBAL_FLAG;
	}
	else if (calibrate_RC->rc.ch[0] > RC_CALI_VALUE_HOLE && calibrate_RC->rc.ch[1] < -RC_CALI_VALUE_HOLE && calibrate_RC->rc.ch[2] < -RC_CALI_VALUE_HOLE && calibrate_RC->rc.ch[3] < -RC_CALI_VALUE_HOLE && switch_is_mid(calibrate_RC->rc.s[0]) && switch_is_mid(calibrate_RC->rc.s[1]) && rc_action_flag != 0)
	{
		//两个摇杆打成./\.,保持2s
		rc_cmd_time++;
		rc_action_flag = GYRO_FLAG;
	}
	else
	{
		//无操作则重置遥控器控制时间
		rc_cmd_time = 0;
	}
	
	//记录校准系统时间
	calibrate_systemTick = xTaskGetTickCount();

	if (calibrate_systemTick - rc_cmd_systemTick > CALIBRATE_END_TIME)
	{
		//若从收到遥控器指令开始(rc_cmd_systemTick)超过20秒未完成校准, 则强制终止这次校准
		rc_action_flag = 0;
		return;
	}
	else if (calibrate_systemTick - rc_cmd_systemTick > RC_CALI_BUZZER_MIDDLE_TIME && rc_cmd_systemTick != 0 && rc_action_flag != 0)
	{
		rc_cali_buzzer_middle_on();
	}
	else if (calibrate_systemTick - rc_cmd_systemTick > 0 && rc_cmd_systemTick != 0 && rc_action_flag != 0)
	{
		rc_cali_buzzer_start_on();
	}

	if (rc_action_flag != 0)
	{
		buzzer_time++;
	}
    
	if (buzzer_time > RCCALI_BUZZER_CYCLE_TIME && rc_action_flag != 0)
	{
		buzzer_time = 0;
	}
	
	if (buzzer_time > RC_CALI_BUZZER_PAUSE_TIME && rc_action_flag != 0)
	{
		cali_buzzer_off();
	}
}

/**
  * @brief          从flash读取校准数据
  * @param[in]      none
  * @retval         none
  */
static void cali_data_read(void)
{
	uint8_t flash_read_buf[CALI_SENSOR_HEAD_LEGHT * 4];
	uint16_t offset = 0;
	for (uint8_t i = 0; i < CALI_LIST_LENGHT; i++)
	{
		//从flash中读取数据
		cali_flash_read(FLASH_USER_ADDR + offset, cali_sensor[i].flash_buf, cali_sensor[i].flash_len);
		
		offset += cali_sensor[i].flash_len * 4;

		//从flash中读取设备名称和是否完成校准的标志
		cali_flash_read(FLASH_USER_ADDR + offset, (uint32_t *)flash_read_buf, CALI_SENSOR_HEAD_LEGHT);
		
		cali_sensor[i].name[0] = flash_read_buf[0];
		cali_sensor[i].name[1] = flash_read_buf[1];
		cali_sensor[i].name[2] = flash_read_buf[2];
		cali_sensor[i].cali_done = flash_read_buf[3];
		
		offset += CALI_SENSOR_HEAD_LEGHT * 4;

		if (cali_sensor[i].cali_done != CALIED_FLAG && cali_sensor[i].cali_hook != NULL)
		{
				cali_sensor[i].cali_cmd = 1;
		}
	}
}

/**
  * @brief          往flash写入校准数据
  * @param[in]      none
  * @retval         none
  */
static void cali_data_write(void)
{
	uint8_t i = 0;
	uint16_t offset = 0;
	
	for (i = 0; i < CALI_LIST_LENGHT; i++)
	{
		//复制 设备校准值
		memcpy((void *)(flash_write_buf + offset), (void *)cali_sensor[i].flash_buf, cali_sensor[i].flash_len * 4);
		offset += cali_sensor[i].flash_len * 4;

		//复制 设备名称 和 设备是否完成校准的标志
		memcpy((void *)(flash_write_buf + offset), (void *)cali_sensor[i].name, CALI_SENSOR_HEAD_LEGHT * 4);
		offset += CALI_SENSOR_HEAD_LEGHT * 4;
	}
	//擦除flash中相关地址
	cali_flash_erase(FLASH_USER_ADDR, 1);
	//写入数据
	cali_flash_write(FLASH_USER_ADDR, (uint32_t *)flash_write_buf, (FLASH_WRITE_BUF_LENGHT + 3) / 4);
}

/**
  * @brief          校准任务，由main函数创建
  * @param[in]      pvParameters: 空
  * @retval         none
  */
void calibrate_task(void const *pvParameters)
{
	//获取遥控器指针
	calibrate_RC = get_remote_ctrl_point_cali();

	while (1)
	{
		//使用遥控器开始校准
		RC_cmd_to_calibrate();
	
		for (uint8_t i = 0; i < CALI_LIST_LENGHT; i++) {
			if (cali_sensor[i].cali_cmd) {
				if (cali_sensor[i].cali_hook != NULL) {
					if (cali_sensor[i].cali_hook(cali_data_addr_buf[i], CALI_FUNC_CMD_ON)) {
						cali_sensor[i].name[0] = cali_name[i][0];	// 校准完成
						cali_sensor[i].name[1] = cali_name[i][1];
						cali_sensor[i].name[2] = cali_name[i][2];
						cali_sensor[i].cali_done = CALIED_FLAG; // 设置校准标志
						cali_sensor[i].cali_cmd = 0;
						cali_data_write(); // 写入Flash
					}
				}
			}
		}
		
		osDelay(CALIBRATE_CONTROL_TIME);
		
		#if INCLUDE_uxTaskGetStackHighWaterMark
			calibrate_task_stack = uxTaskGetStackHighWaterMark(NULL);
		#endif
	}
}

/**
  * @brief          "head"设备校准
  * @param[in][out] cali:指针指向head数据,当cmd为CALI_FUNC_CMD_INIT, 参数是输入,CALI_FUNC_CMD_ON,参数是输出
  * @param[in]      cmd: 
                    CALI_FUNC_CMD_INIT: 代表用校准数据初始化原始数据
                    CALI_FUNC_CMD_ON: 代表需要校准
  * @retval         0:校准任务还没有完
                    1:校准任务已经完成
  */
static bool_t cali_head_hook(uint32_t *cali, bool_t cmd)
{
	head_cali_t *local_cali_t = (head_cali_t *)cali;
	if (cmd == CALI_FUNC_CMD_INIT)
	{
		memcpy(&head_cali, local_cali_t, sizeof(head_cali_t));
		return 1;
	}
	local_cali_t->self_id = SELF_ID; // 自身ID
	local_cali_t->temperature = (int8_t)(cali_get_mcu_temperature()) + 10; // IMU控制温度(基于MCU温度+10°C)
	//IMU温度限幅
	if (local_cali_t->temperature > (int8_t)(GYRO_CONST_MAX_TEMP))
	{
		local_cali_t->temperature = (int8_t)(GYRO_CONST_MAX_TEMP);
	}
	local_cali_t->firmware_version = FIRMWARE_VERSION; // 固件版本
	local_cali_t->latitude = 22.0f; // 默认纬度(22.0f，深圳纬度)	

	return 1;
}

/**
  * @brief          陀螺仪设备校准
  * @param[in][out] cali:指针指向陀螺仪数据,当cmd为CALI_FUNC_CMD_INIT, 参数是输入,CALI_FUNC_CMD_ON,参数是输出
  * @param[in]      cmd: 
                    CALI_FUNC_CMD_INIT: 代表用校准数据初始化原始数据
                    CALI_FUNC_CMD_ON: 代表需要校准
  * @retval         0:校准任务还没有完
                    1:校准任务已经完成
  */
static bool_t cali_gyro_hook(uint32_t *cali, bool_t cmd)
{
	imu_cali_t *local_cali_t = (imu_cali_t *)cali;
	if (cmd == CALI_FUNC_CMD_INIT)
	{
		gyro_set_cali(local_cali_t->scale, local_cali_t->offset);
		return 0;
	}
	else if (cmd == CALI_FUNC_CMD_ON)
	{
		static uint16_t count_time = 0;
		gyro_cali_fun(local_cali_t->scale, local_cali_t->offset, &count_time);
		if (count_time > GYRO_CALIBRATE_TIME)
		{
			count_time = 0;
			cali_buzzer_off();
			gyro_cali_enable_control();
			return 1;
		}
		else
		{
			gyro_cali_disable_control(); //disable the remote control to make robot no move
			imu_start_buzzer();
			return 0;
		}
	}

	return 0;
}

/**
  * @brief          云台设备校准
  * @param[in][out] cali:指针指向云台数据,当cmd为CALI_FUNC_CMD_INIT, 参数是输入,CALI_FUNC_CMD_ON,参数是输出
  * @param[in]      cmd: 
                    CALI_FUNC_CMD_INIT: 代表用校准数据初始化原始数据
                    CALI_FUNC_CMD_ON: 代表需要校准
  * @retval         0:校准任务还没有完
                    1:校准任务已经完成
  */
static bool_t cali_gimbal_hook(uint32_t *cali, bool_t cmd)
{
	gimbal_cali_t *local_cali_t = (gimbal_cali_t *)cali;
	if (cmd == CALI_FUNC_CMD_INIT)
	{
		set_cali_gimbal_hook(local_cali_t->yaw_offset, local_cali_t->pitch_offset,
												 local_cali_t->yaw_max_angle, local_cali_t->yaw_min_angle,
												 local_cali_t->pitch_max_angle, local_cali_t->pitch_min_angle);
		return 0;
	}
	else if (cmd == CALI_FUNC_CMD_ON)
	{
		if (cmd_cali_gimbal_hook(&local_cali_t->yaw_offset, &local_cali_t->pitch_offset,
														 &local_cali_t->yaw_max_angle, &local_cali_t->yaw_min_angle,
														 &local_cali_t->pitch_max_angle, &local_cali_t->pitch_min_angle))
		{
			cali_buzzer_off();
			return 1;
		}
		else
		{
			gimbal_start_buzzer();
			return 0;
		}
	}
	
	return 0;
}
