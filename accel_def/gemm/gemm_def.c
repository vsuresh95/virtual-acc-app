#define _GNU_SOURCE
#include <stdlib.h>
#include <sched.h>
#include <pthread.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <limits.h>
#include <hpthread.h>
#include <common_helper.h>
#include <vam_physical_accel.h>
#include <gemm_params.h>
#include <gemm_def.h>
#include <gemm_stratus.h>
#include <gemm_sm_stratus.h>
#include <gemm_queue.h>
#include <libesp.h>
#include <esp.h>
#include <esp_accelerator.h>
#include <nn_token.h>

// ESP API for getting contig_alloc handle
extern contig_handle_t *lookup_handle(void *buf, enum contig_alloc_policy *policy);

// Device-dependent probe function for baseline accelerator
void gemm_probe(physical_accel_t *accel) {
    accel->prim = PRIM_GEMM;
    accel->cpu_invoke = true;
    accel->ioctl_cm = GEMM_STRATUS_IOC_ACCESS;

    // Create a new access struct and track within the device struct
    struct gemm_stratus_access *gemm_desc = (struct gemm_stratus_access *) malloc (sizeof(struct gemm_stratus_access));
    accel->esp_access_desc = (struct esp_access *) gemm_desc;
    accel->esp_access_desc->ioctl_cm = ESP_IOCTL_ACC_NO_SM;
}

// Device-dependent probe function for SM accelerator
void gemm_sm_probe(physical_accel_t *accel) {
    accel->prim = PRIM_GEMM;
    accel->cpu_invoke = false;
    accel->ioctl_cm = GEMM_SM_STRATUS_IOC_ACCESS;

    // Create a new access struct and track within the device struct
    struct gemm_sm_stratus_access *gemm_desc = (struct gemm_sm_stratus_access *) malloc (sizeof(struct gemm_sm_stratus_access));
    accel->esp_access_desc = (struct esp_access *) gemm_desc;
}

void *gemm_invoke(void *a) {
    cpu_invoke_args_t *args = (cpu_invoke_args_t *) a;
    physical_accel_t *accel = args->accel;
    unsigned context = args->context;
    bool *kill_pthread = &args->kill_pthread;
    uint64_t *context_runtime = &args->active_cycles; // for VAM
    hpthread_t *th = accel->th[context];
    hpthread_args_t *h_args = th->args;
    unsigned *mem = (unsigned *) h_args->mem;
    gemm_queue_t *q = (gemm_queue_t *) &mem[h_args->queue_ptr];
    LOW_DEBUG(printf("[INVOKE] Started thread for invoking GeMM on %s:%d!\n", accel->devname, context);)
    // Set queue to busy
    if (__atomic_load_n(&(q->info.stat), __ATOMIC_SEQ_CST) == QUEUE_BUSY) { SCHED_YIELD; };
    __atomic_store_n(&(q->info.stat), QUEUE_BUSY, __ATOMIC_SEQ_CST);
    HIGH_DEBUG(printf("[INVOKE] Queue set to busy on %s:%d!\n", accel->devname, context);)

    #ifndef DO_SCHED_RR
    // Set niceness based on priority
    pid_t tid = syscall(SYS_gettid);
    unsigned prio = th->nprio;
    setpriority(PRIO_PROCESS, tid, nice_table[prio - 1]);
    HIGH_DEBUG(printf("[INVOKE] Set niceness to %d for %s:%d!\n", nice_table[prio - 1], accel->devname, context);)
    #endif

    // Set up local descriptors for each context ahead of time
    struct gemm_stratus_access *gemm_access_desc;
    gemm_access_desc = (struct gemm_stratus_access *) malloc (sizeof(struct gemm_stratus_access));
	// Setting up ESP memory buffer
    enum contig_alloc_policy policy;
    contig_handle_t *handle = lookup_handle((void*) mem, &policy);
    gemm_access_desc->esp.contig = contig_to_khandle(*handle);
    gemm_access_desc->esp.ddr_node = contig_to_most_allocated(*handle);
    gemm_access_desc->esp.alloc_policy = policy;
    gemm_access_desc->esp.run = true;
    gemm_access_desc->esp.src_offset = 0;
    gemm_access_desc->esp.dst_offset = 0;
    gemm_access_desc->esp.coherence = ACC_COH_RECALL;

    HIGH_DEBUG(unsigned invoke_count = 0;)

    while (1) {
        if (*kill_pthread) { 
            __atomic_store_n(&(q->info.stat), QUEUE_AVAIL, __ATOMIC_SEQ_CST);
            pthread_exit(NULL);
        }

        gemm_queue_entry_t *e = gemm_queue_can_pop(q);
        if (e != NULL) {
            gemm_params_t *params = &(e->gemm_params);
            gemm_access_desc->dim_m = params->dim_m;
            gemm_access_desc->dim_n = params->dim_n;
            gemm_access_desc->dim_k = params->dim_k;
            gemm_access_desc->weight_base = params->weight_base;
            gemm_access_desc->input_base = params->input_base;
            gemm_access_desc->output_base = params->output_base;

            // Wait for input to be valid/output to be empty
            unsigned *input_flag = (unsigned *) &mem[gemm_access_desc->input_base];
            unsigned *output_flag = (unsigned *) &mem[gemm_access_desc->output_base];
            while(__atomic_load_n(input_flag, __ATOMIC_ACQUIRE) != 1) { SCHED_YIELD; }
            while(__atomic_load_n(output_flag, __ATOMIC_ACQUIRE) != 0) { SCHED_YIELD; }
            unsigned expected_value = 0;
            while(!__atomic_compare_exchange_n(&accel->accel_lock, &expected_value, 1, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) { 
                expected_value = 0;
                SCHED_YIELD;
            }
            // Then change the queue tail
            gemm_queue_pop(q);
            HIGH_DEBUG(printf("[INVOKE] Starting GEMM %d on %s:%d\n", invoke_count, accel->devname, context);)

            struct esp_access *esp_access_desc = (struct esp_access *) gemm_access_desc;
            if (ioctl(accel->fd, GEMM_STRATUS_IOC_ACCESS, esp_access_desc)) {
                perror("ioctl");
                exit(EXIT_FAILURE);
            }
            // Set output valid
            __atomic_store_n(input_flag, 0, __ATOMIC_RELEASE);
            __atomic_store_n(output_flag, 1, __ATOMIC_RELEASE);
            uint64_t *mon_extended = (uint64_t *) esp_access_desc->mon_info.util;
            *context_runtime += mon_extended[0]; // Single context only
            __atomic_store_n(&accel->accel_lock, 0, __ATOMIC_RELEASE);
            HIGH_DEBUG(printf("[INVOKE] Finished GEMM %d on %s:%d\n", invoke_count++, accel->devname, context);)
        }
        SCHED_YIELD;
    }

    return NULL;
}