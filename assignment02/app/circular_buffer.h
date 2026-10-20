#ifndef SMS_CIRCULAR_BUFFER_H
#define SMS_CIRCULAR_BUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>

#define SMS_BUFFER_CAPACITY 64

typedef struct {
    uint64_t timestamp_ms;
    float temperature;
    float humidity;
} sms_sample_t;

typedef struct {
    sms_sample_t samples[SMS_BUFFER_CAPACITY];
    size_t head, tail, count;
    uint64_t dropped;
    uint64_t produced, consumed;
    bool closed;
    pthread_mutex_t mutex;
    pthread_cond_t nonempty;
} sms_buffer_t;

typedef struct {
    size_t current_size;
    uint64_t total_produced, total_consumed, overflow_count;
} sms_buffer_stats_t;

int sms_buffer_init(sms_buffer_t *buffer);
bool sms_buffer_push(sms_buffer_t *buffer, const sms_sample_t *sample);
bool sms_buffer_pop(sms_buffer_t *buffer, sms_sample_t *sample);
void sms_buffer_close(sms_buffer_t *buffer);
void sms_buffer_destroy(sms_buffer_t *buffer);
bool sms_buffer_get_stats(sms_buffer_t *buffer, sms_buffer_stats_t *stats);

#endif
