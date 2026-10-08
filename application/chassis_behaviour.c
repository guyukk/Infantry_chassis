/**
  * @file       chassis_behaviour.c
  * @brief      底盘行为模式设置
  * @note       
  *			如果要添加一个新的行为模式
	*
  *			1.在.h文件中的 chassis_behaviour_e枚举 下添加新的底盘模式CHASSIS_XXX_XXX
  *			
  *			2.在.c文件中的函数声明区末尾添加新的函数声明, 并在文件末尾添加新的具体控制实现函数
  *			
  *			3.在.c文件中的 chassis_behaviour_mode_set() 函数中添加新的判断, 使chassis_behaviour能被赋值成CHASSIS_XXX_XXX
  *			
	*			4.在.c文件中的 chassis_behaviour_control_set() 函数中添加新判断, 以此调用相关控制函数
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

#include "arm_math.h"
#include "chassis_behaviour.h"
#include "chassis_task.h"
#include "cmsis_os.h"
#include "robot_param.h"

/* 函数声明区 */
static void chassis_no_move_control											(fp32 *vx_set, fp32 *vy_set, fp32 *wz_set, chassis_move_t *chassis_move_rc_to_vector);
static void chassis_infantry_follow_gimbal_yaw_control	(fp32 *vx_set, fp32 *vy_set, fp32 *wz_set, chassis_move_t *chassis_move_rc_to_vector);
static void chassis_spin_control												(fp32 *vx_set, fp32 *vy_set, fp32 *wz_set, chassis_move_t *chassis_move_rc_to_vector);


chassis_behaviour_e chassis_behaviour_mode = CHASSIS_NO_MOVE;

/**
  * @brief          根据遥控器开关位置和键盘输入设置底盘行为模式, 并为每种行为模式选择合适的底盘控制模式
  * @param[in]      chassis_move_mode: 底盘数据指针
  * @retval         none
  */
void chassis_behaviour_mode_set(chassis_move_t *chassis_move_mode)
{
	if (chassis_move_mode == NULL) return;

	//遥控器设置模式，以下参数均可选择
	//CHASSIS_ZERO_FORCE, CHASSIS_NO_MOVE, CHASSIS_FOLLOW_GIMBAL_YAW, CHASSIS_OPEN, CHASSIS_SPIN
	if (switch_is_down(chassis_move_mode->chassis_RC->rc.s[CHASSIS_MODE_CHANNEL]))
	{
		chassis_behaviour_mode = CHASSIS_FOLLOW_GIMBAL_YAW;		//下档跟随云台模式
	}
	else if (switch_is_mid(chassis_move_mode->chassis_RC->rc.s[CHASSIS_MODE_CHANNEL]))
	{
		chassis_behaviour_mode = CHASSIS_NO_MOVE;		//中档静止模式
	}
	else if (switch_is_up(chassis_move_mode->chassis_RC->rc.s[CHASSIS_MODE_CHANNEL]) && !switch_is_down(chassis_move_mode->chassis_RC->rc.s[1]))  //电脑没改
	{
		chassis_behaviour_mode = CHASSIS_SPIN;	//上档小陀螺模式
	}
	
	
	/* 遥控器在下档，根据键盘x、c、shift来改变模式，每2个毫秒刷新一次，所以要一直按着才能保证模式正确 */
	if (switch_is_down(chassis_move_mode->chassis_RC->rc.s[CHASSIS_MODE_CHANNEL]) && (chassis_move_mode->chassis_RC->key.v & CHASSIS_STOP_KEYBOARD))
	{
		chassis_behaviour_mode = CHASSIS_NO_MOVE;		//x键 静止模式
	}
	else if (switch_is_down(chassis_move_mode->chassis_RC->rc.s[CHASSIS_MODE_CHANNEL]) && (chassis_move_mode->chassis_RC->key.v & CHASSIS_FOLLOW_KEYBOARD))
	{
		chassis_behaviour_mode = CHASSIS_FOLLOW_GIMBAL_YAW;		//c键 跟随云台模式
	}
	else if(switch_is_down(chassis_move_mode->chassis_RC->rc.s[CHASSIS_MODE_CHANNEL]) && (chassis_move_mode->chassis_RC->key.v & CHASSIS_SPIN_KEYBOARD))
	{
		chassis_behaviour_mode = CHASSIS_SPIN;	//shift键 小陀螺模式
	}

	//根据行为模式选择一个底盘控制模式
	if (chassis_behaviour_mode == CHASSIS_NO_MOVE)	//静止模式
	{
		chassis_move_mode->chassis_mode = CHASSIS_VECTOR_NO_MOVE; 	//速度静止模式
	}
	else if (chassis_behaviour_mode == CHASSIS_FOLLOW_GIMBAL_YAW)		//跟随云台模式
	{
		chassis_move_mode->chassis_mode = CHASSIS_VECTOR_FOLLOW_GIMBAL_YAW; 	//速度跟随云台模式
	}
	else if(chassis_behaviour_mode == CHASSIS_SPIN)		//小陀螺模式
	{
		chassis_move_mode->chassis_mode = CHASSIS_VECTOR_SPIN;	//速度小陀螺模式
	}
	

}

