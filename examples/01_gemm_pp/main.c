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

int main(int argc, char **argv) {
    if (argc > 1) {
        dim_m = atoi(argv[1]);
        dim_n = atoi(argv[2]);
        dim_k = atoi(argv[3]);
    }
    if (argc > 4) {
        iterations = atoi(argv[4]);
	}

    printf("[APP] Starting app: GEMM single threaded %d %d %d %d!\n", dim_m, dim_n, dim_k, iterations);

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

    // Allocate sufficient memory for this hpthread
    unsigned mem_size = (output_queue_offset * sizeof(nn_token_t)) + sizeof(gemm_queue_t);
    nn_token_t *mem = (nn_token_t *) esp_alloc(mem_size);

    HIGH_DEBUG(printf("[APP] Memory allocated for size %d\n", mem_size));

    // In/Out task queue
    gemm_queue_t *in_q = (gemm_queue_t *) &mem[input_queue_offset];
    sm_queue_init((sm_queue_t *) in_q);
    gemm_queue_t *out_q = (gemm_queue_t *) &mem[output_queue_offset];
    sm_queue_init((sm_queue_t *) out_q);

    // Input/output flags
    unsigned *input_flag = (unsigned *) &mem[mat_a_valid_offset];
    unsigned *output_flag = (unsigned *) &mem[mat_c_valid_offset];
    __atomic_store_n(input_flag, 0, __ATOMIC_SEQ_CST);
    __atomic_store_n(output_flag, 0, __ATOMIC_SEQ_CST);

    // Declare hpthread and assign attributes
    hpthread_t *th = (hpthread_t *) malloc(sizeof(hpthread_t));
    hpthread_args_t *args = (hpthread_args_t *) malloc(sizeof(hpthread_args_t));
    args->mem = mem; args->queue_ptr = input_queue_offset;
    th->cpu_invoke = true; // Create a CPU thread to invoke the accelerator
    hpthread_setargs(th, args);
    hpthread_setname(th, "gemm");
    hpthread_setprimitive(th, PRIM_GEMM);

    HIGH_DEBUG(printf("[APP] Before hpthread create request...\n"));

    // Create a hpthread
    hpthread_create(th);
        
    unsigned input_tasks_remaining = iterations;
    unsigned inputs_remaining = iterations;
    unsigned output_tasks_remaining = iterations;
    unsigned outputs_remaining = iterations;
    gemm_queue_entry_t in_e = { 
        .gemm_params = {
            dim_m, dim_n, dim_k, mat_b_offset, mat_a_valid_offset, mat_c_valid_offset
        }
    };
    gemm_queue_entry_t out_e = { 
        .gemm_params = { 0, 0, 0, 0, mat_c_valid_offset, 0 }
    };
    
    uint64_t t_start = get_counter();
    while (input_tasks_remaining != 0 || inputs_remaining != 0 || output_tasks_remaining != 0 || outputs_remaining != 0) {
        if (input_tasks_remaining != 0) {
            // Check if input queue is full
            if (!gemm_queue_full(in_q)) {
                HIGH_DEBUG(printf("[APP] Enqueueing new input %d\n", iterations - input_tasks_remaining));
                gemm_queue_push(in_q, &in_e);
                input_tasks_remaining--;
            }
        }
        if (inputs_remaining != 0) {
            // Check if input queue is not empty and input data is invalid
            unsigned *input_flag = (unsigned *) &mem[mat_a_valid_offset];
            bool input_is_invaid = (__atomic_load_n(input_flag, __ATOMIC_ACQUIRE) == 0);
            if (!gemm_queue_empty(in_q) && input_is_invaid) {
                HIGH_DEBUG(printf("[APP] Sending new input %d\n", iterations - inputs_remaining));
                // Set input flag valid
                __atomic_store_n(input_flag, 1, __ATOMIC_RELEASE);
                inputs_remaining--;
            }
        }
        if (output_tasks_remaining != 0) {
            // Check if output queue is full
            if (!gemm_queue_full(out_q)) {
                HIGH_DEBUG(printf("[APP] Enqueueing new output %d\n", iterations - output_tasks_remaining));
                gemm_queue_push(out_q, &out_e);
                output_tasks_remaining--;
            }
        }
        if (outputs_remaining != 0) {
            // Check if output queue is not empty
            if (!gemm_queue_empty(out_q)) {
                // Silenty pop the queue to check if input is valid
                gemm_queue_entry_t *e = gemm_queue_can_pop(out_q);
                if (e != NULL) {
                    gemm_params_t *params = &(e->gemm_params);
                    // Wait for input to be valid
                    unsigned *input_flag = (unsigned *) &mem[params->input_base];
                    if (__atomic_load_n(input_flag, __ATOMIC_ACQUIRE) == 1) {
                        // Then change the queue tail
                        HIGH_DEBUG(printf("[APP] Dequeueing new output %d\n", iterations - outputs_remaining));
                        gemm_queue_pop(out_q);
                        __atomic_store_n(input_flag, 0, __ATOMIC_RELEASE);
                        outputs_remaining--;
                    }
                }
            }
        }
    }
    uint64_t t_loop = get_counter() - t_start;

    hpthread_join(th);
    esp_free(mem);
	printf("[APP] Avg time = %lu\n", t_loop/iterations);
    return 0;
}
