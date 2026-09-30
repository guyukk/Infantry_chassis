/**
  *********************************************************************
  * @file      observe_task.c/h
  * @brief     该任务是与裁判系统进行交互，绘制UI
  * @note
  * @history
  *
  @verbatim
  ==============================================================================

  ==============================================================================
  @endverbatim
  *********************************************************************
  */

#include "UI.h"
#include "cmsis_os.h"
#include "stdio.h"
#include "chassis_task.h"
#include "gimbal_task.h"
#include "shoot.h"
#include "auto_aim.h"
#include "referee.h"
#include "protocol.h"
#include "pm01_api.h"
#include "gimbal_behaviour.h"

#include "ui_g.h"

extern volatile pm01_od_t pm01_od;
extern robot_status_t robot_status;

#define UI_TIME    100

const char aim_mode[2][4] = {" ON", "OFF"};

const char shoot_mode[7][6] = {" OFF ", "FRICT", "READY", "READY", "BURST", "AUTO ", "DONE "};

const char gimbal_mode[3][6] = {" NAN ", "REAL ", "SPIN "};

const char chassis_mode[4][6] = {" NAN ", "REAL ", "C_YAW", "SPIN "};

int32_t super_cap_volt = 0;

void UI_init(void);
void UI_set(void);
void UI_update(void);
void UI_loop(void);
void UI_send_cmd(void);

/*******************************************************************************
 * @功能       : UI任务主函数
 * @参数       : argument - 任务参数（未使用）
 * @返回值     : 无
 * @概述       : UI任务的主循环，周期性调用UI初始化、设置、更新和发送函数
 *******************************************************************************/
void UI_task(void const * argument)
{
    osDelay(500);
    UI_init();
    while (1)
    {
        UI_set();
        UI_update();
        UI_loop();
        osDelay(UI_TIME);
    }
}

/*******************************************************************************
 * @功能       : UI初始化函数
 * @参数       : 无
 * @返回值     : 无
 * @概述       : 初始化UI系统，调用ui_init_g函数
 *******************************************************************************/
void UI_init(void)
{
    ui_self_id = get_robot_id();
    if (ui_self_id != 0)
    {
        ui_init_g();
    }
}

/*******************************************************************************
 * @功能       : UI设置函数
 * @参数       : 无
 * @返回值     : 无
 * @概述       : 根据遥控器开关状态和机器人ID设置UI颜色等属性
 *******************************************************************************/
void UI_set(void)
{
    if (switch_is_down(chassis_move.chassis_RC->rc.s[SHOOT_RC_MODE_CHANNEL]))
    {
        ui_self_id = get_robot_id();
        if (ui_self_id != 0)
        {
            ui_init_g();
        }
    }
}

/*******************************************************************************
 * @功能       : UI更新函数
 * @参数       : 无
 * @返回值     : 无
 * @概述       : 读取各个模块的状态数据，更新UI显示内容，包括底盘、射击、
 *               云台、自瞄状态，以及超电百分比和HP数值
 *******************************************************************************/
void UI_update(void)
{
    uint8_t chassis_mode_index = 0;
    uint8_t shoot_mode_index = 0;
    uint8_t gimbal_mode_index = 0;
    uint8_t aim_mode_index = 0;

    super_cap_volt = (int32_t)pm01_od.v_out;
    ui_self_id = get_robot_id();

    if (ui_self_id == 0)
    {
        return;
    }

    if (chassis_move.chassis_mode <= CHASSIS_VECTOR_SPIN)
    {
        chassis_mode_index = (uint8_t)chassis_move.chassis_mode;
    }
		
    if (gimbal_behaviour <= GIMBAL_SPIN)
    {
        if (gimbal_behaviour == GIMBAL_SPIN)
        {
            gimbal_mode_index = 2;
        }
        else if (gimbal_behaviour == GIMBAL_ABSOLUTE_ANGLE || \
                 gimbal_behaviour == GIMBAL_RELATIVE_ANGLE)
        {
            gimbal_mode_index = 1;
        }
        else
        {
            gimbal_mode_index = 0;
        }
    }

    aim_mode_index = auto_aim_flag ? 0 : 1;

    strcpy(ui_g_Ungroup_chassis_dis->string, chassis_mode[chassis_mode_index]);
    strcpy(ui_g_Ungroup_friction_mode->string, shoot_mode[shoot_mode_index]);
    strcpy(ui_g_Ungroup_gimbal_dis->string, gimbal_mode[gimbal_mode_index]);
    strcpy(ui_g_Ungroup_aim_dis->string, aim_mode[aim_mode_index]);

    uint8_t super_cap_percent = 0;
    if (super_cap_volt >= 2000 && super_cap_volt <= 2600)
    {
        super_cap_percent = (uint8_t)((super_cap_volt - 2000) * 100 / \
                                     (2600 - 2000));
    }
    else if (super_cap_volt > 2600)
    {
        super_cap_percent = 100;
    }

    char super_cap_str[6];
    sprintf(super_cap_str, "%3d%%", super_cap_percent);
    strcpy(ui_g_Ungroup_super_capacity_dis->string, super_cap_str);

    ui_g_Ungroup_hp_num->number = robot_status.current_HP;
}

/*******************************************************************************
 * @功能       : UI循环函数
 * @参数       : 无
 * @返回值     : 无
 * @概述       : 调用ui_update_g函数，将更新的UI元素发送到裁判系统
 *******************************************************************************/
void UI_loop(void)
{
    ui_update_g();
}
