
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "calibrate_task.h"
#include "bsp_flash.h"
typedef union { float fp32; uint8_t bytes[4]; } fp32_to_bytes;
typedef uint8_t gimbal_behaviour_e;
typedef uint8_t chassis_behaviour_e;
#pragma pack(1)                      
                       
typedef struct
{
                  
    uint8_t head1;                               
    uint8_t head2;                               
    
                          
    uint16_t ch0 : 11;                    
    uint16_t ch1 : 11;                    
    uint16_t ch2 : 11;                    
    uint16_t ch3 : 11;                    
    
                    
    uint8_t  gear_switch : 2;                      
    
                       
    uint8_t  pause_key : 1;                
    uint8_t  custom_l_key : 1;                
    uint8_t  custom_r_key : 1;                
    
                 
    uint16_t wheel : 11;                   
    
                 
    uint8_t  trigger_key : 1;              
    
                             
    int16_t  mouse_x;                         
    int16_t  mouse_y;                         
    int16_t  mouse_z;                          
    
                      
    uint8_t  mouse_l_key : 2;                
    uint8_t  mouse_r_key : 2;                
    uint8_t  mouse_m_key : 2;                
    
                                  
    uint16_t keyboard_key;                     
    
                              
    uint16_t crc;                              
} F4_VTM_Data_t;
#pragma pack()
#pragma pack(1)                      
                       
typedef struct
{
                  
    uint8_t head1;                               
    uint8_t head2;                               
    
                          
    uint16_t ch0 : 11;                    
    uint16_t ch1 : 11;                    
    uint16_t ch2 : 11;                    
    uint16_t ch3 : 11;                    
    
                    
    uint8_t  gear_switch : 2;                      
    
                       
    uint8_t  pause_key : 1;                
    uint8_t  custom_l_key : 1;                
    uint8_t  custom_r_key : 1;                
    
                 
    uint16_t wheel : 11;                   
    
                 
    uint8_t  trigger_key : 1;              
    
                             
    int16_t  mouse_x;                         
    int16_t  mouse_y;                         
    int16_t  mouse_z;                          
    
                      
    uint8_t  mouse_l_key : 2;                
    uint8_t  mouse_r_key : 2;                
    uint8_t  mouse_m_key : 2;                
    
                                  
    uint16_t keyboard_key;                     
    
                              
    uint16_t crc;                              
} H7_VTM_Data_t;
#pragma pack()
typedef char f4_vtm_size[(sizeof(F4_VTM_Data_t) == 21) ? 1 : -1];
typedef char h7_vtm_size[(sizeof(H7_VTM_Data_t) == 21) ? 1 : -1];
typedef char f4_crc_offset[(offsetof(F4_VTM_Data_t, crc) == 19) ? 1 : -1];
typedef char h7_crc_offset[(offsetof(H7_VTM_Data_t, crc) == 19) ? 1 : -1];
struct { uint8_t before[8]; F4_VTM_Data_t value; uint8_t after[8]; } f4_guard;
struct { uint8_t before[8]; H7_VTM_Data_t value; uint8_t after[8]; } h7_guard;
typedef struct { uint8_t shoot_mode; } shoot_control_t;
shoot_control_t shoot_control;
gimbal_behaviour_e gimbal_behaviour;
chassis_behaviour_e chassis_behaviour_mode;
typedef struct { uint8_t robot_id; } robot_status_t;
robot_status_t robot_status;
struct { struct { float radian_of_ecd; } gimbal_yaw_motor; } gimbal_control;
struct {
  float gimbal_radian_of_ecd;
  uint8_t gimbal_behaviour, gimbal_shoot_mode;
  float chassis_yaw, chassis_pitch, chassis_roll, vx, vy, wz;
} chassis_move;
struct {
  float chassis_yaw, chassis_pitch, chassis_roll;
  float chassis_vx, chassis_vy, chassis_wz;
  uint8_t chassis_mode, robot_id;
} chassis_data;
float GIMBAL_INS_yaw, GIMBAL_INS_pitch, GIMBAL_INS_roll;
float ins_angles[3];
float *get_INS_angle_point(void) { return ins_angles; }
float loop_float_constrain(float input, float min_value, float max_value);
uint8_t frame46[46], frame28[28];
uint16_t frame46_size, frame28_size;
void USART1_Transmit_DMA(uint8_t *bytes, uint16_t size) {
  frame46_size = size; if (size <= 46) memcpy(frame46, bytes, size);
}
void usart1_tx_dma_enable(uint8_t *bytes, uint16_t size) {
  frame28_size = size; if (size <= 28) memcpy(frame28, bytes, size);
}
uint8_t flash_image[120];
float loaded_scale[3], loaded_offset[3];
uint32_t gyro_load_count;
void flash_read(uint32_t address, uint32_t *dest, uint32_t words) {
  memcpy(dest, flash_image + address - ADDR_FLASH_SECTOR_9, words * 4U);
}
void INS_set_cali_gyro(float scale[3], float offset[3]) {
  memcpy(loaded_scale, scale, sizeof(loaded_scale));
  memcpy(loaded_offset, offset, sizeof(loaded_offset));
  ++gyro_load_count;
}
float loop_float_constrain(float Input, float minValue, float maxValue){
    if (maxValue < minValue)
    {
        return Input;
    }

    if (Input > maxValue)
    {
        float len = maxValue - minValue;
        while (Input > maxValue)
        {
            Input -= len;
        }
    }
    else if (Input < minValue)
    {
        float len = maxValue - minValue;
        while (Input < minValue)
        {
            Input += len;
        }
    }
    return Input;
}
/* Boot-time calibration loading for the F407 chassis.
 * Legacy flash layout: HEAD 8+4, GM 20+4, GYR 24+4, ACC 24+4, MAG 24+4.
 * Each trailer contains a three-byte name and a one-byte valid flag.
 * GM remains reserved so the stored gyro record stays at byte offset 36.
 * The original chassis never started a calibration task or wrote these records.
 */

