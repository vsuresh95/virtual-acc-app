#define _GNU_SOURCE
#include <stdlib.h>
#include <sched.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <hpthread.h>
#include <common_helper.h>
#include <vam_physical_accel.h>
#include <gemm_params.h>
#include <gemm_stratus.h>
#include <gemm_queue.h>
#include <gemm_def.h>
#include <libesp.h>
#include <esp.h>
#include <esp_accelerator.h>
#include <nn_token.h>

// ESP API for getting contig_alloc handle
extern contig_handle_t *lookup_handle(void *buf, enum contig_alloc_policy *policy);
// Instance for invoke argments
cpu_invoke_intf_t gemm_cpu_invoke_intf;
// Invoke args list
gemm_invoke_args *gemm_invoke_args_list = NULL;

// Device-dependent probe function
void gemm_probe(physical_accel_t *accel) {
    accel->prim = PRIM_GEMM;

    // Create a new access struct and track within the device struct
    struct gemm_stratus_access *gemm_desc = (struct gemm_stratus_access *) malloc (sizeof(struct gemm_stratus_access));
    accel->esp_access_desc = (struct esp_access *) gemm_desc;
}

static inline bool multi_poll(unsigned *flag, unsigned expected_value, unsigned max_wait_cycles) {
    unsigned total_waited = 0;
    while (__atomic_load_n(flag, __ATOMIC_ACQUIRE) != expected_value) {
        total_waited++;
        if (total_waited >= max_wait_cycles) return false;
    }
    return true;
}

