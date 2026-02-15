#include "bus/message_bus.h"
#include "mimi_config.h"

#include <string.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include "esp_err.h"
#include "esp_log.h"

typedef struct {
    mimi_msg_t items[MIMI_BUS_QUEUE_LEN];
    int head;
    int tail;
    int count;
    pthread_mutex_t mu;
    pthread_cond_t cv;
} host_queue_t;

static const char *TAG = "bus_host";

static host_queue_t s_inbound;
static host_queue_t s_outbound;

static void queue_init(host_queue_t *q)
{
    memset(q, 0, sizeof(*q));
    pthread_mutex_init(&q->mu, NULL);
    pthread_cond_init(&q->cv, NULL);
}

static mimi_err_t queue_push(host_queue_t *q, const mimi_msg_t *msg)
{
    pthread_mutex_lock(&q->mu);

    if (q->count >= MIMI_BUS_QUEUE_LEN) {
        pthread_mutex_unlock(&q->mu);
        return ESP_ERR_NO_MEM;
    }

    q->items[q->tail] = *msg;
    q->tail = (q->tail + 1) % MIMI_BUS_QUEUE_LEN;
    q->count++;

    pthread_cond_signal(&q->cv);
    pthread_mutex_unlock(&q->mu);
    return ESP_OK;
}

static int cond_timedwait_ms(pthread_cond_t *cv, pthread_mutex_t *mu, uint32_t timeout_ms)
{
    if (timeout_ms == UINT32_MAX) {
        return pthread_cond_wait(cv, mu);
    }

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000L;
    }

    return pthread_cond_timedwait(cv, mu, &ts);
}

static mimi_err_t queue_pop(host_queue_t *q, mimi_msg_t *msg, uint32_t timeout_ms)
{
    pthread_mutex_lock(&q->mu);

    while (q->count == 0) {
        int rc = cond_timedwait_ms(&q->cv, &q->mu, timeout_ms);
        if (rc == ETIMEDOUT) {
            pthread_mutex_unlock(&q->mu);
            return ESP_ERR_TIMEOUT;
        }
    }

    *msg = q->items[q->head];
    q->head = (q->head + 1) % MIMI_BUS_QUEUE_LEN;
    q->count--;

    pthread_mutex_unlock(&q->mu);
    return ESP_OK;
}

mimi_err_t message_bus_init(void)
{
    queue_init(&s_inbound);
    queue_init(&s_outbound);
    ESP_LOGI(TAG, "Message bus initialized (queue depth %d)", MIMI_BUS_QUEUE_LEN);
    return ESP_OK;
}

mimi_err_t message_bus_push_inbound(const mimi_msg_t *msg)
{
    return queue_push(&s_inbound, msg);
}

mimi_err_t message_bus_pop_inbound(mimi_msg_t *msg, uint32_t timeout_ms)
{
    return queue_pop(&s_inbound, msg, timeout_ms);
}

mimi_err_t message_bus_push_outbound(const mimi_msg_t *msg)
{
    return queue_push(&s_outbound, msg);
}

mimi_err_t message_bus_pop_outbound(mimi_msg_t *msg, uint32_t timeout_ms)
{
    return queue_pop(&s_outbound, msg, timeout_ms);
}
