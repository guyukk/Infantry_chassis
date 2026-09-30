/**
  * @file       slip_control.h
  * @brief     	打滑控制
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
	
#include "slip_control.h"
#include "INS_task.h"
#include "chassis_task.h"
#include "kalman_filter.h"
#include "cmsis_os.h"
#include "arm_math.h"
#include "pid.h"
#include <math.h>

/************************************
后期维护需要调整的参数

1.卡尔曼滤波参数(最重要)
	过程噪声协方差 Q_data[9] ―― 需要根据实际系统调整
	观测噪声协方差 R_kf_data[9] ―― 需要根据传感器精度调整

2.滑移阈值参数chassis_slip_threshold ―― 需要实际测试调整

3.PID参数pid_params[3] ―― KP、KI、KD


建议按以下顺序调试:

观察 predicted_speed 是否合理;
调整 Q 和 R 参数使滤波平滑;
观察 diffs 值变化, 调整 chassis_slip_threshold;
调试PID;
************************************/

/* 卡尔曼滤波器实例 */
KalmanFilter_t chassis_speed_kalman;

/* 打滑判定变量 */
fp32 chassis_slip_threshold = 0.10f;

/* PID控制器实例 */
pid_type_def slip_pid;

/* ARM数学库矩阵实例 */
arm_matrix_instance_f32 R;      // 旋转矩阵

arm_matrix_instance_f32 H;      // 观测矩阵
arm_matrix_instance_f32 F;      // 状态转移矩阵
arm_matrix_instance_f32 B;      // 控制矩阵
arm_matrix_instance_f32 Q;      // 过程噪声协方差
arm_matrix_instance_f32 R_kf;   // 观测噪声协方差
arm_matrix_instance_f32 P;      // 估计误差协方差

/* 静态函数声明 */
static void quaternion_to_rotation_matrix(const fp32* q, arm_matrix_instance_f32 *R);
static void init_arm_matrices(void);
static fp32 calc_time_diff(uint32_t new_tick, uint32_t old_tick);

//pid参数
fp32 pid_params[3]={0.8f, 0.0f, 0.05f};

/**
  * @brief 初始化速度预测滤波器
  */
void init_speed_predictor(void)
{
	static uint8_t init_flag = 0;
	if(!init_flag) {
		/* 初始化ARM矩阵 */
		init_arm_matrices();
		
		/* 初始化卡尔曼滤波器 */
		Kalman_Filter_Init(&chassis_speed_kalman, 3, 3, 3);
		
		/* 设置矩阵指针 */
		chassis_speed_kalman.F_data = F.pData;
		chassis_speed_kalman.B_data = B.pData;
		chassis_speed_kalman.H_data = H.pData;
		chassis_speed_kalman.Q_data = Q.pData;
		chassis_speed_kalman.R_data = R_kf.pData;
		chassis_speed_kalman.P_data = P.pData;
		
		init_flag = 1;
		PID_init(&slip_pid, PID_USUAL, pid_params, 0.3f, 0.05f); // 设置最大输出和积分项限幅
	}
}

/**
  * @brief 获取融合后的底盘速度
  */
void get_INS_speed(fp32* vx, fp32* vy, fp32* wz)
{
    static uint32_t last_tick = 0;
    static fp32 integrated_vx = 0.0f;
    static fp32 integrated_vy = 0.0f;
    
    /* 时间差计算 */
    uint32_t current_tick = osKernelSysTick();
    fp32 dt = calc_time_diff(current_tick, last_tick);
    last_tick = current_tick;

    /* 获取传感器数据 */
    const fp32* gyro = get_gyro_data_point();
    const fp32* accel = get_accel_data_point();
    const fp32* quat = get_INS_quat_point();
    
    /* 更新旋转矩阵 */
    quaternion_to_rotation_matrix(quat, &R);
    
    /* 计算重力分量 */
    float32_t gravity_world[3] = {0, 0, 9.81f};
    float32_t gravity_body[3] = {0};
    arm_matrix_instance_f32 gravity_world_mat = {3, 1, gravity_world};
    arm_matrix_instance_f32 gravity_body_mat = {3, 1, gravity_body};
    
    arm_mat_mult_f32(&R, &gravity_world_mat, &gravity_body_mat);

    /* 计算线性加速度 */
    float32_t linear_accel[3] = {
        accel[0] - gravity_body[0],
        accel[1] - gravity_body[1],
        accel[2] - gravity_body[2]
    };
    
    /* 速度积分 */
    integrated_vx += linear_accel[0] * dt;
    integrated_vy += linear_accel[1] * dt;
    
    /* 输出结果 */
    *vx = integrated_vx;
    *vy = integrated_vy;
    *wz = gyro[INS_GYRO_Z_ADDRESS_OFFSET];
}

/* 定义变量 */
fp32 vx_wheel, vy_wheel, wz_wheel;
fp32 vx_ins, vy_ins, wz_ins;
fp32 predicted_speed[3];
float32_t pid_output_x, pid_output_y, pid_output_z;
float32_t diffs[3];	 //滑移检测

/**
  * @brief 滑移控制主函数, 用于检测是否会打滑, 限制vx_set、vy_set、wz_set三个设定量
  */
