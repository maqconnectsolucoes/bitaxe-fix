#include "work_queue.h"
#include "esp_log.h"
#include <stdlib.h>
#include <time.h>
#include <errno.h>

void queue_init(work_queue *queue)
{
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
    queue->free_fn = NULL;
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
    pthread_cond_init(&queue->not_full, NULL);
}

void queue_enqueue(work_queue *queue, void *new_work)
{
    pthread_mutex_lock(&queue->lock);

    while (queue->count == QUEUE_SIZE)
    {
        pthread_cond_wait(&queue->not_full, &queue->lock);
    }

    queue->buffer[queue->tail] = new_work;
    queue->item_free_fn[queue->tail] = queue->free_fn;
    queue->tail = (queue->tail + 1) % QUEUE_SIZE;
    queue->count++;

    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->lock);
}

void *queue_dequeue(work_queue *queue)
{
    pthread_mutex_lock(&queue->lock);

    while (queue->count == 0)
    {
        pthread_cond_wait(&queue->not_empty, &queue->lock);
    }

    void *next_work = queue->buffer[queue->head];
    queue->head = (queue->head + 1) % QUEUE_SIZE;
    queue->count--;

    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->lock);

    return next_work;
}

void *queue_dequeue_timeout(work_queue *queue, int timeout_ms)
{
    return queue_dequeue_timeout_ex(queue, timeout_ms, NULL);
}

void *queue_dequeue_timeout_ex(work_queue *queue, int timeout_ms, void (**free_fn)(void *))
{
    pthread_mutex_lock(&queue->lock);

    while (queue->count == 0)
    {
        struct timespec timeout_time;
        clock_gettime(CLOCK_REALTIME, &timeout_time);

        // Add timeout_ms milliseconds to current time
        timeout_time.tv_sec += timeout_ms / 1000;
        timeout_time.tv_nsec += (timeout_ms % 1000) * 1000000;

        // Handle nanosecond overflow
        if (timeout_time.tv_nsec >= 1000000000) {
            timeout_time.tv_sec += 1;
            timeout_time.tv_nsec -= 1000000000;
        }

        int result = pthread_cond_timedwait(&queue->not_empty, &queue->lock, &timeout_time);
        if (result == ETIMEDOUT) {
            // Timeout occurred, return NULL
            pthread_mutex_unlock(&queue->lock);
            return NULL;
        }
    }

    void *next_work = queue->buffer[queue->head];
    if (free_fn) {
        *free_fn = queue->item_free_fn[queue->head];
    }
    queue->head = (queue->head + 1) % QUEUE_SIZE;
    queue->count--;

    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->lock);

    return next_work;
}

void queue_clear(work_queue *queue)
{
    pthread_mutex_lock(&queue->lock);

    while (queue->count > 0)
    {
        void *next_work = queue->buffer[queue->head];
        void (*item_free)(void *) = queue->item_free_fn[queue->head];
        if (item_free) {
            item_free(next_work);
        } else {
            free(next_work);
        }
        queue->head = (queue->head + 1) % QUEUE_SIZE;
        queue->count--;
    }

    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->lock);
}
