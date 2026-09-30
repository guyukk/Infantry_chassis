//
// Created by RM UI Designer
// Dynamic Edition
//

#include "string.h"
#include "ui_interface.h"
#include "ui_g.h"

#define TOTAL_FIGURE 2
#define TOTAL_STRING 6
#define TOTAL_NUMBER 1

ui_interface_figure_t ui_g_now_figures[TOTAL_FIGURE];
uint8_t ui_g_dirty_figure[TOTAL_FIGURE];

ui_interface_string_t ui_g_now_strings[TOTAL_STRING];
uint8_t ui_g_dirty_string[TOTAL_STRING];

ui_interface_number_t ui_g_now_numbers[TOTAL_NUMBER];
uint8_t ui_g_dirty_number[TOTAL_NUMBER];

#ifndef MANUAL_DIRTY
ui_interface_figure_t ui_g_last_figures[TOTAL_FIGURE];
ui_interface_string_t ui_g_last_strings[TOTAL_STRING];
ui_interface_number_t ui_g_last_numbers[TOTAL_NUMBER];
#endif

#define SCAN_AND_SEND() ui_scan_and_send(ui_g_now_figures, ui_g_dirty_figure, \
                                        ui_g_now_strings, ui_g_dirty_string, \
                                        TOTAL_FIGURE, TOTAL_STRING)

/*******************************************************************************
 * @功能       : UI元素初始化函数
 * @参数       : 无
 * @返回值     : 无
 * @概述       : 初始化所有UI元素（图形、字符串、数字），设置初始属性和位置，
 *               并将所有元素发送到裁判系统
 *******************************************************************************/