void slip_control(chassis_move_t* chassis)
{
	/* 初始化检查 */
	static uint8_t init_flag = 0;
	if(!init_flag) {
		init_speed_predictor();
		init_flag = 1;
	}
	
	get_INS_speed(&vx_ins, &vy_ins, &wz_ins);
	
	//获取转底盘速度, 通过 chassis_feedback_update() 函数里的 chas_for_cal() 函数实现
	vx_wheel = chassis->vx;
	vy_wheel = chassis->vy;
	wz_wheel = chassis->wz;
	
	/* 卡尔曼预测 */
//  float32_t u[3] = {chassis->vx_set * 0.1f, chassis->vy_set * 0.1f, chassis->wz_set * 0.1f};
	float32_t u[3] = {chassis->vx_set, chassis->vy_set, chassis->wz_set};
	float32_t z[3] = {vx_wheel, vy_wheel, wz_wheel};
	
	arm_copy_f32(u, chassis_speed_kalman.ControlVector, 3);
	arm_copy_f32(z, chassis_speed_kalman.MeasuredVector, 3);
	
	Kalman_Filter_Update(&chassis_speed_kalman);
	arm_copy_f32(chassis_speed_kalman.xhat_data, predicted_speed, 3);
	
	arm_sub_f32(predicted_speed, (float32_t[]){vx_ins, vy_ins, wz_ins}, diffs, 3);
	arm_abs_f32(diffs, diffs, 3);
		
	/* 分别计算各轴的滑移并响应 */
	for(int i = 0; i < 3; ++i)
	{
		if(diffs[i] > chassis_slip_threshold)
		{
			switch(i)
			{
				case 0: // X轴滑移处理
					pid_output_x = - PID_calc(&slip_pid, predicted_speed[0], chassis->vx_set);
					chassis->vx_set *= pid_output_x;
					break;
				case 1: // Y轴滑移处理
					pid_output_y = - PID_calc(&slip_pid, predicted_speed[1], chassis->vy_set);
					chassis->vy_set *= pid_output_y;
					break;
				case 2: // Z轴滑移处理
					pid_output_z = - PID_calc(&slip_pid, predicted_speed[2], chassis->wz_set);
					chassis->wz_set *= pid_output_z;
					break;
			}
		}
	}
}

/******************** ARM数学库辅助函数 ********************/

/**
  * @brief          将四元数转换为旋转矩阵
	* @param[in]	    q 四元数
	* @param[out]     R	arm_matrix_instance_f32 结构体指针
  * @retval         none
  */
static void quaternion_to_rotation_matrix(const fp32* q, arm_matrix_instance_f32 *R)
{
	/* 四元数转旋转矩阵 */
	float32_t q0 = q[0], q1 = q[1], q2 = q[2], q3 = q[3];
	
	float32_t* pData = R->pData;
	pData[0] = 1 - 2*q2*q2 - 2*q3*q3;
	pData[1] = 2*q1*q2 - 2*q0*q3;
	pData[2] = 2*q1*q3 + 2*q0*q2;
	
	pData[3] = 2*q1*q2 + 2*q0*q3;
	pData[4] = 1 - 2*q1*q1 - 2*q3*q3;
	pData[5] = 2*q2*q3 - 2*q0*q1;
	
	pData[6] = 2*q1*q3 - 2*q0*q2;
	pData[7] = 2*q2*q3 + 2*q0*q1;
	pData[8] = 1 - 2*q1*q1 - 2*q2*q2;
}

/**
  * @brief          为每个矩阵分配静态内存, 初始化每个矩阵的数值, 调用 arm_mat_init_f32() 绑定矩阵结构与数据数组
  */
static void init_arm_matrices(void)
{
	/* 预分配矩阵内存 */
	static float32_t R_data[9] = {0};
	static float32_t F_data[9] = {1,0,0, 0,1,0, 0,0,1};
	static float32_t B_data[9] = {1,0,0, 0,1,0, 0,0,1};
	static float32_t H_data[9] = {1,0,0, 0,1,0, 0,0,1};
//  static float32_t Q_data[9] = {0.01,0,0, 0,0.01,0, 0,0,0.01};
	static float32_t Q_data[9] = {0.04,0,0, 0,0.04,0, 0,0,0.04};
//  static float32_t R_kf_data[9] = {0.1,0,0, 0,0.1,0, 0,0,0.1};
	static float32_t R_kf_data[9] = {0.4,0,0, 0,0.4,0, 0,0,0.4};
	static float32_t P_data[9] = {1,0,0, 0,1,0, 0,0,1};
	
	arm_mat_init_f32(&R, 3, 3, R_data);
	arm_mat_init_f32(&F, 3, 3, F_data);
	arm_mat_init_f32(&B, 3, 3, B_data);
	arm_mat_init_f32(&H, 3, 3, H_data);
	arm_mat_init_f32(&Q, 3, 3, Q_data);
	arm_mat_init_f32(&R_kf, 3, 3, R_kf_data);
	arm_mat_init_f32(&P, 3, 3, P_data);
}

/**
  * @brief          确定卡尔曼滤波的时间间隔 dt
  */
static fp32 calc_time_diff(uint32_t new_tick, uint32_t old_tick)
{
	const uint32_t TICK_MAX = 0xFFFFFFFF;
	
	uint32_t diff = (new_tick >= old_tick) ? 
									(new_tick -  old_tick) :
									(TICK_MAX -  old_tick + new_tick);
	
	return diff * (1.0f / 168000000.0f); // 168mHz时钟
}
