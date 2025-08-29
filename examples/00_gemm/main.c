#include <helper.h>
#include <sw_gemm.h>

////////////////////////////////////
// Example application using GEMM
// accelerator with the hpthread interface

const unsigned dim_m = 40;
const unsigned dim_n = 20;
const unsigned dim_k = 40;

uint64_t t_setup;
uint64_t t_hpth;
uint64_t t_sw;
uint64_t t_acc;

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
    printf("[APP] Starting app: GEMM single threaded!\n");
	t_setup = 0; t_hpth = 0; t_sw = 0; t_acc = 0;

    const unsigned outer_iterations = 10;
    const unsigned inner_iterations = 20;
    unsigned errors = 0;

    // Outer loop
    for (unsigned i = 0; i < outer_iterations; i++) {
        LOW_DEBUG(printf("[APP] Starting outer iteration %d!\n", i);)

        // Matrix lengths
        uint64_t t_start = get_counter();
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

    	// Descriptor size - 2 (STAT) + 2 * SM_INFO_SIZE (tasks)
    	unsigned stat_len = 2;
    	unsigned stat_offset = mat_c_offset + mat_c_len;
    	unsigned descr_len = 2 * GEMM_PARAM_SIZE;
    	unsigned descr_offset = stat_offset + stat_len;

        // Allocate sufficient memory for this hpthread
        unsigned mem_size = (descr_offset + descr_len) * sizeof(nn_token_t);
        nn_token_t *mem = (nn_token_t *) esp_alloc(mem_size);

        HIGH_DEBUG(printf("[APP] Memory allocated for size %d\n", mem_size));

        // Reference output for comparison
        nn_token_t *gold = (nn_token_t *) malloc((mat_c_offset + mat_c_len) * sizeof(nn_token_t));

        // We will cast the synchronization flags from *mem and atomically reset them
        volatile uint64_t *input_flag = (volatile uint64_t *) &mem[mat_a_valid_offset];
        volatile uint64_t *output_flag = (volatile uint64_t *) &mem[mat_c_valid_offset];
        __atomic_store_n(input_flag, 0, __ATOMIC_SEQ_CST);
        __atomic_store_n(output_flag, 0, __ATOMIC_SEQ_CST);

        // Set up the task descriptors at base_ptr
        unsigned context_base_ptr = stat_offset;
        unsigned *task_descr = (unsigned *) mem;
        // Set context as active and point to descriptors
        task_descr[stat_offset] = CTXT_AVAIL; task_descr[stat_offset + 1] = descr_offset;
        // Create 1 GEMM task
        gemm_params_t params = {dim_m, dim_n, dim_k, mat_b_offset, mat_a_valid_offset, mat_c_valid_offset};
        create_gemm_descr(&task_descr[descr_offset + 0 * GEMM_PARAM_SIZE], &params);
        // Create 1 JUMP task
        create_jump_descr(&task_descr[descr_offset + 1 * GEMM_PARAM_SIZE], descr_offset);
        HIGH_DEBUG(
            printf("[APP] Printing GEMM descriptor...\n");
            print_descr(&task_descr[descr_offset + 0 * GEMM_PARAM_SIZE]);
            printf("[APP] Printing JUMP descriptor...\n");
            print_descr(&task_descr[descr_offset + 1 * GEMM_PARAM_SIZE]);
        )

        // Declare hpthread and assign attributes
        hpthread_t *th = (hpthread_t *) malloc(sizeof(hpthread_t));
        hpthread_args_t *args = (hpthread_args_t *) malloc(sizeof(hpthread_args_t));
        args->mem = mem; args->base_ptr = context_base_ptr;
        hpthread_setargs(th, args);
        hpthread_setname(th, "gemm");
        hpthread_setprimitive(th, PRIM_GEMM);
        if (i != 0) t_setup += get_counter() - t_start;

        HIGH_DEBUG(printf("[APP] Before hpthread create request...\n"));

        // Create a hpthread
        t_start = get_counter();
        hpthread_create(th);
        if (i != 0) t_hpth += get_counter() - t_start;

        // --- At this point you have a virtual compute resource in hpthread th
        // capable of performing GEMM and invoked through shared memory synchronization.

        for (unsigned j = 0; j < inner_iterations; j++) {
            HIGH_DEBUG(printf("[APP] Starting inner iteration %d!\n", j);)

    		// Accelerator is implicitly ready because computation is chained
            init_buffer(&mem[mat_a_offset], &mem[mat_b_offset],
                        &gold[mat_a_offset], &gold[mat_b_offset], &gold[mat_c_offset]);
    		// Inform the accelerator to start.
            t_start = get_counter();
    		__atomic_store_n(input_flag, 1, __ATOMIC_SEQ_CST);

    		// Wait for the accelerator to send output.
    		while(__atomic_load_n(output_flag, __ATOMIC_SEQ_CST) != 1);
            t_acc += get_counter() - t_start;
    		errors += validate_buffer(&mem[mat_c_offset], &gold[mat_c_offset]);

    		// Reset for next iteration.
    		__atomic_store_n(output_flag, 0, __ATOMIC_SEQ_CST);

            LOW_DEBUG( if ((j+1) % 10 == 0) printf("[APP] Iter %d done!\n", j); )
        }

        hpthread_join(th);

        free(gold);
        esp_free(mem);
    }

    if (errors) {
        printf("[APP] FAIL: errors = %d!\n", errors);
    } else {
        printf("[APP] PASS: no errors!\n");
    }
	printf("[APP] Setup = %lu\n", t_setup/(outer_iterations - 1));
	printf("[APP] Hpthread = %lu\n", t_hpth/(outer_iterations - 1));
	printf("[APP] Software = %lu\n", t_sw/(inner_iterations * outer_iterations));
	printf("[APP] Accel = %lu\n", t_acc/(inner_iterations * outer_iterations));

    return errors;
}