#define CALIED_FLAG 0x55
#define CALI_TRAILER_SIZE 4U
#define CALI_GYRO_OFFSET (sizeof(head_cali_t) + sizeof(gimbal_cali_t) + 2U * CALI_TRAILER_SIZE)

typedef char cali_head_layout_check[(sizeof(head_cali_t) == 8) ? 1 : -1];
typedef char cali_gimbal_layout_check[(sizeof(gimbal_cali_t) == 20) ? 1 : -1];
typedef char cali_imu_layout_check[(sizeof(imu_cali_t) == 24) ? 1 : -1];
typedef char cali_gyro_offset_check[(CALI_GYRO_OFFSET == 36) ? 1 : -1];

static head_cali_t head_cali;
static uint8_t head_cali_done;

void cali_param_init(void)
{
    uint32_t trailer;
    imu_cali_t gyro_cali;

    flash_read(ADDR_FLASH_SECTOR_9, (uint32_t *)&head_cali, sizeof(head_cali) / 4);
    flash_read(ADDR_FLASH_SECTOR_9 + sizeof(head_cali), &trailer, 1);
    head_cali_done = ((uint8_t *)&trailer)[3];

    flash_read(ADDR_FLASH_SECTOR_9 + CALI_GYRO_OFFSET, (uint32_t *)&gyro_cali, sizeof(gyro_cali) / 4);
    flash_read(ADDR_FLASH_SECTOR_9 + CALI_GYRO_OFFSET + sizeof(gyro_cali), &trailer, 1);
    if (((uint8_t *)&trailer)[3] == CALIED_FLAG)
    {
        INS_set_cali_gyro(gyro_cali.scale, gyro_cali.offset);
    }
}

int8_t get_control_temperature(void)
{
    /* Keep the original raw stored temperature even if HEAD is not valid. */
    return head_cali.temperature;
}

void get_flash_latitude(float *latitude)
{
    if (latitude != NULL)
    {
        *latitude = head_cali_done == CALIED_FLAG ? head_cali.latitude : 22.0f;
    }
}

