#ifndef CHASSIS_SOFT_TURN_LOG_H
#define CHASSIS_SOFT_TURN_LOG_H

#include "chassis_task.h"

#define CHASSIS_SOFT_LOG_SIZE 146U
void chassis_soft_turn_log_publish(const chassis_move_t *move, uint32_t now_ms, uint8_t fault);

#endif
