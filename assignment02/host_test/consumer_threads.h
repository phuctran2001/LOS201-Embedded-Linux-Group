#ifndef SMS_CONSUMER_THREADS_H
#define SMS_CONSUMER_THREADS_H

#include <mqueue.h>
#include <stdatomic.h>
#include <stdint.h>
#include "sms_messages.h"

typedef struct {
    mqd_t alarm_queue;
    mqd_t log_queue;
    atomic_bool *running;
    uint64_t counts[3];
    uint64_t log_errors;
} sms_alert_ctx_t;

typedef struct {
    mqd_t log_queue;
    atomic_bool *running;
    char path[256];
    uint64_t written;
    uint64_t rotations;
} sms_logger_ctx_t;

void *sms_alert_thread(void *argument);
void *sms_logger_thread(void *argument);

#endif