void gimbal_to_chassis(uint8_t *gimbal_data){
	static fp32_to_bytes motor_yaw_now;			
	static fp32_to_bytes INS_yaw_now;				
	static fp32_to_bytes INS_pitch_now;			
	static fp32_to_bytes INS_roll_now;			
	
	static uint8_t gimbal_behaviour_now;		
	static uint8_t gimbal_shoot_mode_now;		
	



	
	motor_yaw_now.bytes[0] = gimbal_data[1];
	motor_yaw_now.bytes[1] = gimbal_data[2];
	motor_yaw_now.bytes[2] = gimbal_data[3];
	motor_yaw_now.bytes[3] = gimbal_data[4];
	
	if(motor_yaw_now.fp32 >= 3.14159265358979f){ motor_yaw_now.fp32 = 3.14159265358979f; }
	else if(motor_yaw_now.fp32 < -3.14159265358979f){ motor_yaw_now.fp32 = -3.14159265358979f; }
	
	chassis_move.gimbal_radian_of_ecd = motor_yaw_now.fp32;
	

		chassis_move.gimbal_radian_of_ecd = -chassis_move.gimbal_radian_of_ecd;


	INS_yaw_now.bytes[0] = gimbal_data[5];
	INS_yaw_now.bytes[1] = gimbal_data[6];
	INS_yaw_now.bytes[2] = gimbal_data[7];
	INS_yaw_now.bytes[3] = gimbal_data[8];
	GIMBAL_INS_yaw = INS_yaw_now.fp32;

	INS_pitch_now.bytes[0] = gimbal_data[9];
	INS_pitch_now.bytes[1] = gimbal_data[10];
	INS_pitch_now.bytes[2] = gimbal_data[11];
	INS_pitch_now.bytes[3] = gimbal_data[12];
	GIMBAL_INS_pitch = INS_pitch_now.fp32;
	
	INS_roll_now.bytes[0] = gimbal_data[13];
	INS_roll_now.bytes[1] = gimbal_data[14];
	INS_roll_now.bytes[2] = gimbal_data[15];
	INS_roll_now.bytes[3] = gimbal_data[16];
	GIMBAL_INS_roll = INS_roll_now.fp32;
	
	gimbal_behaviour_now = gimbal_data[17];
	chassis_move.gimbal_behaviour = gimbal_behaviour_now;
	
	gimbal_shoot_mode_now = gimbal_data[18];
	chassis_move.gimbal_shoot_mode = gimbal_shoot_mode_now;
	
	uint8_t* vtm_data_ptr = (uint8_t*)&f4_guard.value;
	
	for(uint8_t i = 0; i < sizeof(f4_guard.value) - 2U; i ++)
	{
		vtm_data_ptr[2 + i] = gimbal_data[19 + i];
	}
}
void send_data_to_chassis(void){
	static fp32 motor_yaw_now;							
  static fp32* INS_now_ptr; 							
	static uint8_t gimbal_behaviour_now;		
	static uint8_t shoot_mode_now;					
	static uint8_t* vtm_data_ptr = (uint8_t*)&h7_guard.value;		
	
  motor_yaw_now = loop_float_constrain((gimbal_control . gimbal_yaw_motor . radian_of_ecd), -3.14159265358979f, 3.14159265358979f);
  INS_now_ptr = get_INS_angle_point();	
	gimbal_behaviour_now = (uint8_t)gimbal_behaviour;
	extern shoot_control_t shoot_control; 
	shoot_mode_now = shoot_control.shoot_mode;
		
	
	const static uint8_t length = 46;
	static uint8_t data_chassis[length];		
	
	 
  data_chassis[0] = 0XFE;		
	
	
	uint32_t *ptr1 = (uint32_t*)&motor_yaw_now;
	data_chassis[1] = (*ptr1 >>  0) & 0xFF;			
	data_chassis[2] = (*ptr1 >>  8) & 0xFF;			
	data_chassis[3] = (*ptr1 >> 16) & 0xFF;			
	data_chassis[4] = (*ptr1 >> 24) & 0xFF;			
	
	uint32_t *ptr2 = (uint32_t*)INS_now_ptr;
	data_chassis[5] = (ptr2[0] >>  0) & 0xFF;		
	data_chassis[6] = (ptr2[0] >>  8) & 0xFF;		
	data_chassis[7] = (ptr2[0] >> 16) & 0xFF;		
	data_chassis[8] = (ptr2[0] >> 24) & 0xFF;		
  
	data_chassis[9]  = (ptr2[1] >>  0) & 0xFF;	
	data_chassis[10] = (ptr2[1] >>  8) & 0xFF;	
	data_chassis[11] = (ptr2[1] >> 16) & 0xFF;	
	data_chassis[12] = (ptr2[1] >> 24) & 0xFF;	
	
	data_chassis[13] = (ptr2[2] >> 	0) & 0xFF;	
	data_chassis[14] = (ptr2[2] >> 	8) & 0xFF;	
	data_chassis[15] = (ptr2[2] >> 16) & 0xFF;	
	data_chassis[16] = (ptr2[2] >> 24) & 0xFF;	
	
	data_chassis[17] = gimbal_behaviour_now;		
	
	data_chassis[18] = shoot_mode_now;					
	
	
	for(uint8_t i = 0; i < sizeof(h7_guard.value) - 2U; i ++)
	{
		data_chassis[19 + i] = vtm_data_ptr[2 + i];
	}
	for(uint8_t i = sizeof(h7_guard.value) - 2U; i < 26U; i ++)
	{
		data_chassis[19 + i] = 0;
	}
	

  data_chassis[length - 1] = 0XFD;  
	 
	
	USART1_Transmit_DMA(data_chassis, length);	
}
void send_to_gimbal(void){
	static fp32_to_bytes yaw_now, roll_now, pitch_now;		
	static uint8_t chassis_behaviour_now;									
	static uint8_t Robot_ID;															
	static fp32_to_bytes	chassis_vx_now;									
	static fp32_to_bytes	chassis_vy_now;									
	static fp32_to_bytes	chassis_wz_now;									

	
	const static uint8_t length = 28;
	static uint8_t data_gimbal[length];
  
	yaw_now.fp32 = chassis_move.chassis_yaw;
  pitch_now.fp32 = chassis_move.chassis_pitch;
  roll_now.fp32 = chassis_move.chassis_roll;
	extern chassis_behaviour_e chassis_behaviour_mode;
	chassis_behaviour_now = (uint8_t)chassis_behaviour_mode;
	extern robot_status_t robot_status;
	Robot_ID = robot_status.robot_id;
	chassis_vx_now.fp32 = chassis_move.vx;
	chassis_vy_now.fp32 = chassis_move.vy;
	chassis_wz_now.fp32 = chassis_move.wz;

	 
  data_gimbal[0] = 's';		
	
	
  data_gimbal[1] = yaw_now.bytes[0];
  data_gimbal[2] = yaw_now.bytes[1];
  data_gimbal[3] = yaw_now.bytes[2];
  data_gimbal[4] = yaw_now.bytes[3];
	
	data_gimbal[5] = pitch_now.bytes[0];
  data_gimbal[6] = pitch_now.bytes[1];
  data_gimbal[7] = pitch_now.bytes[2];
  data_gimbal[8] = pitch_now.bytes[3];
	
  data_gimbal[9]  = roll_now.bytes[0];
  data_gimbal[10] = roll_now.bytes[1];
  data_gimbal[11] = roll_now.bytes[2];
  data_gimbal[12] = roll_now.bytes[3];
	
	data_gimbal[13] = chassis_behaviour_now;
	
	data_gimbal[14] = Robot_ID;
	
  data_gimbal[15] = chassis_vx_now.bytes[0];
  data_gimbal[16] = chassis_vx_now.bytes[1];
  data_gimbal[17] = chassis_vx_now.bytes[2];
  data_gimbal[18] = chassis_vx_now.bytes[3];
	
	data_gimbal[19] = chassis_vy_now.bytes[0];
  data_gimbal[20] = chassis_vy_now.bytes[1];
  data_gimbal[21] = chassis_vy_now.bytes[2];
  data_gimbal[22] = chassis_vy_now.bytes[3];
	
  data_gimbal[23] = chassis_wz_now.bytes[0];
  data_gimbal[24] = chassis_wz_now.bytes[1];
  data_gimbal[25] = chassis_wz_now.bytes[2];
  data_gimbal[26] = chassis_wz_now.bytes[3];	
	
  data_gimbal[27] = 'e';		
	 

  usart1_tx_dma_enable(data_gimbal, length);
}
void chassis_to_gimbal(uint8_t *data_chassis){
	static fp32_to_bytes yaw_now, pitch_now, roll_now;		
	static uint8_t chassis_behaviour_now;									
	static uint8_t Robot_ID;															
	static fp32_to_bytes chassis_vx_now;									
	static fp32_to_bytes chassis_vy_now;									
	static fp32_to_bytes chassis_wz_now;									

	
	yaw_now.bytes[0] = data_chassis[1];
	yaw_now.bytes[1] = data_chassis[2];
	yaw_now.bytes[2] = data_chassis[3];
	yaw_now.bytes[3] = data_chassis[4];
	chassis_data.chassis_yaw = yaw_now.fp32;

	pitch_now.bytes[0] = data_chassis[5];
	pitch_now.bytes[1] = data_chassis[6];
	pitch_now.bytes[2] = data_chassis[7];
	pitch_now.bytes[3] = data_chassis[8];
	chassis_data.chassis_pitch = pitch_now.fp32;

	roll_now.bytes[0] = data_chassis[9];
	roll_now.bytes[1] = data_chassis[10];
	roll_now.bytes[2] = data_chassis[11];
	roll_now.bytes[3] = data_chassis[12];
	chassis_data.chassis_roll = roll_now.fp32;

	
	chassis_behaviour_now = data_chassis[13];
	chassis_data.chassis_mode = chassis_behaviour_now;

	
	Robot_ID = data_chassis[14];
	chassis_data.robot_id = Robot_ID;

	
	chassis_vx_now.bytes[0] = data_chassis[15];
	chassis_vx_now.bytes[1] = data_chassis[16];
	chassis_vx_now.bytes[2] = data_chassis[17];
	chassis_vx_now.bytes[3] = data_chassis[18];
	chassis_data.chassis_vx = chassis_vx_now.fp32;

	chassis_vy_now.bytes[0] = data_chassis[19];
	chassis_vy_now.bytes[1] = data_chassis[20];
	chassis_vy_now.bytes[2] = data_chassis[21];
	chassis_vy_now.bytes[3] = data_chassis[22];
	chassis_data.chassis_vy = chassis_vy_now.fp32;

	chassis_wz_now.bytes[0] = data_chassis[23];
	chassis_wz_now.bytes[1] = data_chassis[24];
	chassis_wz_now.bytes[2] = data_chassis[25];
	chassis_wz_now.bytes[3] = data_chassis[26];
	chassis_data.chassis_wz = chassis_wz_now.fp32;
	
}

