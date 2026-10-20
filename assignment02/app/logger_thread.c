/*
 * Purpose: Write timestamped logs and rotate the log file.
 * Author: Tran Thien Phuc - SE203798
 * Date: 2026-10-19
 */
#define _POSIX_C_SOURCE 200809L
#include "consumer_threads.h"

#include <errno.h>
#include <stdio.h>
#include <time.h>

#define SMS_LOG_MAX_BYTES (1024L * 1024L)

static void receive_deadline(struct timespec *deadline)
{
    clock_gettime(CLOCK_REALTIME, deadline);
    deadline->tv_nsec += 200000000L;
    if (deadline->tv_nsec >= 1000000000L) {
        deadline->tv_sec++;
        deadline->tv_nsec -= 1000000000L;
    }
}

void *sms_logger_thread(void *argument)
{
    sms_logger_ctx_t *ctx = argument;
    if (ctx == NULL || ctx->running == NULL)
        return NULL;

    FILE *logfile = fopen(ctx->path, "a");
    if (logfile == NULL) {
        perror("open sms.log");
        return NULL;
    }

    time_t last_flush = time(NULL);
    for (;;) {
        time_t now = time(NULL);
        if (now != (time_t)-1 && now - last_flush >= 5) {
            if (fflush(logfile) != 0) {
                perror("flush sms.log");
                break;
            }
            last_flush = now;
        }
        sms_log_msg_t message;
        struct timespec deadline;
        receive_deadline(&deadline);

        ssize_t received = mq_timedreceive(ctx->log_queue,
                                            (char *)&message,
                                            sizeof(message), NULL,
                                            &deadline);
        if (received < 0) {
            if (errno == EINTR)
                continue;
            if (errno == ETIMEDOUT && !atomic_load(ctx->running))
                break;
            if (errno == ETIMEDOUT)
                continue;
            perror("mq_timedreceive log");
            break;
        }

        if (received != (ssize_t)sizeof(message))
            continue;
        message.text[sizeof(message.text) - 1] = '\0';

        time_t wall_clock = time(NULL);
        struct tm wall_tm;
        char stamp[32];
        if (wall_clock == (time_t)-1 ||
            localtime_r(&wall_clock, &wall_tm) == NULL ||
            strftime(stamp, sizeof(stamp),
                     "%Y-%m-%d %H:%M:%S", &wall_tm) == 0) {
            fputs("Cannot format log timestamp\n", stderr);
            break;
        }
        if (fprintf(logfile, "[%s] %s\n", stamp, message.text) < 0) {
            perror("write sms.log");
            break;
        }
        ctx->written++;

        long size = ftell(logfile);
        if (size >= SMS_LOG_MAX_BYTES) {
            char backup[sizeof(ctx->path) + 3];
            snprintf(backup, sizeof(backup), "%s.1", ctx->path);
            fclose(logfile);
            if (rename(ctx->path, backup) != 0)
                perror("rotate sms.log");
            else
                ctx->rotations++;

            logfile = fopen(ctx->path, "a");
            if (logfile == NULL) {
                perror("reopen sms.log");
                return NULL;
            }
        }
    }

    fclose(logfile);
    return NULL;
}
