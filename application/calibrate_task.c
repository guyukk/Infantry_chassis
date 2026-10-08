/* Boot-time calibration loading for the F407 chassis.
 * Legacy flash layout: HEAD 8+4, GM 20+4, GYR 24+4, ACC 24+4, MAG 24+4.
 * Each trailer contains a three-byte name and a one-byte valid flag.
 * GM remains reserved so the stored gyro record stays at byte offset 36.
 * The original chassis never started a calibration task or wrote these records.
 */
#include "calibrate_task.h"
#include "bsp_flash.h"
#include "INS_task.h"
#include <stddef.h>

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