volatile struct { uint32_t status, checks, failure; } test_result;
__asm void stop_test(void) {
  BKPT 0
  BX lr
}
void check(int ok, uint32_t code) {
  ++test_result.checks;
  if (!ok) { test_result.failure = code; stop_test(); }
}
void test_entry(void) {
  uint8_t *payload = (uint8_t *)&h7_guard.value;
  uint8_t *received = (uint8_t *)&f4_guard.value;
  float latitude = -1.0f;
  head_cali_t head;
  imu_cali_t gyro;
  memset(&h7_guard, 0xCD, sizeof(h7_guard));
  memset(&f4_guard, 0xAB, sizeof(f4_guard));
  for (uint32_t i = 0; i < sizeof(h7_guard.value); ++i) payload[i] = 0x40U + i;
  gimbal_control.gimbal_yaw_motor.radian_of_ecd = 1.25f;
  ins_angles[0] = -2.0f; ins_angles[1] = 0.375f; ins_angles[2] = -0.5f;
  gimbal_behaviour = 5; shoot_control.shoot_mode = 4;
  send_data_to_chassis();
  check(frame46_size == 46 && frame46[0] == 0xFE && frame46[45] == 0xFD, 1);
  check(!memcmp(frame46 + 1, &gimbal_control.gimbal_yaw_motor.radian_of_ecd, 4), 2);
  check(!memcmp(frame46 + 5, ins_angles, 12), 3);
  check(frame46[17] == 5 && frame46[18] == 4, 4);
  check(!memcmp(frame46 + 19, payload + 2, 19), 5);
  for (uint32_t i = 38; i <= 44; ++i) check(frame46[i] == 0, 6);
  /* Nonzero reserved bytes must not write beyond the receiver structure. */
  memset(frame46 + 38, 0xEF, 7);
  gimbal_to_chassis(frame46);
  check(!memcmp(received + 2, payload + 2, 19), 7);
  check(received[0] == 0xAB && received[1] == 0xAB, 8);
  for (uint32_t i = 0; i < 8; ++i) {
    check(f4_guard.before[i] == 0xAB && f4_guard.after[i] == 0xAB, 9);
    check(h7_guard.before[i] == 0xCD && h7_guard.after[i] == 0xCD, 10);
  }
  check(chassis_move.gimbal_radian_of_ecd == -1.25f, 11);
  check(GIMBAL_INS_yaw == -2.0f && GIMBAL_INS_pitch == 0.375f && GIMBAL_INS_roll == -0.5f, 12);
  check(chassis_move.gimbal_behaviour == 5 && chassis_move.gimbal_shoot_mode == 4, 13);
  chassis_move.chassis_yaw = 0.125f; chassis_move.chassis_pitch = -1.0f;
  chassis_move.chassis_roll = 0.5f;
  chassis_move.vx = 1.0f; chassis_move.vy = -0.25f; chassis_move.wz = 0.75f;
  chassis_behaviour_mode = 2; robot_status.robot_id = 103;
  send_to_gimbal();
  check(frame28_size == 28 && frame28[0] == 's' && frame28[27] == 'e', 14);
  check(frame28[13] == 2 && frame28[14] == 103, 15);
  chassis_to_gimbal(frame28);
  check(chassis_data.chassis_yaw == 0.125f && chassis_data.chassis_pitch == -1.0f && chassis_data.chassis_roll == 0.5f, 16);
  check(chassis_data.chassis_vx == 1.0f && chassis_data.chassis_vy == -0.25f && chassis_data.chassis_wz == 0.75f, 17);
  check(chassis_data.chassis_mode == 2 && chassis_data.robot_id == 103, 18);
  /* Legacy sector: HEAD at 0, flags at 11/35/63/91/119, GYR at 36. */
  memset(flash_image, 0xFF, sizeof(flash_image));
  memset(&head, 0, sizeof(head));
  head.self_id = 3; head.firmware_version = 17; head.temperature = 41; head.latitude = 31.25f;
  gyro.offset[0] = 0.1f; gyro.offset[1] = -0.2f; gyro.offset[2] = 0.3f;
  gyro.scale[0] = 1.0f; gyro.scale[1] = 1.1f; gyro.scale[2] = 0.9f;
  memcpy(flash_image, &head, 8); flash_image[11] = 0x55;
  memcpy(flash_image + 36, &gyro, 24); flash_image[63] = 0x55;
  cali_param_init(); get_flash_latitude(&latitude);
  check(get_control_temperature() == 41 && latitude == 31.25f, 19);
  check(gyro_load_count == 1 && !memcmp(loaded_offset, gyro.offset, 12) && !memcmp(loaded_scale, gyro.scale, 12), 20);
  flash_image[11] = 0xFF; flash_image[63] = 0;
  cali_param_init(); get_flash_latitude(&latitude); get_flash_latitude(0);
  check(get_control_temperature() == 41 && latitude == 22.0f, 21);
  check(gyro_load_count == 1, 22);
  test_result.status = 0x600D;
  stop_test();
}
