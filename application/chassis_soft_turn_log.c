#include "chassis_soft_turn_log.h"
#include "robot_param.h"

#if CHASSIS_SOFT_TURN_TELEMETRY
#include "usart.h"
#include <string.h>

static uint8_t log_buffer[2][CHASSIS_SOFT_LOG_SIZE];
static uint8_t next_buffer;
static uint32_t last_ms, sequence;

static void put_u16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void put_u32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static void put_float(uint8_t **p, fp32 value)
{
    memcpy(*p, &value, sizeof(value)); /* F407 is little endian, IEEE-754 binary32. */
    *p += sizeof(value);
}

static uint16_t crc16(const uint8_t *p, uint16_t length)
{
    uint16_t crc = 0xFFFFU, i;
    uint8_t bit;
    for (i = 0U; i < length; ++i)
    {
        crc ^= (uint16_t)p[i] << 8;
        for (bit = 0U; bit < 8U; ++bit)
            crc = (crc & 0x8000U) ? (uint16_t)((crc << 1) ^ 0x1021U) : (uint16_t)(crc << 1);
    }
    return crc;
}
#endif

void chassis_soft_turn_log_publish(const chassis_move_t *move, uint32_t now_ms, uint8_t fault)
{
#if CHASSIS_SOFT_TURN_TELEMETRY
    uint8_t *packet, *p, i;
    if ((uint32_t)(now_ms - last_ms) < 20U) return;
    last_ms = now_ms;
    packet = log_buffer[next_buffer];
    packet[0] = 'S'; packet[1] = 'T'; packet[2] = 1U;
    packet[3] = (fault ? 1U : 0U) | (move->soft_turn.braking ? 2U : 0U) |
        (move->soft_turn.holding ? 4U : 0U);
    put_u32(packet + 4, sequence++);
    put_u32(packet + 8, now_ms);
    packet[12] = (uint8_t)move->chassis_mode;
    packet[13] = 0U;
    put_u16(packet + 14, CHASSIS_SOFT_LOG_SIZE);
    p = packet + 16;
    put_float(&p, move->raw_target.vx); put_float(&p, move->raw_target.vy); put_float(&p, move->raw_target.wz);
    put_float(&p, move->vx_set); put_float(&p, move->vy_set); put_float(&p, move->wz_set);
    for (i = 0; i < 4U; ++i) put_float(&p, move->chassis_6020[i].angle);
    for (i = 0; i < 4U; ++i) put_float(&p, move->chassis_6020[i].angle_set);
    for (i = 0; i < 4U; ++i) put_float(&p, move->soft_turn.angle_error[i]);
    for (i = 0; i < 4U; ++i) put_float(&p, move->chassis_3508[i].speed_set);
    for (i = 0; i < 4U; ++i) put_float(&p, move->chassis_3508[i].speed);
    put_float(&p, move->chassis_roll); put_float(&p, move->chassis_pitch);
    for (i = 0; i < 4U; ++i) { put_u16(p, (uint16_t)move->chassis_3508[i].give_current); p += 2; }
    for (i = 0; i < 4U; ++i) { put_u16(p, (uint16_t)move->chassis_6020[i].give_current); p += 2; }
    put_u16(p, crc16(packet, CHASSIS_SOFT_LOG_SIZE - 2U));
    /* HAL_BUSY drops this record; it never aborts or overwrites the active DMA buffer. */
    if (HAL_UART_Transmit_DMA(&huart6, packet, CHASSIS_SOFT_LOG_SIZE) == HAL_OK)
        next_buffer ^= 1U;
#else
    (void)move; (void)now_ms; (void)fault;
#endif
}
