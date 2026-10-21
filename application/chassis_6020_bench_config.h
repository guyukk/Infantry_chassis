#ifndef CHASSIS_6020_BENCH_CONFIG_H
#define CHASSIS_6020_BENCH_CONFIG_H

/* Explicit bench firmware switch. Set to 0 before building normal chassis firmware. */
#define CHASSIS_6020_BENCH_TEST 1

/* 1: one short open-loop pulse; 2: DBUS-controlled closed-loop angle sweep. */
#define CHASSIS_6020_BENCH_MODE 2

/* Both DBUS switches UP arm the test after an observed disarmed state. */
#define CHASSIS_6020_BENCH_AXIS 0
#define CHASSIS_6020_BENCH_RC_DEADBAND 25
#define CHASSIS_6020_BENCH_RC_TIMEOUT_MS 100U
#define CHASSIS_6020_BENCH_FEEDBACK_TIMEOUT_MS 100U
#define CHASSIS_6020_BENCH_SWEEP_RAD_PER_SEC 0.50f
#define CHASSIS_6020_BENCH_CURRENT_LIMIT 1500
#define CHASSIS_6020_BENCH_PULSE_CURRENT 300
#define CHASSIS_6020_BENCH_PULSE_MS 150U
#define CHASSIS_6020_BENCH_TRACE_INTERVAL_MS 10U

#if CHASSIS_6020_BENCH_MODE != 1 && CHASSIS_6020_BENCH_MODE != 2
#error Invalid CHASSIS_6020_BENCH_MODE
#endif
#if CHASSIS_6020_BENCH_PULSE_CURRENT > CHASSIS_6020_BENCH_CURRENT_LIMIT
#error Pulse current exceeds bench limit
#endif

#endif
