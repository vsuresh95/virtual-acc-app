#ifndef __SM_QUEUE_H__
#define __SM_QUEUE_H__

#include <hpthread.h>

typedef struct {
} sm_queue_entry_t;

typedef struct {
    unsigned head;
    unsigned tail;
} sm_queue_t;

#endif // __SM_QUEUE_H__
