#ifndef CHASSIS_6020_BENCH_LOGIC_H
#define CHASSIS_6020_BENCH_LOGIC_H

#include <stdint.h>

/* Pure helpers are shared by the firmware and the ARMCC/Unicorn regression check. */
static uint8_t bench_6020_healthy(uint8_t rc_ok, uint8_t motors_ok,
	uint8_t switches_up, uint8_t tx_ok)
{
	return rc_ok && motors_ok && switches_up && tx_ok;
}

static uint8_t bench_6020_can_arm(uint8_t healthy, uint8_t release_seen,
	int16_t axis, int16_t deadband)
{
	return healthy && release_seen && axis >= -deadband && axis <= deadband;
}

static float bench_6020_shortest_set(float measured, float target, float pi)
{
	float error = target - measured;
	while (error > pi)
	{
		error -= 2.0f * pi;
	}
	while (error < -pi)
	{
		error += 2.0f * pi;
	}
	return measured + error;
}

static int16_t bench_6020_limit_current(float output, int16_t limit)
{
	if (output != output)
	{
		return 0;
	}
	if (output > limit)
	{
		return limit;
	}
	if (output < -limit)
	{
		return -limit;
	}
	return (int16_t)output;
}

#endif