void *gemm_invoke(void *a) {
    // Kill pthread flag
    bool *kill_thread = (bool *) a;
    // Initialize current invoke args list
    gemm_invoke_args *cur_entry = NULL;
    struct epoll_event out; // Dummy struct for epoll wait
    while (1) {
        // Check for exit
        if (*kill_thread) pthread_exit(NULL);
        // Check if there are valid args to process
        uint8_t state = __atomic_load_n(&gemm_cpu_invoke_intf.state, __ATOMIC_ACQUIRE);
        if (state == INVOKE_ARGS_VALID) {
            // Retrieve the args
            cpu_invoke_args_t *args = gemm_cpu_invoke_intf.args;
            // Create a new entry in the invoke args list
            gemm_invoke_args *new_entry = (gemm_invoke_args *) malloc (sizeof(gemm_invoke_args));
            hpthread_args_t *h_args = args->args;
            new_entry->mem = (unsigned *) h_args->mem;
            new_entry->queue_ptr = h_args->queue_ptr;
            new_entry->accel = args->accel;
            LOW_DEBUG(printf("[INVOKE] Creating new invoke entry for GeMM on %s!\n", new_entry->accel->devname);)
            new_entry->kill_hpthread = h_args->kill_hpthread;
            new_entry->ongoing_exec = false;
            // Add new entry to the list
            new_entry->next = NULL;
            if (gemm_invoke_args_list == NULL) {
                gemm_invoke_args_list = new_entry;
            } else {
                gemm_invoke_args *cur = gemm_invoke_args_list;
                while (cur->next != NULL) cur = cur->next;
                cur->next = new_entry;
            }
            
            // Set the interface state to invalid
            __atomic_store_n(&gemm_cpu_invoke_intf.state, INVOKE_ARGS_INVALID, __ATOMIC_RELEASE);
            // Free the temporary args struct
            free(args);

            // Setting up ESP memory buffer
            enum contig_alloc_policy policy;
            struct gemm_stratus_access *gemm_access_desc = (struct gemm_stratus_access *) new_entry->accel->esp_access_desc;
            contig_handle_t *handle = lookup_handle((void*) new_entry->mem, &policy);
            gemm_access_desc->esp.contig = contig_to_khandle(*handle);
            gemm_access_desc->esp.ddr_node = contig_to_most_allocated(*handle);
            gemm_access_desc->esp.alloc_policy = policy;
            gemm_access_desc->esp.run = true;
            gemm_access_desc->esp.src_offset = 0;
            gemm_access_desc->esp.dst_offset = 0;
            gemm_access_desc->esp.coherence = ACC_COH_RECALL;
            HIGH_DEBUG(new_entry->invoke_count = 0;)

            // Setup epoll event for polling completions
            new_entry->ep = epoll_create1(0);
            if (new_entry->ep < 0) {
                perror("epoll_create1");
                exit(EXIT_FAILURE);
            }
            struct epoll_event ev = { .events = EPOLLIN, .data.fd = new_entry->accel->fd };
            if (epoll_ctl(new_entry->ep, EPOLL_CTL_ADD, new_entry->accel->fd, &ev) < 0) {
                perror("epoll_ctl");
                exit(EXIT_FAILURE);
            }

            LOW_DEBUG(printf("[INVOKE] Created invoke entry for GeMM on %s!\n", new_entry->accel->devname);)
        }

        // Ensure that the invoke arg list is not empty
        if (gemm_invoke_args_list == NULL) {
            SCHED_YIELD;
            continue;
        }
        // Ensure current entry is not NULL
        if (cur_entry == NULL) cur_entry = gemm_invoke_args_list;

        // Check if the current entry must be killed
        if (*(cur_entry->kill_hpthread)) {
            gemm_invoke_args *to_free = cur_entry;
            cur_entry = cur_entry->next;
            free(to_free);
            // Move to next entry
            if (cur_entry == NULL) cur_entry = gemm_invoke_args_list;
            LOW_DEBUG(printf("[INVOKE] Terminated invoke thread for %s\n", cur_entry->accel->devname);)
            SCHED_YIELD;
            continue;
        }

        // Check if the previous execution is done
        if (cur_entry->ongoing_exec) {
            int n = epoll_wait(cur_entry->ep, &out, 1, 0);
            if (n != 0) {
                if (n < 0) {
                    perror("epoll_wait");
                    exit(EXIT_FAILURE);
                }
                // Reset/set input/output flags
                struct gemm_stratus_access *gemm_access_desc = (struct gemm_stratus_access *) cur_entry->accel->esp_access_desc;
                unsigned *input_flag = (unsigned *) &cur_entry->mem[gemm_access_desc->input_base];
                unsigned *output_flag = (unsigned *) &cur_entry->mem[gemm_access_desc->output_base];
                __atomic_store_n(input_flag, 0, __ATOMIC_RELEASE);
                __atomic_store_n(output_flag, 1, __ATOMIC_RELEASE);
                // Execution is complete
                cur_entry->ongoing_exec = false;
                HIGH_DEBUG(printf("[INVOKE] Finished GEMM %d on %s\n", cur_entry->invoke_count++, cur_entry->accel->devname);)
            }
        }

        // Check if current queue can be popped
        gemm_queue_t *q = (gemm_queue_t *) &cur_entry->mem[cur_entry->queue_ptr];
        gemm_queue_entry_t *e = gemm_queue_can_pop(q);
        if (e != NULL && !cur_entry->ongoing_exec) {
            gemm_params_t *params = &(e->gemm_params);
            struct gemm_stratus_access *gemm_access_desc = (struct gemm_stratus_access *) cur_entry->accel->esp_access_desc;
            physical_accel_t *accel = cur_entry->accel;
            gemm_access_desc->dim_m = params->dim_m;
            gemm_access_desc->dim_n = params->dim_n;
            gemm_access_desc->dim_k = params->dim_k;
            gemm_access_desc->weight_base = params->weight_base;
            gemm_access_desc->input_base = params->input_base;
            gemm_access_desc->output_base = params->output_base;

            // Wait for input to be valid/output to be empty
            unsigned *input_flag = (unsigned *) &cur_entry->mem[gemm_access_desc->input_base];
            unsigned *output_flag = (unsigned *) &cur_entry->mem[gemm_access_desc->output_base];
            if (!multi_poll(input_flag, 1, 100) || !multi_poll(output_flag, 0, 100)) {
                cur_entry = cur_entry->next;
                continue;
            }
            // Then change the queue tail
            gemm_queue_pop(q);
            HIGH_DEBUG(printf("[INVOKE] Starting GEMM %d on %s\n", cur_entry->invoke_count, accel->devname);)

            struct esp_access *esp_access_desc = (struct esp_access *) accel->esp_access_desc;
            if (ioctl(accel->fd, GEMM_STRATUS_IOC_ACCESS, esp_access_desc)) {
                perror("ioctl");
                exit(EXIT_FAILURE);
            }
            // Set ongoing exec flag
            cur_entry->ongoing_exec = true;
        }

        // Switch to the next round-robin
        cur_entry = cur_entry->next;
        SCHED_YIELD;
    }

    return NULL;
}
