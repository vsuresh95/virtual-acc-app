#ifndef __GEMM_QUEUE_H__
#define __GEMM_QUEUE_H__

#include <sm_queue.h>
#include <gemm_node_args.h>

#define GEMM_QUEUE_SIZE 4
#define GEMM_ENTRIES_OFFSET 2
#define GEMM_ENTRY_SIZE GEMM_PARAM_SIZE

typedef struct {
    sm_queue_entry_t common;
    gemm_params_t gemm_params;
} gemm_queue_entry_t;

typedef struct {
    sm_queue_t info;
    gemm_queue_entry_t entry[GEMM_QUEUE_SIZE];
} gemm_queue_t;

static inline void print_gemm_entry(gemm_queue_entry_t *e) {
    printf("\tdim_m=%d\n", e->gemm_params.dim_m);
    printf("\tdim_n=%d\n", e->gemm_params.dim_n);
    printf("\tdim_k=%d\n", e->gemm_params.dim_k);
    printf("\tweight_base=%d\n", e->gemm_params.weight_base);
    printf("\tinput_base=%d\n", e->gemm_params.input_base);
    printf("\toutput_base=%d\n", e->gemm_params.output_base);
}

static inline bool gemm_queue_push(gemm_queue_t *q, gemm_queue_entry_t *e) {
    unsigned head = __atomic_load_n(&(q->info.head), __ATOMIC_ACQUIRE);
    unsigned tail = __atomic_load_n(&(q->info.tail), __ATOMIC_ACQUIRE);

    // Full when advancing head would equal tail
    unsigned next = (head + 1) % GEMM_QUEUE_SIZE;
    if (next == tail) {
        return false;
    }

    // Copy params to head slot and advance head
    __atomic_thread_fence(__ATOMIC_ACQ_REL);
    gemm_queue_entry_t *slot = &(q->entry[head]);
    *slot = *e;
    __atomic_store_n(&(q->info.head), next, __ATOMIC_RELEASE);
    return true;
}

static inline void gemm_queue_pop(gemm_queue_t *q) {
    unsigned tail = __atomic_load_n(&(q->info.tail), __ATOMIC_ACQUIRE);
    __atomic_store_n(&(q->info.tail), (tail + 1) % GEMM_QUEUE_SIZE, __ATOMIC_RELEASE);
}

static inline gemm_queue_entry_t *gemm_queue_can_pop(gemm_queue_t *q) {
    unsigned head = __atomic_load_n(&(q->info.head), __ATOMIC_ACQUIRE);
    unsigned tail = __atomic_load_n(&(q->info.tail), __ATOMIC_ACQUIRE);

    // Empty when head == tail
    if (head == tail) {
        return NULL;
    }
    // Read entry and advance tail
    gemm_queue_entry_t *slot = &q->entry[tail];
    return slot;
}

static inline bool gemm_queue_empty(gemm_queue_t *q) {
    sm_queue_t *info = &(q->info);
    unsigned head = info->head;
    unsigned tail = info->tail;
    __atomic_thread_fence(__ATOMIC_ACQUIRE);

    return (head == tail);
}

static inline bool gemm_queue_full(gemm_queue_t *q) {
    sm_queue_t *info = &(q->info);
    unsigned head = info->head;
    unsigned tail = info->tail;
    __atomic_thread_fence(__ATOMIC_ACQUIRE);

    unsigned next = (head + 1) % GEMM_QUEUE_SIZE;
    return (next == tail);
}
    
#endif // __GEMM_QUEUE_H__
