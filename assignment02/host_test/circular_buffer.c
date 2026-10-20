#include "circular_buffer.h"

#include <string.h>

int sms_buffer_init(sms_buffer_t *buffer)
{
    if (buffer == NULL)
        return -1;

    memset(buffer, 0, sizeof(*buffer));

    if (pthread_mutex_init(&buffer->mutex, NULL) != 0)
        return -1;

    if (pthread_cond_init(&buffer->nonempty, NULL) != 0) {
        pthread_mutex_destroy(&buffer->mutex);
        return -1;
    }

    return 0;
}

bool sms_buffer_push(sms_buffer_t *buffer, const sms_sample_t *sample)
{
    if (buffer == NULL || sample == NULL)
        return false;

    pthread_mutex_lock(&buffer->mutex);

    if (buffer->closed) {
        pthread_mutex_unlock(&buffer->mutex);
        return false;
    }

    if (buffer->count == SMS_BUFFER_CAPACITY) {
        buffer->dropped++;
        pthread_mutex_unlock(&buffer->mutex);
        return false;
    }

    buffer->samples[buffer->tail] = *sample;
    buffer->tail = (buffer->tail + 1) % SMS_BUFFER_CAPACITY;
    buffer->count++;

    pthread_cond_signal(&buffer->nonempty);
    pthread_mutex_unlock(&buffer->mutex);
    return true;
}

bool sms_buffer_pop(sms_buffer_t *buffer, sms_sample_t *sample)
{
    if (buffer == NULL || sample == NULL)
        return false;

    pthread_mutex_lock(&buffer->mutex);

    while (buffer->count == 0 && !buffer->closed)
        pthread_cond_wait(&buffer->nonempty, &buffer->mutex);

    if (buffer->count == 0) {
        pthread_mutex_unlock(&buffer->mutex);
        return false;
    }

    *sample = buffer->samples[buffer->head];
    buffer->head = (buffer->head + 1) % SMS_BUFFER_CAPACITY;
    buffer->count--;

    pthread_mutex_unlock(&buffer->mutex);
    return true;
}

void sms_buffer_close(sms_buffer_t *buffer)
{
    if (buffer == NULL)
        return;

    pthread_mutex_lock(&buffer->mutex);
    buffer->closed = true;
    pthread_cond_broadcast(&buffer->nonempty);
    pthread_mutex_unlock(&buffer->mutex);
}

void sms_buffer_destroy(sms_buffer_t *buffer)
{
    if (buffer == NULL)
        return;

    pthread_cond_destroy(&buffer->nonempty);
    pthread_mutex_destroy(&buffer->mutex);
}
