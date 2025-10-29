#ifndef __SM_QUEUE_H__
#define __SM_QUEUE_H__

#include <hpthread.h>

#define SM_ENTRY_SIZE 6

typedef struct {
} sm_queue_entry_t;

typedef struct {
    uint64_t stat;
    uint64_t head;
    uint64_t tail;
} sm_queue_t;

static inline void sm_queue_init(sm_queue_t *q) {
    __atomic_store_n(&(q->stat), QUEUE_AVAIL, __ATOMIC_SEQ_CST);
    __atomic_store_n(&(q->head), 0, __ATOMIC_SEQ_CST);
    __atomic_store_n(&(q->tail), 0, __ATOMIC_SEQ_CST);
}

#endif // __SM_QUEUE_H__
