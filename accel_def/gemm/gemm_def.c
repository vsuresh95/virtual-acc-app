#define _GNU_SOURCE
#include <stdlib.h>
#include <sched.h>
#include <pthread.h>
#include <limits.h>
#include <hpthread.h>
#include <common_helper.h>
#include <vam_physical_accel.h>
#include <gemm_params.h>
#include <gemm_def.h>
#include <gemm_stratus.h>
#include <gemm_queue.h>
#include <libesp.h>
#include <esp.h>
#include <esp_accelerator.h>
#include <nn_token.h>

// ESP API for getting contig_alloc handle
extern contig_handle_t *lookup_handle(void *buf, enum contig_alloc_policy *policy);

// Device-dependent probe function
void gemm_probe(physical_accel_t *accel) {
    accel->prim = PRIM_GEMM;

    // Create a new access struct and track within the device struct
    struct gemm_stratus_access *gemm_desc = (struct gemm_stratus_access *) malloc (sizeof(struct gemm_stratus_access));
    accel->esp_access_desc = (struct esp_access *) gemm_desc;
}

static inline bool multi_poll(unsigned *flag, unsigned expected_value, unsigned max_wait_cycles) {
    unsigned wait_cycles = 16;
    while (__atomic_load_n(flag, __ATOMIC_ACQUIRE) != expected_value) {
        for (unsigned i = 0; i < wait_cycles; i++) { __asm__ volatile ("nop"); }
        wait_cycles *= 2;
        if (wait_cycles > 128) return false;
    }
    return true;
}

