#ifndef CHASSIS_SOFT_TURN_H
#define CHASSIS_SOFT_TURN_H

#include "struct_typedef.h"

#define CHASSIS_SOFT_TURN_WHEELS 4U

typedef struct
{
    fp32 vx, vy, wz; /* body frame: m/s, m/s, rad/s */
} chassis_twist_t;

typedef struct
{
    fp32 world_vx, world_vy, wz;
    fp32 angle[CHASSIS_SOFT_TURN_WHEELS];
    fp32 speed[CHASSIS_SOFT_TURN_WHEELS];
    fp32 desired_angle[CHASSIS_SOFT_TURN_WHEELS];
    fp32 angle_error[CHASSIS_SOFT_TURN_WHEELS];
    fp32 wheel_scale, still_time;
    uint8_t reversed[CHASSIS_SOFT_TURN_WHEELS];
    uint8_t branch_valid[CHASSIS_SOFT_TURN_WHEELS];
    uint8_t braking, holding;
} chassis_soft_turn_t;

uint8_t chassis_soft_turn_finite(fp32 value);
fp32 chassis_soft_turn_dt(fp32 elapsed_seconds);
fp32 chassis_soft_turn_wrap(fp32 angle);
void chassis_soft_turn_reset(chassis_soft_turn_t *state, const fp32 measured_angle[4]);
/* Return 0 on invalid input. Caller must inhibit currents and reset the PID state. */
uint8_t chassis_soft_turn_shape_twist(chassis_soft_turn_t *state,
    const chassis_twist_t *target, fp32 yaw, fp32 dt, uint8_t fast_stop,
    chassis_twist_t *output);
/* Inverse kinematics supplies raw, unoptimized module targets. */
uint8_t chassis_soft_turn_shape_wheels(chassis_soft_turn_t *state,
    const fp32 raw_angle[4], const fp32 raw_speed[4],
    const fp32 measured_angle[4], const fp32 measured_speed[4],
    fp32 dt, uint8_t fast_stop);

#endif
