#ifndef __SM_QUEUE_H__
#define __SM_QUEUE_H__

#include <hpthread.h>

typedef struct {
    unsigned valid;
    unsigned req_id;
    unsigned done_offset;
} sm_queue_entry_t;

typedef struct {
    unsigned stat;
    hpthread_prim_t prim;
    unsigned head;
    unsigned tail;
} sm_queue_t;

#endif // __SM_QUEUE_H__