void *gemm_invoke(void *a) {
    // Read the arguments struct
    physical_accel_t *accel = (physical_accel_t *) a;
    cpu_invoke_args_t *args = accel->args;
    HIGH_DEBUG(printf("[INVOKE] Started invoke thread on %s\n", accel->devname);)
    bool *kill_pthread = &args->kill_pthread;
    bitset_t *valid_contexts_ack = &args->valid_contexts_ack;
    uint64_t *context_runtime = args->active_cycles; // for VAM
    uint64_t context_vruntime[MAX_CONTEXTS] = {0}; // for local multiplexing
    uint64_t sched_period_elapsed = get_counter();
    const uint64_t SCHED_PERIOD = 78125000; // 1s
    unsigned current_context = 0;
    hpthread_t **th = accel->th;
    HIGH_DEBUG(unsigned invoke_count[MAX_CONTEXTS] = {0};)
    // Set up local descriptors for each context ahead of time
    struct gemm_stratus_access **gemm_access_desc;
    gemm_access_desc = (struct gemm_stratus_access **) malloc (MAX_CONTEXTS * sizeof(struct gemm_stratus_access *));
    for (int i = 0; i < MAX_CONTEXTS; i++) {
        gemm_access_desc[i] = (struct gemm_stratus_access *) malloc (sizeof(struct gemm_stratus_access));
    }

    while (1) {
        // Check if we need to exit
        if (*kill_pthread) {
            HIGH_DEBUG(printf("[INVOKE] Terminating invoke thread on %s\n", accel->devname);)
            pthread_exit(NULL);
        }
        // Check for old contexts to remove
        for (int i = 0; i < MAX_CONTEXTS; i++) {
            if (!bitset_test(accel->valid_contexts, i) && bitset_test(*valid_contexts_ack, i)) {
                hpthread_args_t *h_args = th[i]->args;
                unsigned *mem = (unsigned *) h_args->mem;
                gemm_queue_t *q = (gemm_queue_t *) &mem[h_args->queue_ptr];
                __atomic_store_n(&(q->info.stat), QUEUE_AVAIL, __ATOMIC_SEQ_CST);
                bitset_reset(*valid_contexts_ack, i);
                HIGH_DEBUG(printf("[INVOKE] Released context %d on %s for hpthread %s\n", i, accel->devname, hpthread_get_name(th[i]));)
            }
        }
        // Ensure there is at least one valid context
        if (bitset_none(accel->valid_contexts)) {
            SCHED_YIELD;
            continue;
        }

        // Iterate through all the contexts and find the lowest active cycles for this scheduling period
        uint64_t min_vruntime = UINT64_MAX;
        unsigned min_nprio = INT_MAX;
        unsigned new_context = (current_context + 1) % MAX_CONTEXTS;
        for (int i = 0; i < MAX_CONTEXTS; i++) {
            if (!bitset_test(accel->valid_contexts, new_context)) {
                context_vruntime[new_context] = UINT64_MAX;
            } else {
                unsigned nprio = th[new_context]->nprio;
                if (bitset_test(accel->valid_contexts, new_context) && nprio < min_nprio && context_vruntime[new_context] == min_vruntime) {
                    min_nprio = nprio;
                    current_context = new_context;
                } else if (bitset_test(accel->valid_contexts, new_context) && context_vruntime[new_context] < min_vruntime) {
                    min_vruntime = context_vruntime[new_context];
                    min_nprio = nprio;
                    current_context = new_context;
                }
            }
            new_context = (new_context + 1) % MAX_CONTEXTS;
        }
        HIGH_DEBUG(
            if (bitset_count(accel->valid_contexts) > 1)
                printf("[INVOKE] Selected context %d on %s\n", current_context, accel->devname);
        )
        // Check for new contexts to add
        for (int i = 0; i < MAX_CONTEXTS; i++) {
            if (bitset_test(accel->valid_contexts, i) && !bitset_test(*valid_contexts_ack, i)) {
                hpthread_args_t *h_args = th[i]->args;
                unsigned *mem = (unsigned *) h_args->mem;
                gemm_queue_t *q = (gemm_queue_t *) &mem[h_args->queue_ptr];
                if (__atomic_load_n(&(q->info.stat), __ATOMIC_SEQ_CST) == QUEUE_BUSY) { SCHED_YIELD; continue; };
                __atomic_store_n(&(q->info.stat), QUEUE_BUSY, __ATOMIC_SEQ_CST);
                bitset_set(*valid_contexts_ack, i);
                context_vruntime[i] = min_vruntime + 1; // Initialize vruntime
                // We will populate the common fields of esp_access
                enum contig_alloc_policy policy;
                contig_handle_t *handle = lookup_handle((void*) mem, &policy);
                gemm_access_desc[i]->esp.contig = contig_to_khandle(*handle);
                gemm_access_desc[i]->esp.ddr_node = contig_to_most_allocated(*handle);
                gemm_access_desc[i]->esp.alloc_policy = policy;
                gemm_access_desc[i]->esp.run = true;
                gemm_access_desc[i]->esp.src_offset = 0;
                gemm_access_desc[i]->esp.dst_offset = 0;
                gemm_access_desc[i]->esp.coherence = ACC_COH_RECALL;
                HIGH_DEBUG(printf("[INVOKE] Added context %d on %s for hpthread %s\n", i, accel->devname, hpthread_get_name(th[i]));)
            }
        }

        if (get_counter() - sched_period_elapsed >= SCHED_PERIOD) {
            // Reset all vruntime counters
            for (int i = 0; i < MAX_CONTEXTS; i++) {
                context_vruntime[i] = 0;
            }
            sched_period_elapsed = get_counter();
            HIGH_DEBUG(printf("[INVOKE] Scheduling period elapsed for %s\n", accel->devname);)
        }

        hpthread_args_t *h_args = th[current_context]->args;
        unsigned *mem = (unsigned *) h_args->mem;
        gemm_queue_t *q = (gemm_queue_t *) &mem[h_args->queue_ptr];
        unsigned nprio = th[current_context]->nprio;

        gemm_queue_entry_t *e = gemm_queue_can_pop(q);
        if (e != NULL) {
            gemm_params_t *params = &(e->gemm_params);
            gemm_access_desc[current_context]->dim_m = params->dim_m;
            gemm_access_desc[current_context]->dim_n = params->dim_n;
            gemm_access_desc[current_context]->dim_k = params->dim_k;
            gemm_access_desc[current_context]->weight_base = params->weight_base;
            gemm_access_desc[current_context]->input_base = params->input_base;
            gemm_access_desc[current_context]->output_base = params->output_base;

            // Wait for input to be valid/output to be empty
            unsigned *input_flag = (unsigned *) &mem[params->input_base];
            unsigned *output_flag = (unsigned *) &mem[params->output_base];
            if (!multi_poll(input_flag, 1, 5)) { SCHED_YIELD; continue; } // Try next context
            if (!multi_poll(output_flag, 0, 5)) { SCHED_YIELD; continue; } // Try next context
            // Then change the queue tail
            gemm_queue_pop(q);
            HIGH_DEBUG(printf("[INVOKE] Starting GEMM %d for context %d on %s\n", invoke_count[current_context], current_context, accel->devname);)

            struct esp_access *esp_access_desc = (struct esp_access *) gemm_access_desc[current_context];
            if (ioctl(accel->fd, GEMM_STRATUS_IOC_ACCESS, esp_access_desc)) {
                perror("ioctl");
                exit(EXIT_FAILURE);
            }
            // Set output valid
            __atomic_store_n(input_flag, 0, __ATOMIC_RELEASE);
            __atomic_store_n(output_flag, 1, __ATOMIC_RELEASE);
            uint64_t *mon_extended = (uint64_t *) esp_access_desc->mon_info.util;
            context_runtime[current_context] += mon_extended[0]; // Single context only
            context_vruntime[current_context] += (mon_extended[0] * nprio);
            HIGH_DEBUG(printf("[INVOKE] Finished GEMM %d for context %d on %s\n", invoke_count[current_context]++, current_context, accel->devname);)
        }
        SCHED_YIELD;
    }

    return NULL;
}