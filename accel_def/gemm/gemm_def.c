#define _GNU_SOURCE
#include <stdlib.h>
#include <sched.h>
#include <pthread.h>
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

void *gemm_invoke(void *a) {
    cpu_invoke_args_t *args = (cpu_invoke_args_t *) a;
    hpthread_args_t *h_args = args->args;
    physical_accel_t *accel = args->accel;
    unsigned *mem = (unsigned *) h_args->mem;
    gemm_queue_t *q = (gemm_queue_t *) &mem[h_args->queue_ptr];
    bool *kill_pthread = h_args->kill_pthread;
    LOW_DEBUG(printf("[INVOKE] Started thread for invoking GeMM on %s!\n", accel->devname);)

	// Setting up ESP memory buffer
    enum contig_alloc_policy policy;
    struct gemm_stratus_access *gemm_access_desc = (struct gemm_stratus_access *) accel->esp_access_desc;
    contig_handle_t *handle = lookup_handle((void*) mem, &policy);
    gemm_access_desc->esp.contig = contig_to_khandle(*handle);
    gemm_access_desc->esp.ddr_node = contig_to_most_allocated(*handle);
    gemm_access_desc->esp.alloc_policy = policy;
    gemm_access_desc->esp.run = true;
    gemm_access_desc->esp.src_offset = 0;
    gemm_access_desc->esp.dst_offset = 0;
    gemm_access_desc->esp.coherence = ACC_COH_RECALL;

    while (1) {
        if (*kill_pthread) pthread_exit(NULL);

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
            while(__atomic_load_n(input_flag, __ATOMIC_ACQUIRE) != 1);
            while(__atomic_load_n(output_flag, __ATOMIC_ACQUIRE) != 0);
            // Then change the queue tail
            gemm_queue_pop(q);
            __atomic_store_n(input_flag, 0, __ATOMIC_RELEASE);
            HIGH_DEBUG(printf("[INVOKE] Starting GEMM on %s\n", accel->devname);)

            struct esp_access *esp_access_desc = (struct esp_access *) gemm_access_desc;
            if (ioctl(accel->fd, GEMM_STRATUS_IOC_ACCESS, esp_access_desc)) {
                perror("ioctl");
                exit(EXIT_FAILURE);
            }
            // Set output valid
            __atomic_store_n(output_flag, 1, __ATOMIC_RELEASE);
        }
    }

    return NULL;
}