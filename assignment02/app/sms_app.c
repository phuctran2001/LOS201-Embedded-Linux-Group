/*
 * Purpose: Start threads, parse options, coordinate shutdown and print statistics.
 * Author: Tran Thien Phuc - SE203798
 * Date: 2026-10-19
 */
#define _POSIX_C_SOURCE 200809L
#include "sensor_thread.h"
#include "processor_thread.h"
#include "consumer_threads.h"
#include "sms_messages.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t stop_requested = 0;

static void handle_stop(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
}

static int compare_long(const void *left, const void *right)
{
    long a = *(const long *)left;
    long b = *(const long *)right;
    return (a > b) - (a < b);
}

static void print_jitter(const sms_sensor_ctx_t *sensor)
{
    size_t n = sensor->jitter_count;
    if (n == 0) {
        puts("Jitter: no samples");
        return;
    }

    long sorted[SMS_JITTER_CAPACITY];
    double total = 0.0;
    size_t bins[4] = {0};

    for (size_t i = 0; i < n; i++) {
        long value = sensor->jitter_us[i];
        sorted[i] = value;
        total += value;
        if (value < 100) bins[0]++;
        else if (value < 500) bins[1]++;
        else if (value < 1000) bins[2]++;
        else bins[3]++;
    }

    qsort(sorted, n, sizeof(sorted[0]), compare_long);
    size_t p99_index = (99 * n + 99) / 100 - 1;

    printf("Jitter policy: %s; samples: %zu\n",
           sensor->rt_applied ? "SCHED_FIFO" : "SCHED_OTHER", n);
    printf("Jitter Min/Avg/Max/P99 (us): %ld / %.2f / %ld / %ld\n",
           sorted[0], total / n, sorted[n - 1], sorted[p99_index]);
    printf("Histogram [0,100):%zu [100,500):%zu [500,1000):%zu [1000,+):%zu\n",
           bins[0], bins[1], bins[2], bins[3]);
}

static void usage(const char *program)
{
    fprintf(stderr,
            "Usage: %s [-i interval_ms] [-t temp_C] [-h humidity_pct] "
            "[-d seconds] [-v] [-r]\n", program);
}

