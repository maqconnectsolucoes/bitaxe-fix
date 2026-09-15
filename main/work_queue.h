#ifndef WORK_QUEUE_H
#define WORK_QUEUE_H

#include <pthread.h>

#define QUEUE_SIZE 12

typedef struct
{
    void *buffer[QUEUE_SIZE];
    int head;
    int tail;
    int count;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    void (*free_fn)(void *); // Protocol-specific free function for queue items
    // free_fn as it was when each slot was enqueued. The protocol coordinator
    // swaps free_fn (and the active protocol) while items from the previous
    // protocol can still be in flight, so whoever dequeues must free the item
    // with the function that matched it at enqueue time, not the current one.
    void (*item_free_fn[QUEUE_SIZE])(void *);
} work_queue;

void queue_init(work_queue *queue);
void queue_enqueue(work_queue *queue, void *new_work);
void *queue_dequeue(work_queue *queue);
void *queue_dequeue_timeout(work_queue *queue, int timeout_ms);
// Same as queue_dequeue_timeout, but also returns the free function captured
// for that item (NULL means plain free()).
void *queue_dequeue_timeout_ex(work_queue *queue, int timeout_ms, void (**free_fn)(void *));
void queue_clear(work_queue *queue);

#endif // WORK_QUEUE_H
