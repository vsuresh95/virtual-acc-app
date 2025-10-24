#define _GNU_SOURCE
#include <helper.h>
#include <sw_gemm.h>

////////////////////////////////////
// Example application using GEMM
// accelerator with the hpthread interface

unsigned dim_m = 32;
unsigned dim_n = 32;
unsigned dim_k = 32;
unsigned iterations = 100;
unsigned n_threads = 1;

int main(int argc, char **argv) {
    if (argc > 1) {
        dim_m = atoi(argv[1]);
        dim_n = atoi(argv[2]);
        dim_k = atoi(argv[3]);
    }
    if (argc > 4) {
        iterations = atoi(argv[4]);
	}
    if (argc > 5) {
        n_threads = atoi(argv[5]);
	}

    printf("[APP] Starting app: GEMM pipelined %d %d %d %d for %d threads!\n", dim_m, dim_n, dim_k, iterations, n_threads);

    // Run main thread on CPU 0 always.
    long online = sysconf(_SC_NPROCESSORS_ONLN);
    if (online > 1) {
        cpu_set_t set;
        CPU_ZERO(&set);
        CPU_SET(0, &set);
        if (pthread_setaffinity_np(pthread_self(), sizeof(set), &set) != 0) {
            perror("pthread_setaffinity_np");
        }
    }
    // Set scheduling attributes
    struct sched_param sp = { .sched_priority = 1 };
    if (pthread_setschedparam(pthread_self(), SCHED_RR, &sp) != 0) {
        perror("pthread_setschedparam");
    }

    // Matrix lengths
    unsigned flag_len = PAYLOAD_OFFSET/sizeof(nn_token_t); // Number of nn_token_t elements reserved for flags
    unsigned mat_a_len = dim_m * dim_k;
    unsigned mat_b_len = dim_n * dim_k;
    unsigned mat_c_len = dim_m * dim_n;
    // Sync flag and data offsets
    unsigned mat_a_valid_offset = VALID_OFFSET;
    unsigned mat_a_offset = mat_a_valid_offset + flag_len;
    unsigned mat_b_offset = mat_a_offset + mat_a_len;
    unsigned mat_c_valid_offset = mat_b_offset + mat_b_len;
    unsigned mat_c_offset = mat_c_valid_offset + flag_len;
    unsigned input_queue_offset = mat_c_offset + mat_c_len;
    unsigned output_queue_offset = input_queue_offset + sizeof(gemm_queue_t)/sizeof(nn_token_t);
    unsigned thread_offset = output_queue_offset + sizeof(gemm_queue_t)/sizeof(nn_token_t);

    // Allocate sufficient memory for this hpthread
    unsigned mem_size = n_threads * ((output_queue_offset * sizeof(nn_token_t)) + sizeof(gemm_queue_t));
    nn_token_t *mem = (nn_token_t *) esp_alloc(mem_size);

    HIGH_DEBUG(printf("[APP] Memory allocated for size %d\n", mem_size));

    // In/Out task queue
    gemm_queue_t *in_q[n_threads];
    for (unsigned i = 0; i < n_threads; i++) {
        in_q[i] = (gemm_queue_t *) &mem[i * (thread_offset) + input_queue_offset];
        sm_queue_init((sm_queue_t *) in_q[i]);
    }
    gemm_queue_t *out_q[n_threads];
    for (unsigned i = 0; i < n_threads; i++) {
        out_q[i] = (gemm_queue_t *) &mem[i * (thread_offset) + output_queue_offset];
        sm_queue_init((sm_queue_t *) out_q[i]);
    }

    // Input/output flags
    unsigned *input_flag[n_threads], *output_flag[n_threads];
    for (unsigned i = 0; i < n_threads; i++) {
        input_flag[i] = (unsigned *) &mem[i * (thread_offset) + mat_a_valid_offset];
        output_flag[i] = (unsigned *) &mem[i * (thread_offset) + mat_c_valid_offset];
        __atomic_store_n(input_flag[i], 0, __ATOMIC_SEQ_CST);
        __atomic_store_n(output_flag[i], 0, __ATOMIC_SEQ_CST);
    }

    // Declare hpthread and assign attributes
    hpthread_t *th[n_threads];
    for (unsigned i = 0; i < n_threads; i++) {
        th[i] = (hpthread_t *) malloc(sizeof(hpthread_t));
        hpthread_args_t *args = (hpthread_args_t *) malloc(sizeof(hpthread_args_t));
        args->mem = mem;
        args->queue_ptr = i * (thread_offset) + input_queue_offset;
        th[i]->cpu_invoke = true; // Create a CPU thread to invoke the accelerator
        hpthread_setargs(th[i], args);
        char th_name[100];
        snprintf(th_name, 384, "gemm.%d", i);
        hpthread_setname(th[i], th_name);
        hpthread_setprimitive(th[i], PRIM_GEMM);
        hpthread_setpriority(th[i], 1);

        HIGH_DEBUG(printf("[APP] Before hpthread create request for thread %d...\n", i));

        // Create a hpthread
        hpthread_create(th[i]);
    }
        
    unsigned input_tasks_remaining[n_threads];
    unsigned inputs_remaining[n_threads];
    unsigned output_tasks_remaining[n_threads];
    unsigned outputs_remaining[n_threads];
    for (unsigned i = 0; i < n_threads; i++) {
        input_tasks_remaining[i] = iterations;
        inputs_remaining[i] = iterations;
        output_tasks_remaining[i] = iterations;
        outputs_remaining[i] = iterations;
    }
    gemm_queue_entry_t in_e[n_threads];
    for (unsigned i = 0; i < n_threads; i++) {
        in_e[i] = (gemm_queue_entry_t) { 
            .gemm_params = {
                dim_m, dim_n, dim_k, i * (thread_offset) + mat_b_offset, i * (thread_offset) + mat_a_valid_offset, i * (thread_offset) + mat_c_valid_offset
            }
        };
    }
    gemm_queue_entry_t out_e[n_threads];
    for (unsigned i = 0; i < n_threads; i++) {
        out_e[i] = (gemm_queue_entry_t) { 
            .gemm_params = { 0, 0, 0, 0, i * (thread_offset) + mat_c_valid_offset, 0 }
        };
    }

    unsigned thread_status[n_threads];
    for (unsigned i = 0; i < n_threads; i++) {
        thread_status[i] = 0;
    }
    unsigned threads_done = 0;
    unsigned thread_id = 0;
    uint64_t t_start = get_counter();
    while (threads_done < n_threads) {
        if (n_threads > 1 && thread_id == 0) SCHED_YIELD;
        
        if (thread_status[thread_id] == 1) {
            thread_id = (thread_id + 1) % n_threads;
            continue;
        }

        if (input_tasks_remaining[thread_id] + inputs_remaining[thread_id] + output_tasks_remaining[thread_id] + outputs_remaining[thread_id] == 0) {
            threads_done++;
            thread_status[thread_id] = 1;
            thread_id = (thread_id + 1) % n_threads;
            continue;
        }

        if (input_tasks_remaining[thread_id] != 0) {
            // Check if input queue is full
            if (!gemm_queue_full(in_q[thread_id])) {
                HIGH_DEBUG(printf("[APP] Enqueueing new input %d for thread %d\n", iterations - input_tasks_remaining[thread_id], thread_id));
                gemm_queue_push(in_q[thread_id], &in_e[thread_id]);
                input_tasks_remaining[thread_id]--;
            }
        }
        if (inputs_remaining[thread_id] != 0) {
            // Check if input queue is not empty and input data is invalid
            bool input_is_invaid = (__atomic_load_n(input_flag[thread_id], __ATOMIC_ACQUIRE) == 0);
            if (!gemm_queue_empty(in_q[thread_id]) && input_is_invaid) {
                HIGH_DEBUG(printf("[APP] Sending new input %d for thread %d\n", iterations - inputs_remaining[thread_id], thread_id));
                // Set input flag valid
                __atomic_store_n(input_flag[thread_id], 1, __ATOMIC_RELEASE);
                inputs_remaining[thread_id]--;
            }
        }
        if (output_tasks_remaining[thread_id] != 0) {
            // Check if output queue is full
            if (!gemm_queue_full(out_q[thread_id])) {
                HIGH_DEBUG(printf("[APP] Enqueueing new output %d for thread %d\n", iterations - output_tasks_remaining[thread_id], thread_id));
                gemm_queue_push(out_q[thread_id], &out_e[thread_id]);
                output_tasks_remaining[thread_id]--;
            }
        }
        if (outputs_remaining[thread_id] != 0) {
            // Check if output queue is not empty
            if (!gemm_queue_empty(out_q[thread_id])) {
                // Silenty pop the queue to check if input is valid
                gemm_queue_entry_t *e = gemm_queue_can_pop(out_q[thread_id]);
                if (e != NULL) {
                    gemm_params_t *params = &(e->gemm_params);
                    // Wait for input to be valid
                    unsigned *flag = (unsigned *) &mem[params->input_base];
                    if (__atomic_load_n(flag, __ATOMIC_ACQUIRE) == 1) {
                        // Then change the queue tail
                        HIGH_DEBUG(printf("[APP] Dequeueing new output %d for thread %d\n", iterations - outputs_remaining[thread_id], thread_id));
                        gemm_queue_pop(out_q[thread_id]);
                        __atomic_store_n(flag, 0, __ATOMIC_RELEASE);
                        outputs_remaining[thread_id]--;
                        LOW_DEBUG( if (outputs_remaining[thread_id] % 1000 == 0) printf("[APP%d] Iter %d done!\n", thread_id, iterations - outputs_remaining[thread_id]); )
                    }
                }
            }
        }
        thread_id = (thread_id + 1) % n_threads;
    }
    uint64_t t_loop = get_counter() - t_start;

    for (unsigned i = 0; i < n_threads; i++) {
        hpthread_join(th[i]);
    }
    esp_free(mem);
	printf("[APP] Avg time = %lu\n", t_loop/iterations);
    return 0;
}
