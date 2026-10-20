#define _POSIX_C_SOURCE 200809L
#include "consumer_threads.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static void deadline_after_ms(struct timespec *deadline, long milliseconds)
{
    clock_gettime(CLOCK_REALTIME, deadline);
    deadline->tv_nsec += milliseconds * 1000000L;
    if (deadline->tv_nsec >= 1000000000L) {
        deadline->tv_sec++;
        deadline->tv_nsec -= 1000000000L;
    }
}

void *sms_alert_thread(void *argument)
{
    sms_alert_ctx_t *ctx = argument;
    if (ctx == NULL || ctx->running == NULL)
        return NULL;

    const char *names[] = {
        "TEMP_HIGH", "HUMID_HIGH", "TEMP_SPIKE"
    };

    for (;;) {
        sms_alarm_msg_t alarm;
        struct timespec deadline;
        deadline_after_ms(&deadline, 200);

        ssize_t received = mq_timedreceive(ctx->alarm_queue,
                                            (char *)&alarm,
                                            sizeof(alarm), NULL, &deadline);
        if (received < 0) {
            if (errno == EINTR)
                continue;
            if (errno == ETIMEDOUT && !atomic_load(ctx->running))
                break;
            if (errno == ETIMEDOUT)
                continue;
            perror("mq_timedreceive alarm");
            break;
        }

        if (received != (ssize_t)sizeof(alarm) ||
            alarm.type < SMS_ALERT_TEMP_HIGH ||
            alarm.type > SMS_ALERT_TEMP_SPIKE)
            continue;

        ctx->counts[alarm.type]++;
        printf("[ALERT][%llu] %s value=%.2f threshold=%.2f\n",
               (unsigned long long)alarm.timestamp_ms,
               names[alarm.type], alarm.value, alarm.threshold);

        sms_log_msg_t log_message = {0};
        snprintf(log_message.text, sizeof(log_message.text),
                 "[ALERT][%llu] %s value=%.2f threshold=%.2f",
                 (unsigned long long)alarm.timestamp_ms,
                 names[alarm.type], alarm.value, alarm.threshold);

        deadline_after_ms(&deadline, 100);
        if (mq_timedsend(ctx->log_queue, (const char *)&log_message,
                         sizeof(log_message), 0, &deadline) != 0)
            ctx->log_errors++;
    }

    return NULL;
}
