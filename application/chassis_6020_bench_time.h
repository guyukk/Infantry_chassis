#ifndef CHASSIS_6020_BENCH_TIME_H
#define CHASSIS_6020_BENCH_TIME_H

#include <stdint.h>

static uint8_t bench_6020_time_fresh(uint32_t now, uint32_t last,
	uint8_t seen, uint32_t timeout_ms)
{
	return seen && ((uint32_t)(now - last) <= timeout_ms);
}

#endif
