#define _GNU_SOURCE
#include <helper.h>
#include <sw_gemm.h>

////////////////////////////////////
// Example application using GEMM
// accelerator with the hpthread interface

unsigned dim_m = 40;
unsigned dim_n = 20;
unsigned dim_k = 40;
unsigned iterations = 100;

uint64_t t_sw;
uint64_t t_acc;
uint64_t t_push;
uint64_t t_pop;
uint64_t t_ioctl;

// Compare accelerator output with golden output
int validate_buffer(nn_token_t *mem_c, nn_token_t *gold_c)
{
    unsigned errors = 0;
    const unsigned len = dim_m * dim_n;

    for (unsigned j = 0; j < len; j++) {
        if ((fabs(nn_token_to_float(nn_token_sub(gold_c[j], mem_c[j]))) / fabs(nn_token_to_float(gold_c[j]))) > ERR_TH) {
            if (errors < 2) { HIGH_DEBUG(printf("\tGOLD[%u] = %f vs %f = out[%u]\n", j, nn_token_to_float(gold_c[j]), nn_token_to_float(mem_c[j]), j);) }
            errors++;
        }
    }

    HIGH_DEBUG(printf("\tRelative error > %.02f for %d values out of %d\n", ERR_TH, errors, len);)

    return errors;
}

// Initialize input and calculate golden output
void init_buffer(nn_token_t *mem_a, nn_token_t *mem_b, nn_token_t *gold_a, nn_token_t *gold_b, nn_token_t *gold_c)
{
    const float LO = -2.0;
    const float HI = 2.0;
    const unsigned len_a = dim_m * dim_k;
    const unsigned len_b = dim_n * dim_k;

    srand((unsigned int) time(NULL));

    for (unsigned j = 0; j < len_a; j++) {
        float scaling_factor = (float) rand() / (float) RAND_MAX;
        gold_a[j] = nn_token_from_float(LO + scaling_factor * (HI - LO));
        mem_a[j] = nn_token_from_float(LO + scaling_factor * (HI - LO));
    }

    for (unsigned j = 0; j < len_b; j++) {
        float scaling_factor = (float) rand() / (float) RAND_MAX;
        gold_b[j] = nn_token_from_float(LO + scaling_factor * (HI - LO));
        mem_b[j] = nn_token_from_float(LO + scaling_factor * (HI - LO));
    }

    // Compute golden output
    uint64_t t_start = get_counter();
    gemm(gold_a, gold_b, gold_c, dim_m, dim_n, dim_k);
    t_sw += get_counter() - t_start;
}

int main(int argc, char **argv) {
    unsigned errors = 0;
	t_sw = 0; t_acc = 0;
	t_push = 0; t_pop = 0; t_ioctl = 0;

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
        CPU_SET(0, &set); // CPU index 1 (0-based)
        if (pthread_setaffinity_np(pthread_self(), sizeof(set), &set) != 0) {
            perror("pthread_setaffinity_np");
        }
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
    unsigned output_flag_offset = mat_c_offset + mat_c_len;
    unsigned input_queue_offset = output_flag_offset + GEMM_QUEUE_SIZE;

    // Allocate sufficient memory for this hpthread
    unsigned mem_size = (input_queue_offset * sizeof(nn_token_t)) + sizeof(gemm_queue_t);
    nn_token_t *mem = (nn_token_t *) esp_alloc(mem_size);

    HIGH_DEBUG(printf("[APP] Memory allocated for size %d\n", mem_size));

    // Reference output for comparison
    nn_token_t *gold = (nn_token_t *) malloc((mat_c_offset + mat_c_len) * sizeof(nn_token_t));

    // Input task queue
    gemm_queue_t *q = (gemm_queue_t *) &mem[input_queue_offset];
    gemm_queue_init(q);
    // Output flag
    volatile unsigned *output_flag = (volatile unsigned *) &mem[output_flag_offset];
    __atomic_store_n(output_flag, 0, __ATOMIC_SEQ_CST);

    // Create GEMM queue entry
    gemm_queue_entry_t e = { 
        .common = { 
            .valid = 0,
            .req_id = 0,
            .done_offset = output_flag_offset,
        },
        .gemm_params = {
            dim_m, dim_n, dim_k, mat_b_offset, mat_a_valid_offset, mat_c_valid_offset
        }
    };
    HIGH_DEBUG(
        printf("[APP] Printing GEMM queue entry...\n");
        print_gemm_entry(&e);
    )

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

    init_buffer(&mem[mat_a_offset], &mem[mat_b_offset],
                &gold[mat_a_offset], &gold[mat_b_offset], &gold[mat_c_offset]);
        
    for (unsigned j = 0; j < iterations; j++) {
        HIGH_DEBUG(printf("[APP] Starting iteration %d!\n", j);)

        // Enqueue new GEMM entry
        uint64_t t_start = get_counter();
        if(!gemm_queue_push(q, &e)) {
            perror("full queue");
            exit(1);
        }

        // Wait for the accelerator to send output.
        while(__atomic_load_n(output_flag, __ATOMIC_SEQ_CST) != 1);
        t_acc += get_counter() - t_start;

        // Reset for next iteration.
        __atomic_store_n(output_flag, 0, __ATOMIC_SEQ_CST);

        LOW_DEBUG( if (j % 10 == 0) printf("[APP] Iter %d done!\n", j); )
    }

    errors += validate_buffer(&mem[mat_c_offset], &gold[mat_c_offset]);

    hpthread_join(th);

    free(gold);
    esp_free(mem);

    if (errors) {
        printf("[APP] FAIL: errors = %d!\n", errors);
    } else {
        printf("[APP] PASS: no errors!\n");
    }
	printf("[APP] Software = %lu\n", t_sw/iterations);
	printf("[APP] Accel = %lu\n", t_acc/iterations);
	printf("[APP] Push = %lu\n", t_push/iterations);
	printf("[APP] Pop = %lu\n", t_pop/iterations);
	printf("[APP] IOCTL = %lu\n", t_ioctl/iterations);

    return errors;
}
