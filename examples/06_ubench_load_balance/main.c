#define _GNU_SOURCE
#include <helper.h>
#include <sw_gemm.h>

////////////////////////////////////
// Example application using GEMM
// accelerator with the hpthread interface

#define GEMM_QUEUE_WORDS (sizeof(sm_queue_t) / sizeof(nn_token_t))

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

    #ifdef DO_CPU_PIN
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
    #endif
    #ifdef DO_SCHED_RR
    // Set scheduling attributes
    struct sched_param sp = { .sched_priority = 1 };
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp) != 0) {
        perror("pthread_setschedparam");
    }
    #endif

    // Matrix lengths
    unsigned mat_a_len = dim_m * dim_k;
    unsigned mat_b_len = dim_n * dim_k;
    unsigned mat_c_len = dim_m * dim_n;
    // Sync flag and data offsets
    unsigned mat_a_offset = 0;
    unsigned mat_b_offset = mat_a_offset + mat_a_len;
    unsigned mat_c_offset = mat_b_offset + mat_b_len;
    unsigned input_queue_offset = mat_c_offset + mat_c_len;
    unsigned output_queue_offset = input_queue_offset + GEMM_QUEUE_WORDS;
    unsigned descriptor_offset = output_queue_offset + GEMM_QUEUE_WORDS;

    // Allocate sufficient memory for this hpthread
    unsigned mem_size = (descriptor_offset + GEMM_ENTRY_SIZE) * sizeof(nn_token_t);
    nn_token_t *mem = (nn_token_t *) esp_alloc(mem_size);

    HIGH_DEBUG(printf("[APP] Memory allocated for size %d\n", mem_size));

    // Reference output for comparison
    nn_token_t *gold = (nn_token_t *) malloc((mat_c_offset + mat_c_len) * sizeof(nn_token_t));

    // Input task queue
    sm_queue_t *in_q = (sm_queue_t *) &mem[input_queue_offset];
    sm_queue_init(in_q);
    // Output task queue
    sm_queue_t *out_q = (sm_queue_t *) &mem[output_queue_offset];
    sm_queue_init(out_q);

    // Create GEMM queue entry
    gemm_queue_entry_t *e = (gemm_queue_entry_t *) &mem[descriptor_offset];
    e->common = (sm_queue_entry_t) { 
        .output_queue = output_queue_offset,
        .output_entry = 0
    };
    e->gemm_params = (gemm_params_t) {
        .dim_m = dim_m,
        .dim_n = dim_n,
        .dim_k = dim_k,
        .weight_base = mat_b_offset,
        .input_base = mat_a_offset,
        .output_base = mat_c_offset
    };
    HIGH_DEBUG( printf("[APP] Printing GEMM queue entry...\n"); print_gemm_entry(e); )

    // Declare hpthread and assign attributes
    hpthread_t *th = (hpthread_t *) malloc(sizeof(hpthread_t));
    hpthread_args_t *args = (hpthread_args_t *) malloc(sizeof(hpthread_args_t));
    args->mem = mem; args->queue_ptr = input_queue_offset;
    #ifndef ENABLE_SM
    th->cpu_invoke = true; // Create a CPU thread to invoke the accelerator
    #else
    th->cpu_invoke = false;
    #endif
    hpthread_setargs(th, args);
    hpthread_setname(th, "gemm");
    hpthread_setprimitive(th, PRIM_GEMM);
    hpthread_setpriority(th, 1);

    HIGH_DEBUG(printf("[APP] Before hpthread create request...\n"));

    // Create a hpthread
    hpthread_create(th);
     
    unsigned outputs_remaining = iterations;
    unsigned inputs_remaining = iterations;

    uint64_t t_start = get_counter();
    while (inputs_remaining != 0 || outputs_remaining != 0) {
        if (inputs_remaining != 0) {
            // Check if input queue is full
            if (!sm_queue_full(in_q)) {
                HIGH_DEBUG(printf("[APP] Enqueueing new input %d\n", iterations - inputs_remaining));
                sm_queue_push(in_q, descriptor_offset);
                inputs_remaining--;
            }
        }
        if (outputs_remaining != 0) {
            if (!sm_queue_empty(out_q)) {
                HIGH_DEBUG(printf("[APP] Dequeueing new output %d\n", iterations - outputs_remaining));
                sm_queue_pop(out_q);
                outputs_remaining--;
                LOW_DEBUG( if (outputs_remaining % 1000 == 0) printf("[APP] Iter %d done!\n", iterations - outputs_remaining); )
            }
        }
    }
    uint64_t t_loop = get_counter() - t_start;
    hpthread_join(th);
    hpthread_report();

    free(gold);
    esp_free(mem);
	printf("[APP] Avg time = %lu\n", t_loop/iterations);

    return 0;
}
