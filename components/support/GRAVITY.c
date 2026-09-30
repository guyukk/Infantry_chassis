#include "GRAVITY.h"
#include "arm_math.h"
#include <math.h>
#include "float.h"
#define MAX_ITERATIONS 10       // 最大迭代次数优化
#define CONVERGENCE_THRESHOLD 0.0001f  // 收敛阈值提升
#define MIN_DISTANCE 0.1f       // 最小有效距离

BallisticParams params;

//装定弹道参数
void init_BallisticParams(BallisticParams* params){
		
//    params->k = 0.5f * AIR_DENSITY * PROJECTILE_MASS; // 更精确的弹道系数计算
		params->k = 0.092; //来源华南师大Vanguard战队，他们为RM_VISION做的适配，deepseek给出0.0402和以上的公式
//		params->k = 0.0402;
    params->s_bias = 0.15f;
    params->z_bias = 0.21f;
}

// 改进的弹道模型（考虑二次空气阻力）
float advancedAirResistanceModel(float s, float v, float angle, BallisticParams params) {
    const float cos_theta = arm_cos_f32(angle);
    const float sin_theta = arm_sin_f32(angle);
    
    // 使用龙格-库塔法进行微分方程数值解
    float t = 0.0f;
    float dt = 0.001f;
    float x = 0.0f, z = 0.0f;
    float vx = v * cos_theta;
    float vz = v * sin_theta;
    
    for(int i=0; i<10000 && x<s; i++){
        const float velocity = sqrtf(vx*vx + vz*vz);
        const float drag = params.k * velocity;
        
        // x方向运动
        vx -= drag * vx * dt;
        x += vx * dt;
        
        // z方向运动
        vz -= (GRAVITY + drag * vz) * dt;
        z += vz * dt;
        
        t += dt;
        
        // 提前退出条件
        if(x > s || z < -10.0f) break;
    }
    return z;
}

// 优化迭代算法（牛顿-拉夫逊法）
float optimizedPitchCompensation(float s, float z_target, float v, BallisticParams params) {
    float angle = atan2f(z_target, s);  // 初始估计
    float delta;
    float best_angle = angle;
    float min_error = FLT_MAX;
    
    for(int i=0; i<MAX_ITERATIONS; i++){
        const float z_actual = advancedAirResistanceModel(s, v, angle, params);
        const float error = z_target - z_actual;
        
        // 计算数值导数
        const float delta_angle = 0.001f;
        const float z_deriv = advancedAirResistanceModel(s, v, angle + delta_angle, params);
        const float derivative = (z_deriv - z_actual) / delta_angle;
        
        // 牛顿迭代步
        if(fabsf(derivative) > 1e-6f) {
            delta = error / derivative;
            angle += delta;
        }
        
        // 记录最佳解
        if(fabsf(error) < min_error){
            min_error = fabsf(error);
            best_angle = angle;
        }
        
        // 收敛检查
        if(fabsf(delta) < CONVERGENCE_THRESHOLD) break;
    }
    return best_angle;
}

float calculateCompensationAngle(float shoot_angle, float distance, float initial_velocity, BallisticParams params) {
    // 输入校验
    if(distance < MIN_DISTANCE) return shoot_angle;
    
    // 坐标转换（考虑枪口偏移）
    const float s = distance * cosf(shoot_angle) + params.s_bias;
    const float z = distance * sinf(shoot_angle) + params.z_bias;
    
    // 使用优化后的弹道模型
    return optimizedPitchCompensation(s, z, initial_velocity, params);
}
