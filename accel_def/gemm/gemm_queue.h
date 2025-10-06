#ifndef __GEMM_QUEUE_H__
#define __GEMM_QUEUE_H__

#include <sm_queue.h>
#include <gemm_params.h>

#define GEMM_QUEUE_SIZE 4

HIGH_DEBUG(static unsigned queue_empty_print = 0;)

typedef struct {
    sm_queue_entry_t common;
    gemm_params_t gemm_params;
} gemm_queue_entry_t;

typedef struct {
    sm_queue_t info;
    gemm_queue_entry_t entry[GEMM_QUEUE_SIZE];
} gemm_queue_t;

extern uint64_t t_push;
extern uint64_t t_pop;

static inline void gemm_queue_init(gemm_queue_t *q) {
    __atomic_store_n(&(q->info.stat), QUEUE_INVALID, __ATOMIC_SEQ_CST);
    __atomic_store_n(&(q->info.head), 0, __ATOMIC_SEQ_CST);
    __atomic_store_n(&(q->info.tail), 0, __ATOMIC_SEQ_CST);
    for (int i = 0; i < GEMM_QUEUE_SIZE; i++) {
        __atomic_store_n(&(q->entry[i].common.valid), 0, __ATOMIC_SEQ_CST);
    }
}

// Createa a JUMP task descriptor
static inline void print_gemm_entry(gemm_queue_entry_t *e) {
    printf("\tvalid=%d\n", e->common.valid);
    printf("\treq_id=%d\n", e->common.req_id);
    printf("\tdone_offset=%d\n", e->common.done_offset);
    printf("\tdim_m=%d\n", e->gemm_params.dim_m);
    printf("\tdim_n=%d\n", e->gemm_params.dim_n);
    printf("\tdim_k=%d\n", e->gemm_params.dim_k);
    printf("\tweight_base=%d\n", e->gemm_params.weight_base);
    printf("\tinput_base=%d\n", e->gemm_params.input_base);
    printf("\toutput_base=%d\n", e->gemm_params.output_base);
}

static inline bool gemm_queue_push(gemm_queue_t *q, gemm_queue_entry_t *e) {
    uint64_t t_start = get_counter();
    unsigned head = __atomic_load_n(&(q->info.head), __ATOMIC_ACQUIRE);
    unsigned tail = __atomic_load_n(&(q->info.tail), __ATOMIC_ACQUIRE);

    // Full when advancing head would equal tail
    unsigned next = (head + 1) % GEMM_QUEUE_SIZE;
    if (next == tail) {
        HIGH_DEBUG(printf("[SM PUSH] GEMM queue is full\n");)
        return false;
    }

    // Ensure previous consumer cleared valid at target slot
    gemm_queue_entry_t *slot = &(q->entry[head]);
    unsigned *slot_valid = &(slot->common.valid);

    // Copy params and increment head
    *slot = *e;
    __atomic_store_n(slot_valid, 1, __ATOMIC_RELEASE);
    // Advance head
    __atomic_store_n(&(q->info.head), next, __ATOMIC_RELEASE);
    t_push += get_counter() - t_start;
    HIGH_DEBUG(printf("[SM PUSH] Pushed GEMM entry %u at idx %u\n", e->common.req_id, head);)
    return true;
}

static inline gemm_queue_entry_t *gemm_queue_pop(gemm_queue_t *q) {
    uint64_t t_start = get_counter();
    unsigned head = __atomic_load_n(&(q->info.head), __ATOMIC_ACQUIRE);
    unsigned tail = __atomic_load_n(&(q->info.tail), __ATOMIC_ACQUIRE);

    // Empty when head == tail
    if (head == tail) {
        HIGH_DEBUG(
            if (queue_empty_print < 10) {
                printf("[SM POP] GEMM queue is empty\n");
            }
            queue_empty_print++;
        )
        return NULL;
    }    

    asm volatile ("fence");
    // Ensure target slot is valid
    gemm_queue_entry_t *slot = &q->entry[tail];
    HIGH_DEBUG(queue_empty_print = 0;)
    unsigned *slot_valid = &(slot->common.valid);
    if (__atomic_load_n(slot_valid, __ATOMIC_ACQUIRE) != 1) {
        HIGH_DEBUG(printf("[SM POP] GEMM slot %u not valid yet\n", tail);)
        return NULL;
    }

    // Clear valid before advancing tail to allow producer reuse
    __atomic_store_n(slot_valid, 0, __ATOMIC_RELEASE);
    // Advance tail
    __atomic_store_n(&(q->info.tail), (tail + 1) % GEMM_QUEUE_SIZE, __ATOMIC_RELEASE);
    t_pop += get_counter() - t_start;
    HIGH_DEBUG(printf("[SM POP] Popped GEMM entry %u from idx %u\n", slot->common.req_id, tail);)
    return slot;
}

#endif // __GEMM_QUEUE_H__
