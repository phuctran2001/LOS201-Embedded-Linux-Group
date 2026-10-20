/*
 * Purpose: Read sensor samples and measure timing jitter.
 * Author: Tran Thien Phuc - SE203798
 * Date: 2026-10-19
 */
#define _POSIX_C_SOURCE 200809L
#include "sensor_thread.h"
#include "sms_sensor.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

static void add_ms(struct timespec *timepoint, uint32_t milliseconds)
{
    timepoint->tv_sec += milliseconds / 1000;
    timepoint->tv_nsec += (long)(milliseconds % 1000) * 1000000L;
    if (timepoint->tv_nsec >= 1000000000L) {
        timepoint->tv_sec++;
        timepoint->tv_nsec -= 1000000000L;
    }
}

static long difference_us(const struct timespec *actual,
                          const struct timespec *planned)
{
    long value = (long)(actual->tv_sec - planned->tv_sec) * 1000000L
               + (actual->tv_nsec - planned->tv_nsec) / 1000L;
    return value < 0 ? -value : value;
}

void *sms_sensor_thread(void *argument)
{
    sms_sensor_ctx_t *ctx = argument;
    if (ctx == NULL || ctx->buffer == NULL || ctx->running == NULL ||
        ctx->interval_ms < 100)
        return NULL;

    ctx->rt_applied = false;
    ctx->samples_read = 0;
    ctx->read_errors = 0;
    ctx->jitter_count = 0;

    if (ctx->request_rt) {
        struct sched_param parameter = { .sched_priority = 50 };
        int status = pthread_setschedparam(pthread_self(), SCHED_FIFO,
                                           &parameter);
        if (status == 0)
            ctx->rt_applied = true;
        else
            fprintf(stderr, "SCHED_FIFO failed: %s\n", strerror(status));
    }

    int fd = open(SMS_DEVICE_PATH, O_RDONLY);
    if (fd < 0) {
        perror("open /dev/sms_sensor");
        ctx->read_errors++;
        return NULL;
    }

    __u32 interval = ctx->interval_ms;
    if (ioctl(fd, SMS_SET_INTERVAL, &interval) < 0) {
        perror("SMS_SET_INTERVAL");
        ctx->read_errors++;
        close(fd);
        return NULL;
    }

    struct timespec next, actual;
    if (clock_gettime(CLOCK_MONOTONIC, &next) != 0) {
        perror("clock_gettime");
        ctx->read_errors++;
        close(fd);
        return NULL;
    }

    while (atomic_load(ctx->running)) {
        add_ms(&next, ctx->interval_ms);

        int status;
        do {
            status = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,
                                     &next, NULL);
        } while (status == EINTR && atomic_load(ctx->running));

        if (!atomic_load(ctx->running))
            break;
        if (status != 0) {
            fprintf(stderr, "clock_nanosleep: %s\n", strerror(status));
            ctx->read_errors++;
            break;
        }

        if (clock_gettime(CLOCK_MONOTONIC, &actual) == 0 &&
            ctx->jitter_count < SMS_JITTER_CAPACITY)
            ctx->jitter_us[ctx->jitter_count++] =
                difference_us(&actual, &next);

        if (lseek(fd, 0, SEEK_SET) < 0) {
            perror("lseek sensor");
            ctx->read_errors++;
            break;
        }

        char line[128];
        ssize_t length = read(fd, line, sizeof(line) - 1);
        if (length < 0 && errno == EAGAIN) {
            const struct timespec pause_time = {
                .tv_sec = 0, .tv_nsec = 2000000L
            };
            for (int retry = 0; retry < 5 &&
                                atomic_load(ctx->running); retry++) {
                nanosleep(&pause_time, NULL);
                if (lseek(fd, 0, SEEK_SET) < 0) {
                    length = -1;
                    break;
                }
                length = read(fd, line, sizeof(line) - 1);
                if (length >= 0 || errno != EAGAIN)
                    break;
            }
        }
        if (length < 0) {
            if (errno == EAGAIN || errno == EINTR)
                continue;
            perror("read sensor");
            ctx->read_errors++;
            break;
        }
        if (length == 0)
            continue;

        line[length] = '\0';
        unsigned long long timestamp;
        sms_sample_t sample;
        if (sscanf(line, "%llu,%f,%f", &timestamp,
                   &sample.temperature, &sample.humidity) != 3) {
            fprintf(stderr, "Invalid sensor CSV: %s\n", line);
            ctx->read_errors++;
            continue;
        }

        sample.timestamp_ms = (uint64_t)timestamp;
        ctx->samples_read++;
        if (ctx->verbose)
            printf("[SENSOR] %llu,%.2f,%.2f\n", timestamp,
                   sample.temperature, sample.humidity);

        if (!sms_buffer_push(ctx->buffer, &sample))
            fprintf(stderr, "Sensor buffer full or closed; sample dropped\n");
    }

    close(fd);
    return NULL;
}
