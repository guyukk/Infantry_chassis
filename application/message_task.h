#ifndef MESSAGE_TASK_H
#define MESSAGE_TASK_H
#include "struct_typedef.h"
#include "CAN_receive.h"
#include "pid.h"
#include "remote_control.h"

#define MESSAGE_TASK_INIT_TIME 5


extern void message_task(void const *pvParameters);


#endif