/**
  * @brief          根据当前行为模式调用对应的控制函数来设置底盘运动的三个参数
  * @param[out]     vx_set, 通常控制纵向移动.
  * @param[out]     vy_set, 通常控制横向移动.
  * @param[out]     wz_set, 通常控制旋转运动.
  * @param[in]      chassis_move_rc_to_vector, 包括底盘所有信息.
  * @retval         none
  */
void chassis_behaviour_control_set(fp32 *vx_set, fp32 *vy_set, fp32 *wz_set, chassis_move_t *chassis_move_rc_to_vector)
{
	if (vx_set == NULL || vy_set == NULL || wz_set == NULL || chassis_move_rc_to_vector == NULL) return;


	if (chassis_behaviour_mode == CHASSIS_NO_MOVE)
	{
		chassis_no_move_control(vx_set, vy_set, wz_set, chassis_move_rc_to_vector);
	}
	else if (chassis_behaviour_mode == CHASSIS_FOLLOW_GIMBAL_YAW)
	{
		chassis_infantry_follow_gimbal_yaw_control(vx_set, vy_set, wz_set, chassis_move_rc_to_vector);
	}
	else if(chassis_behaviour_mode == CHASSIS_SPIN)
	{
		chassis_spin_control(vx_set, vy_set, wz_set, chassis_move_rc_to_vector);
	}
}

/**
  * @brief          底盘不移动的行为状态机下, 底盘模式是不跟随角度
  * @author         RM
  * @param[in]      vx_set 前进的速度, 正值 前进速度, 	负值 后退速度
  * @param[in]      vy_set 左右的速度, 正值 左移速度,   负值 右移速度
  * @param[in]      wz_set 旋转的速度, 正值 逆时针旋转, 负值 顺时针旋转
  * @param[in]      chassis_move_rc_to_vector底盘数据
  * @retval         返回空
  */
static void chassis_no_move_control(fp32 *vx_set, fp32 *vy_set, fp32 *wz_set, chassis_move_t *chassis_move_rc_to_vector)
{
    if (vx_set == NULL || vy_set == NULL || wz_set == NULL || chassis_move_rc_to_vector == NULL) return;
    *vx_set = 0.0f;
    *vy_set = 0.0f;
    *wz_set = 0.0f;
}

fp32 chassis_wz_rc_sen = CHASSIS_WZ_RC_SEN;

/**
  * @brief          底盘跟随云台的行为状态机下, 底盘模式是跟随云台角度, 底盘旋转速度会根据角度差计算底盘旋转的角速度
  * @author         RM
  * @param[in]      vx_set前进的速度, 正值 前进速度, 负值 后退速度
  * @param[in]      vy_set左右的速度, 正值 左移速度, 负值 右移速度
  * @param[in]      angle_set底盘与云台控制到的相对角度
  * @param[in]      chassis_move_rc_to_vector底盘数据
  * @retval         返回空
  */
static void chassis_infantry_follow_gimbal_yaw_control(fp32 *vx_set, fp32 *vy_set, fp32 *wz_set, chassis_move_t *chassis_move_rc_to_vector)
{
	if (vx_set == NULL || vy_set == NULL || wz_set == NULL || chassis_move_rc_to_vector == NULL) return;
	
	//根据 遥控器的通道值以及键盘按键 得出 一般情况下的速度设定值
	chassis_rc_to_control_vector(vx_set, vy_set, chassis_move_rc_to_vector);
	
	int16_t wz_channel = 0;
	rc_deadband_limit(chassis_move_rc_to_vector->chassis_RC->rc.ch[CHASSIS_WZ_CHANNEL], wz_channel, CHASSIS_RC_DEADLINE);
	
	*wz_set = -(fp32)wz_channel * chassis_wz_rc_sen;
}


/**
  * @brief          设置固定的旋转速度wz, 在根据遥控器输入设置vx和vy
  * @param[in]      vx_set 前进的速度, 正值 前进速度,	  负值 后退速度
  * @param[in]      vy_set 左右的速度, 正值 左移速度,	  负值 右移速度
  * @param[in]      wz_set 旋转速度,	 正值 逆时针旋转, 负值 顺时针旋转
  * @param[in]      chassis_move_rc_to_vector底盘数据
  * @retval         none
  */
static void chassis_spin_control(fp32 *vx_set, fp32 *vy_set, fp32 *wz_set, chassis_move_t *chassis_move_rc_to_vector)
{
	if (vx_set == NULL || vy_set == NULL || wz_set == NULL || chassis_move_rc_to_vector == NULL) return;
	
	//根据 遥控器的通道值以及键盘按键 得出 一般情况下的速度设定值 
	chassis_rc_to_control_vector(vx_set, vy_set, chassis_move_rc_to_vector);
	
	//设置角速度为 旋转速度 乘以 比例因子
	*wz_set = CHASSIS_SPIN_SPEED;

	return;
}
