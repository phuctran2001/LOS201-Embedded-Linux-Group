/*
 * Purpose: Calculate rolling averages and detect alerts.
 * Author: Tran Thien Phuc - SE203798
 * Date: 2026-10-19
 */
#define _POSIX_C_SOURCE 200809L
#include "processor_thread.h"

#include <math.h>
#include <stdio.h>
#include <time.h>

#define WINDOW_SIZE 10

static int send_with_timeout(mqd_t queue, const void *message, size_t size)
{
    struct timespec deadline;
    if (clock_gettime(CLOCK_REALTIME, &deadline) != 0)
        return -1;

    deadline.tv_nsec += 100000000L;
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec++;
        deadline.tv_nsec -= 1000000000L;
    }

    return mq_timedsend(queue, message, size, 0, &deadline);
}

static void send_alarm(sms_processor_ctx_t *ctx, const sms_sample_t *sample,
                       sms_alert_type_t type, float value, float threshold)
{
    sms_alarm_msg_t alarm = {
        .timestamp_ms = sample->timestamp_ms,
        .type = type,
        .value = value,
        .threshold = threshold
    };

    if (send_with_timeout(ctx->alarm_queue, &alarm, sizeof(alarm)) == 0)
        ctx->alerts_sent[type]++;
    else
        ctx->messages_dropped++;
}

void *sms_processor_thread(void *argument)
{
    sms_processor_ctx_t *ctx = argument;
    if (ctx == NULL || ctx->buffer == NULL)
        return NULL;

    float temperatures[WINDOW_SIZE] = {0};
    float humidities[WINDOW_SIZE] = {0};
    size_t position = 0;
    size_t window_count = 0;
    float previous_temperature = 0.0f;
    bool have_previous = false;
    sms_sample_t sample;

    while (sms_buffer_pop(ctx->buffer, &sample)) {
        temperatures[position] = sample.temperature;
        humidities[position] = sample.humidity;
        position = (position + 1) % WINDOW_SIZE;
        if (window_count < WINDOW_SIZE)
            window_count++;

        float total_temperature = 0.0f;
        float total_humidity = 0.0f;
        for (size_t i = 0; i < window_count; i++) {
            total_temperature += temperatures[i];
            total_humidity += humidities[i];
        }

        float average_temperature = total_temperature / window_count;
        float average_humidity = total_humidity / window_count;
        ctx->processed++;

        sms_log_msg_t log_message = {0};
        snprintf(log_message.text, sizeof(log_message.text),
                 "[DATA][%llu] T=%.2fC H=%.2f%% AvgT=%.2fC AvgH=%.2f%%",
                 (unsigned long long)sample.timestamp_ms,
                 sample.temperature, sample.humidity,
                 average_temperature, average_humidity);

        if (send_with_timeout(ctx->log_queue, &log_message,
                              sizeof(log_message)) != 0)
            ctx->messages_dropped++;

        if (sample.temperature > ctx->temp_threshold)
            send_alarm(ctx, &sample, SMS_ALERT_TEMP_HIGH,
                       sample.temperature, ctx->temp_threshold);

        if (sample.humidity > ctx->humid_threshold)
            send_alarm(ctx, &sample, SMS_ALERT_HUMID_HIGH,
                       sample.humidity, ctx->humid_threshold);

        if (have_previous) {
            float change = fabsf(sample.temperature - previous_temperature);
            if (change > 5.0f)
                send_alarm(ctx, &sample, SMS_ALERT_TEMP_SPIKE, change, 5.0f);
        }

        previous_temperature = sample.temperature;
        have_previous = true;
    }

    return NULL;
}
