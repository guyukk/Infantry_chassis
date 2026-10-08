#include "chassis_soft_turn.h"
#include "robot_param.h"
#include <float.h>
#include <math.h>
#include <string.h>

#define SOFT_PI 3.14159265358979323846f
#define SOFT_ZERO 0.000001f

uint8_t chassis_soft_turn_finite(fp32 value)
{
    return (value <= FLT_MAX && value >= -FLT_MAX);
}

fp32 chassis_soft_turn_dt(fp32 elapsed_seconds)
{
    if (!chassis_soft_turn_finite(elapsed_seconds) || elapsed_seconds < 0.001f)
        return 0.001f;
    if (elapsed_seconds > 0.005f) return 0.005f;
    return elapsed_seconds;
}

fp32 chassis_soft_turn_wrap(fp32 angle)
{
    if (!chassis_soft_turn_finite(angle)) return 0.0f;
    angle = fmodf(angle, 2.0f * SOFT_PI);
    if (angle > SOFT_PI) angle -= 2.0f * SOFT_PI;
    if (angle < -SOFT_PI) angle += 2.0f * SOFT_PI;
    return angle;
}

static fp32 approach(fp32 previous, fp32 target, fp32 step)
{
    fp32 difference = target - previous;
    if (difference > step) return previous + step;
    if (difference < -step) return previous - step;
    return target;
}

void chassis_soft_turn_reset(chassis_soft_turn_t *state, const fp32 measured_angle[4])
{
    uint8_t i;
    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
    state->wheel_scale = 1.0f;
    for (i = 0; i < CHASSIS_SOFT_TURN_WHEELS; ++i)
    {
        state->angle[i] = measured_angle == NULL ? 0.0f :
            chassis_soft_turn_wrap(measured_angle[i]);
        state->desired_angle[i] = state->angle[i];
    }
}

uint8_t chassis_soft_turn_shape_twist(chassis_soft_turn_t *state,
    const chassis_twist_t *target, fp32 yaw, fp32 dt, uint8_t fast_stop,
    chassis_twist_t *output)
{
    fp32 vx, vy, magnitude, cosine, sine, dx, dy, step, scale;
    if (state == NULL || target == NULL || output == NULL ||
        !chassis_soft_turn_finite(target->vx) || !chassis_soft_turn_finite(target->vy) ||
        !chassis_soft_turn_finite(target->wz) || !chassis_soft_turn_finite(yaw)) return 0U;
    dt = chassis_soft_turn_dt(dt);
    if (fast_stop)
    {
        /* Wheel braking is independent of the ordinary command ramp. */
        state->world_vx = state->world_vy = state->wz = 0.0f;
        output->vx = output->vy = output->wz = 0.0f;
        return 1U;
    }
    vx = target->vx;
    vy = target->vy;
    magnitude = sqrtf(vx * vx + vy * vy);
    if (!chassis_soft_turn_finite(magnitude)) return 0U;
    if (magnitude > CHASSIS_SOFT_MAX_TRANSLATION)
    {
        scale = CHASSIS_SOFT_MAX_TRANSLATION / magnitude;
        vx *= scale;
        vy *= scale;
    }
    yaw = chassis_soft_turn_wrap(yaw);
    cosine = cosf(yaw);
    sine = sinf(yaw);
    dx = cosine * vx - sine * vy - state->world_vx;
    dy = sine * vx + cosine * vy - state->world_vy;
    magnitude = sqrtf(dx * dx + dy * dy);
    step = ((vx == 0.0f && vy == 0.0f) ? CHASSIS_SOFT_RELEASE_DECEL :
        CHASSIS_SOFT_VECTOR_ACCEL) * dt;
    if (magnitude > step)
    {
        scale = step / magnitude;
        dx *= scale;
        dy *= scale;
    }
    state->world_vx += dx;
    state->world_vy += dy;
    state->wz = approach(state->wz, target->wz, CHASSIS_SOFT_YAW_ACCEL * dt);
    output->vx = cosine * state->world_vx + sine * state->world_vy;
    output->vy = -sine * state->world_vx + cosine * state->world_vy;
    output->wz = state->wz;
    return 1U;
}

