#ifndef SMS_SENSOR_THREAD_H
#define SMS_SENSOR_THREAD_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "circular_buffer.h"

#define SMS_JITTER_CAPACITY 4096

typedef struct {
    sms_buffer_t *buffer;
    atomic_bool *running;
    uint32_t interval_ms;
    bool verbose;
    bool request_rt;
    bool rt_applied;
    uint64_t samples_read;
    uint64_t read_errors;
    long jitter_us[SMS_JITTER_CAPACITY];
    size_t jitter_count;
} sms_sensor_ctx_t;

void *sms_sensor_thread(void *argument);

#endif
