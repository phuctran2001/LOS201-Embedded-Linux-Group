#ifndef SMS_MESSAGES_H
#define SMS_MESSAGES_H

#include <stdint.h>

#define SMS_ALARM_QUEUE "/sms_student_alarm"
#define SMS_LOG_QUEUE   "/sms_student_log"
#define SMS_LOG_LENGTH  192

typedef enum {
    SMS_ALERT_TEMP_HIGH = 0,
    SMS_ALERT_HUMID_HIGH = 1,
    SMS_ALERT_TEMP_SPIKE = 2
} sms_alert_type_t;

typedef struct {
    uint64_t timestamp_ms;
    sms_alert_type_t type;
    float value;
    float threshold;
} sms_alarm_msg_t;

typedef struct {
    char text[SMS_LOG_LENGTH];
} sms_log_msg_t;

#endif
