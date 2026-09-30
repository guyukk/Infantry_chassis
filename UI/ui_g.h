//
// Created by RM UI Designer
// Dynamic Edition
//

#ifndef UI_g_H
#define UI_g_H

#include "ui_interface.h"

extern ui_interface_figure_t ui_g_now_figures[2];
extern uint8_t ui_g_dirty_figure[2];

extern ui_interface_string_t ui_g_now_strings[6];
extern uint8_t ui_g_dirty_string[6];

extern ui_interface_number_t ui_g_now_numbers[1];
extern uint8_t ui_g_dirty_number[1];

#define ui_g_Ungroup_auto_aim ((ui_interface_rect_t*)&(ui_g_now_figures[0]))
#define ui_g_Ungroup_Crosshair ((ui_interface_round_t*)&(ui_g_now_figures[1]))

#define ui_g_Ungroup_chassis_dis ((ui_interface_string_t*)&(ui_g_now_strings[0]))
#define ui_g_Ungroup_friction_mode ((ui_interface_string_t*)&(ui_g_now_strings[1]))
#define ui_g_Ungroup_gimbal_dis ((ui_interface_string_t*)&(ui_g_now_strings[2]))
#define ui_g_Ungroup_aim_dis ((ui_interface_string_t*)&(ui_g_now_strings[3]))
#define ui_g_Ungroup_hp_dis ((ui_interface_string_t*)&(ui_g_now_strings[4]))
#define ui_g_Ungroup_super_capacity_dis ((ui_interface_string_t*)&(ui_g_now_strings[5]))

#define ui_g_Ungroup_hp_num ((ui_interface_number_t*)&(ui_g_now_numbers[0]))


#ifdef MANUAL_DIRTY
#define ui_g_Ungroup_auto_aim_dirty (ui_g_dirty_figure[0])
#define ui_g_Ungroup_Crosshair_dirty (ui_g_dirty_figure[1])
#define ui_g_Ungroup_chassis_dis_dirty (ui_g_dirty_string[0])
#define ui_g_Ungroup_friction_mode_dirty (ui_g_dirty_string[1])
#define ui_g_Ungroup_gimbal_dis_dirty (ui_g_dirty_string[2])
#define ui_g_Ungroup_aim_dis_dirty (ui_g_dirty_string[3])
#define ui_g_Ungroup_hp_dis_dirty (ui_g_dirty_string[4])
#define ui_g_Ungroup_super_capacity_dis_dirty (ui_g_dirty_string[5])
#define ui_g_Ungroup_hp_num_dirty (ui_g_dirty_number[0])

#endif

void ui_init_g(void);
void ui_update_g(void);

#endif // UI_g_H