int main(int argc, char **argv)
{
    unsigned interval_ms = 500;
    float temp_threshold = 40.0f;
    float humid_threshold = 80.0f;
    int duration_seconds = 60;
    bool verbose = false;
    bool request_rt = false;

    int option;
    while ((option = getopt(argc, argv, "i:t:h:d:vr")) != -1) {
        switch (option) {
        case 'i': interval_ms = (unsigned)strtoul(optarg, NULL, 10); break;
        case 't': temp_threshold = strtof(optarg, NULL); break;
        case 'h': humid_threshold = strtof(optarg, NULL); break;
        case 'd': duration_seconds = atoi(optarg); break;
        case 'v': verbose = true; break;
        case 'r': request_rt = true; break;
        default: usage(argv[0]); return 2;
        }
    }

    if (interval_ms < 100 || interval_ms > 5000 ||
        duration_seconds < 0 || optind != argc) {
        usage(argv[0]);
        return 2;
    }

    struct sigaction action = {0};
    action.sa_handler = handle_stop;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) != 0 ||
        sigaction(SIGTERM, &action, NULL) != 0) {
        perror("sigaction");
        return 1;
    }

    sms_buffer_t *buffer = malloc(sizeof(*buffer));
    if (buffer == NULL) {
        perror("malloc buffer");
        return 1;
    }
    if (sms_buffer_init(buffer) != 0) {
        fprintf(stderr, "Failed to initialize circular buffer\n");
        free(buffer);
        return 1;
    }


    mqd_t alarm_queue = (mqd_t)-1;
    mqd_t log_queue = (mqd_t)-1;
    int result = 1;
    bool memory_locked = false;

    mq_unlink(SMS_ALARM_QUEUE);
    mq_unlink(SMS_LOG_QUEUE);

    struct mq_attr alarm_attr = {0};
    alarm_attr.mq_maxmsg = 10;
    alarm_attr.mq_msgsize = sizeof(sms_alarm_msg_t);
    alarm_queue = mq_open(SMS_ALARM_QUEUE, O_CREAT | O_RDWR, 0600,
                          &alarm_attr);
    if (alarm_queue == (mqd_t)-1) {
        perror("mq_open alarm");
        goto finish;
    }

    struct mq_attr log_attr = {0};
    log_attr.mq_maxmsg = 10;
    log_attr.mq_msgsize = sizeof(sms_log_msg_t);
    log_queue = mq_open(SMS_LOG_QUEUE, O_CREAT | O_RDWR, 0600, &log_attr);
    if (log_queue == (mqd_t)-1) {
        perror("mq_open log");
        goto finish;
    }

    atomic_bool sensor_running = true;
    atomic_bool alert_running = true;
    atomic_bool logger_running = true;

    sms_sensor_ctx_t sensor = {
        .buffer = buffer, .running = &sensor_running,
        .interval_ms = interval_ms, .verbose = verbose,
        .request_rt = request_rt
    };
    sms_processor_ctx_t processor = {
        .buffer = buffer, .alarm_queue = alarm_queue,
        .log_queue = log_queue, .temp_threshold = temp_threshold,
        .humid_threshold = humid_threshold
    };
    sms_alert_ctx_t alert = {
        .alarm_queue = alarm_queue, .log_queue = log_queue,
        .running = &alert_running
    };
    sms_logger_ctx_t logger = {
        .log_queue = log_queue, .running = &logger_running,
        .path = SMS_LOG_PATH
    };

    if (request_rt) {
        if (mlockall(MCL_CURRENT | MCL_FUTURE) == 0)
            memory_locked = true;
        else
            perror("mlockall");
    }

    pthread_t sensor_id, processor_id, alert_id, logger_id;
    bool sensor_started = false, processor_started = false;
    bool alert_started = false, logger_started = false;
    int status;

    status = pthread_create(&logger_id, NULL, sms_logger_thread, &logger);
    if (status != 0) {
        fprintf(stderr, "pthread_create Logger: %s\n", strerror(status));
        goto shutdown;
    }
    logger_started = true;

    status = pthread_create(&alert_id, NULL, sms_alert_thread, &alert);
    if (status != 0) {
        fprintf(stderr, "pthread_create Alert: %s\n", strerror(status));
        goto shutdown;
    }
    alert_started = true;

    status = pthread_create(&processor_id, NULL, sms_processor_thread,
                            &processor);
    if (status != 0) {
        fprintf(stderr, "pthread_create Processor: %s\n", strerror(status));
        goto shutdown;
    }
    processor_started = true;

    status = pthread_create(&sensor_id, NULL, sms_sensor_thread, &sensor);
    if (status != 0) {
        fprintf(stderr, "pthread_create Sensor: %s\n", strerror(status));
        goto shutdown;
    }
    sensor_started = true;

    printf("SMS running: interval=%u ms, T>%.1f C, H>%.1f%%, duration=%d s\n",
           interval_ms, temp_threshold, humid_threshold, duration_seconds);

    int elapsed = 0;
    while (!stop_requested &&
           (duration_seconds == 0 || elapsed < duration_seconds)) {
        sleep(1);
        elapsed++;
    }
    result = 0;

shutdown:
    atomic_store(&sensor_running, false);
    if (sensor_started)
        pthread_join(sensor_id, NULL);
    if (sensor.read_errors != 0)
        result = 1;

    sms_buffer_close(buffer);
    if (processor_started)
        pthread_join(processor_id, NULL);

    atomic_store(&alert_running, false);
    if (alert_started)
        pthread_join(alert_id, NULL);

    atomic_store(&logger_running, false);
    if (logger_started)
        pthread_join(logger_id, NULL);

    printf("Summary: read=%llu processed=%llu buffer_dropped=%llu "
           "queue_dropped=%llu logs=%llu rotations=%llu read_errors=%llu\n",
           (unsigned long long)sensor.samples_read,
           (unsigned long long)processor.processed,
           (unsigned long long)buffer->dropped,
           (unsigned long long)(processor.messages_dropped + alert.log_errors),
           (unsigned long long)logger.written,
           (unsigned long long)logger.rotations,
           (unsigned long long)sensor.read_errors);
    printf("Alerts: temperature=%llu humidity=%llu spike=%llu\n",
           (unsigned long long)alert.counts[0],
           (unsigned long long)alert.counts[1],
           (unsigned long long)alert.counts[2]);
    print_jitter(&sensor);
    puts("Shutdown complete");

    if (memory_locked)
        munlockall();

finish:
    if (alarm_queue != (mqd_t)-1)
        mq_close(alarm_queue);
    if (log_queue != (mqd_t)-1)
        mq_close(log_queue);
    mq_unlink(SMS_ALARM_QUEUE);
    mq_unlink(SMS_LOG_QUEUE);
    sms_buffer_destroy(buffer);
    free(buffer);
    return result;
}