void ui_init_g()
{
    ui_g_Ungroup_auto_aim->figure_type = 1;
    ui_g_Ungroup_auto_aim->operate_type = 1;
    ui_g_Ungroup_auto_aim->layer = 0;
    ui_g_Ungroup_auto_aim->color = 0;
    ui_g_Ungroup_auto_aim->start_x = 649;
    ui_g_Ungroup_auto_aim->start_y = 203;
    ui_g_Ungroup_auto_aim->width = 1;
    ui_g_Ungroup_auto_aim->end_x = 1249;
    ui_g_Ungroup_auto_aim->end_y = 803;

    ui_g_Ungroup_Crosshair->figure_type = 2;
    ui_g_Ungroup_Crosshair->operate_type = 1;
    ui_g_Ungroup_Crosshair->layer = 0;
    ui_g_Ungroup_Crosshair->color = 0;
    ui_g_Ungroup_Crosshair->start_x = 958;
    ui_g_Ungroup_Crosshair->start_y = 441;
    ui_g_Ungroup_Crosshair->width = 5;
    ui_g_Ungroup_Crosshair->r = 15;

    ui_g_Ungroup_chassis_dis->figure_type = 3;
    ui_g_Ungroup_chassis_dis->operate_type = 1;
    ui_g_Ungroup_chassis_dis->layer = 0;
    ui_g_Ungroup_chassis_dis->color = 0;
    ui_g_Ungroup_chassis_dis->font_size = 20;
    ui_g_Ungroup_chassis_dis->str_length = 5;
    ui_g_Ungroup_chassis_dis->width = 1;
    ui_g_Ungroup_chassis_dis->start_x = 100;
    ui_g_Ungroup_chassis_dis->start_y = 100;
    strcpy(ui_g_Ungroup_chassis_dis->string, " NAN ");

    ui_g_Ungroup_friction_mode->figure_type = 3;
    ui_g_Ungroup_friction_mode->operate_type = 1;
    ui_g_Ungroup_friction_mode->layer = 0;
    ui_g_Ungroup_friction_mode->color = 0;
    ui_g_Ungroup_friction_mode->font_size = 20;
    ui_g_Ungroup_friction_mode->str_length = 5;
    ui_g_Ungroup_friction_mode->width = 1;
    ui_g_Ungroup_friction_mode->start_x = 100;
    ui_g_Ungroup_friction_mode->start_y = 150;
    strcpy(ui_g_Ungroup_friction_mode->string, " OFF ");

    ui_g_Ungroup_gimbal_dis->figure_type = 3;
    ui_g_Ungroup_gimbal_dis->operate_type = 1;
    ui_g_Ungroup_gimbal_dis->layer = 0;
    ui_g_Ungroup_gimbal_dis->color = 0;
    ui_g_Ungroup_gimbal_dis->font_size = 20;
    ui_g_Ungroup_gimbal_dis->str_length = 5;
    ui_g_Ungroup_gimbal_dis->width = 1;
    ui_g_Ungroup_gimbal_dis->start_x = 100;
    ui_g_Ungroup_gimbal_dis->start_y = 200;
    strcpy(ui_g_Ungroup_gimbal_dis->string, " NAN ");

    ui_g_Ungroup_aim_dis->figure_type = 3;
    ui_g_Ungroup_aim_dis->operate_type = 1;
    ui_g_Ungroup_aim_dis->layer = 0;
    ui_g_Ungroup_aim_dis->color = 0;
    ui_g_Ungroup_aim_dis->font_size = 20;
    ui_g_Ungroup_aim_dis->str_length = 3;
    ui_g_Ungroup_aim_dis->width = 1;
    ui_g_Ungroup_aim_dis->start_x = 100;
    ui_g_Ungroup_aim_dis->start_y = 300;
    strcpy(ui_g_Ungroup_aim_dis->string, "OFF");

    ui_g_Ungroup_hp_dis->figure_type = 3;
    ui_g_Ungroup_hp_dis->operate_type = 1;
    ui_g_Ungroup_hp_dis->layer = 0;
    ui_g_Ungroup_hp_dis->color = 0;
    ui_g_Ungroup_hp_dis->font_size = 20;
    ui_g_Ungroup_hp_dis->str_length = 5;
    ui_g_Ungroup_hp_dis->width = 1;
    ui_g_Ungroup_hp_dis->start_x = 100;
    ui_g_Ungroup_hp_dis->start_y = 350;
    strcpy(ui_g_Ungroup_hp_dis->string, "HP:  ");

    ui_g_Ungroup_super_capacity_dis->figure_type = 3;
    ui_g_Ungroup_super_capacity_dis->operate_type = 1;
    ui_g_Ungroup_super_capacity_dis->layer = 0;
    ui_g_Ungroup_super_capacity_dis->color = 0;
    ui_g_Ungroup_super_capacity_dis->font_size = 20;
    ui_g_Ungroup_super_capacity_dis->str_length = 5;
    ui_g_Ungroup_super_capacity_dis->width = 1;
    ui_g_Ungroup_super_capacity_dis->start_x = 100;
    ui_g_Ungroup_super_capacity_dis->start_y = 250;
    strcpy(ui_g_Ungroup_super_capacity_dis->string, "  0%");

    ui_g_Ungroup_hp_num->figure_type = 4;
    ui_g_Ungroup_hp_num->operate_type = 1;
    ui_g_Ungroup_hp_num->layer = 0;
    ui_g_Ungroup_hp_num->color = 0;
    ui_g_Ungroup_hp_num->font_size = 20;
    ui_g_Ungroup_hp_num->width = 1;
    ui_g_Ungroup_hp_num->start_x = 100;
    ui_g_Ungroup_hp_num->start_y = 400;
    ui_g_Ungroup_hp_num->number = 0;

    uint32_t idx = 0;
    for (int i = 0; i < TOTAL_FIGURE; i++)
    {
        ui_g_now_figures[i].figure_name[2] = idx & 0xFF;
        ui_g_now_figures[i].figure_name[1] = (idx >> 8) & 0xFF;
        ui_g_now_figures[i].figure_name[0] = (idx >> 16) & 0xFF;
        ui_g_now_figures[i].operate_type = 1;
#ifndef MANUAL_DIRTY
        ui_g_last_figures[i] = ui_g_now_figures[i];
#endif
        ui_g_dirty_figure[i] = 1;
        idx++;
    }

    for (int i = 0; i < TOTAL_STRING; i++)
    {
        ui_g_now_strings[i].figure_name[2] = idx & 0xFF;
        ui_g_now_strings[i].figure_name[1] = (idx >> 8) & 0xFF;
        ui_g_now_strings[i].figure_name[0] = (idx >> 16) & 0xFF;
        ui_g_now_strings[i].operate_type = 1;
#ifndef MANUAL_DIRTY
        ui_g_last_strings[i] = ui_g_now_strings[i];
#endif
        ui_g_dirty_string[i] = 1;
        idx++;
    }

    for (int i = 0; i < TOTAL_NUMBER; i++)
    {
        ui_g_now_numbers[i].figure_name[2] = idx & 0xFF;
        ui_g_now_numbers[i].figure_name[1] = (idx >> 8) & 0xFF;
        ui_g_now_numbers[i].figure_name[0] = (idx >> 16) & 0xFF;
        ui_g_now_numbers[i].operate_type = 1;
#ifndef MANUAL_DIRTY
        ui_g_last_numbers[i] = ui_g_now_numbers[i];
#endif
        ui_g_dirty_number[i] = 1;
        idx++;
    }

    SCAN_AND_SEND();

    for (int i = 0; i < TOTAL_FIGURE; i++)
    {
        ui_g_now_figures[i].operate_type = 2;
    }

    for (int i = 0; i < TOTAL_STRING; i++)
    {
        ui_g_now_strings[i].operate_type = 2;
    }

    for (int i = 0; i < TOTAL_NUMBER; i++)
    {
        ui_g_now_numbers[i].operate_type = 2;
    }
}

/*******************************************************************************
 * @功能       : UI元素更新函数
 * @参数       : 无
 * @返回值     : 无
 * @概述       : 检查UI元素是否发生变化，如果变化则标记为需要更新，
 *               并将变化的元素发送到裁判系统
 *******************************************************************************/
void ui_update_g()
{
#ifndef MANUAL_DIRTY
    for (int i = 0; i < TOTAL_FIGURE; i++)
    {
        if (memcmp(&ui_g_now_figures[i], &ui_g_last_figures[i], \
                   sizeof(ui_g_now_figures[i])) != 0)
        {
            ui_g_dirty_figure[i] = 1;
            ui_g_last_figures[i] = ui_g_now_figures[i];
        }
    }

    for (int i = 0; i < TOTAL_STRING; i++)
    {
        if (memcmp(&ui_g_now_strings[i], &ui_g_last_strings[i], \
                   sizeof(ui_g_now_strings[i])) != 0)
        {
            ui_g_dirty_string[i] = 1;
            ui_g_last_strings[i] = ui_g_now_strings[i];
        }
    }

    for (int i = 0; i < TOTAL_NUMBER; i++)
    {
        if (memcmp(&ui_g_now_numbers[i], &ui_g_last_numbers[i], \
                   sizeof(ui_g_now_numbers[i])) != 0)
        {
            ui_g_dirty_number[i] = 1;
            ui_g_last_numbers[i] = ui_g_now_numbers[i];
        }
    }
#endif
    SCAN_AND_SEND();
}