uint8_t chassis_soft_turn_shape_wheels(chassis_soft_turn_t *state,
    const fp32 raw_angle[4], const fp32 raw_speed[4],
    const fp32 measured_angle[4], const fp32 measured_speed[4],
    fp32 dt, uint8_t fast_stop)
{
    fp32 maximum = 0.0f, angle, speed, error, factor, delta;
    uint8_t i, stopped = 1U;
    if (state == NULL || raw_angle == NULL || raw_speed == NULL ||
        measured_angle == NULL || measured_speed == NULL) return 0U;
    for (i = 0; i < CHASSIS_SOFT_TURN_WHEELS; ++i)
    {
        if (!chassis_soft_turn_finite(raw_angle[i]) ||
            !chassis_soft_turn_finite(raw_speed[i]) ||
            !chassis_soft_turn_finite(measured_angle[i]) ||
            !chassis_soft_turn_finite(measured_speed[i])) return 0U;
        if (fabsf(raw_speed[i]) > maximum) maximum = fabsf(raw_speed[i]);
    }
    dt = chassis_soft_turn_dt(dt);
    state->wheel_scale = maximum > CHASSIS_SOFT_MAX_WHEEL_SPEED ?
        CHASSIS_SOFT_MAX_WHEEL_SPEED / maximum : 1.0f;
    if (fast_stop && !state->braking)
    {
        /* Capture feedback once; never steer while explicitly braking. */
        for (i = 0; i < CHASSIS_SOFT_TURN_WHEELS; ++i)
        {
            state->angle[i] = chassis_soft_turn_wrap(measured_angle[i]);
            state->desired_angle[i] = state->angle[i];
        }
        state->still_time = 0.0f;
    }
    state->braking = fast_stop;
    for (i = 0; i < CHASSIS_SOFT_TURN_WHEELS; ++i)
    {
        speed = raw_speed[i] * state->wheel_scale;
        if (fast_stop) speed = 0.0f;
        else if (fabsf(speed) > SOFT_ZERO)
        {
            state->holding = 0U;
            angle = chassis_soft_turn_wrap(raw_angle[i]);
            error = fabsf(chassis_soft_turn_wrap(angle - measured_angle[i]));
            if (!state->branch_valid[i])
            {
                state->reversed[i] = error > SOFT_PI * 0.5f;
                state->branch_valid[i] = 1U;
            }
            else if (!state->reversed[i] && error > SOFT_PI * 0.5f + CHASSIS_SOFT_FLIP_HYSTERESIS)
                state->reversed[i] = 1U;
            else if (state->reversed[i] && error < SOFT_PI * 0.5f - CHASSIS_SOFT_FLIP_HYSTERESIS)
                state->reversed[i] = 0U;
            if (state->reversed[i])
            {
                angle = chassis_soft_turn_wrap(angle + SOFT_PI);
                speed = -speed;
            }
            state->desired_angle[i] = angle;
            error = chassis_soft_turn_wrap(angle - measured_angle[i]);
            factor = cosf(error);
            if (factor < 0.0f) factor = 0.0f;
            speed *= factor;
            delta = chassis_soft_turn_wrap(angle - state->angle[i]);
            delta = approach(0.0f, delta, CHASSIS_SOFT_STEER_RATE * dt);
            state->angle[i] = chassis_soft_turn_wrap(state->angle[i] + delta);
        }
        else speed = 0.0f; /* atan2(0, 0) must not change a stopped wheel. */
        state->angle_error[i] = chassis_soft_turn_wrap(state->desired_angle[i] - measured_angle[i]);
        /* Reach an explicit zero setpoint before reversing the drive motor. */
        if (state->speed[i] * speed < 0.0f) speed = 0.0f;
        state->speed[i] = approach(state->speed[i], speed,
            (fast_stop ? CHASSIS_SOFT_FAST_DECEL : CHASSIS_SOFT_WHEEL_ACCEL) * dt);
        if (fabsf(measured_speed[i]) >= CHASSIS_SOFT_STOP_SPEED ||
            state->speed[i] != 0.0f) stopped = 0U;
    }
    /* A zero request plus measured standstill is required for parking. */
    if ((fast_stop || maximum <= SOFT_ZERO) && stopped)
    {
        state->still_time += dt;
        if (state->still_time >= CHASSIS_SOFT_STOP_TIME) state->holding = 1U;
    }
    else
    {
        state->still_time = 0.0f;
        state->holding = 0U;
    }
    return 1U;
}
