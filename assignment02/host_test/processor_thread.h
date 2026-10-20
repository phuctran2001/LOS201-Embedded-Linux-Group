#ifndef SMS_PROCESSOR_THREAD_H
#define SMS_PROCESSOR_THREAD_H

#include <mqueue.h>
#include <stdint.h>
#include "circular_buffer.h"
#include "sms_messages.h"

typedef struct {
    sms_buffer_t *buffer;
    mqd_t alarm_queue;
    mqd_t log_queue;
    float temp_threshold;
    float humid_threshold;
    uint64_t processed;
    uint64_t alerts_sent[3];
    uint64_t messages_dropped;
} sms_processor_ctx_t;

void *sms_processor_thread(void *argument);

#endif
