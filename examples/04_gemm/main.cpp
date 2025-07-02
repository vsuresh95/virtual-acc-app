#include <helper.h>
#include <sw_func.h>

////////////////////////////////////
// Example application using GEMM
// accelerator with the hpthread interface

const unsigned dim_m = 40;
const unsigned dim_n = 20;
const unsigned dim_k = 40;

// Compare accelerator output with golden output
int validate_buffer(token_t *mem_c, token_t *gold_c)
{
    unsigned errors = 0;
    const unsigned len = dim_m * dim_n;

    for (unsigned j = 0; j < len; j++) {
        if ((fabs(gold_c[j] - mem_c[j]) / fabs(gold_c[j])) > ERR_TH) {
            if (errors < 2) { HIGH_DEBUG(printf("\tGOLD[%u] = %d vs %d = out[%u]\n", j, gold_c[j], mem_c[j], j);) }
            errors++;
        }
    }

    HIGH_DEBUG(printf("\tRelative error > %.02f for %d values out of %d\n", ERR_TH, errors, len);)

    return errors;
}

// Initialize input and calculate golden output
void init_buffer(token_t *mem_a, token_t *mem_b, token_t *gold_a, token_t *gold_b, token_t *gold_c)
{
    const float LO = 100.0;
    const float HI = 200.0;
    const unsigned len_a = dim_m * dim_k;
    const unsigned len_b = dim_n * dim_k;

    srand((unsigned int) time(NULL));

    for (unsigned j = 0; j < len_a; j++) {
        float scaling_factor = (float) rand() / (float) RAND_MAX;
        gold_a[j] = (unsigned) (LO + scaling_factor * (HI - LO));
        mem_a[j] = (unsigned) (LO + scaling_factor * (HI - LO));
    }

    for (unsigned j = 0; j < len_b; j++) {
        float scaling_factor = (float) rand() / (float) RAND_MAX;
        gold_b[j] = (unsigned) (LO + scaling_factor * (HI - LO));
        mem_b[j] = (unsigned) (LO + scaling_factor * (HI - LO));
    }

    // Compute golden output
    gemm(gold_a, gold_b, gold_c, dim_m, dim_n, dim_k);
}

int main(int argc, char **argv) {
    printf("[APP] Starting app: GEMM single threaded!\n");

    //////////////////////////////////////////
    // Compute memory layout parameters

    // Matrix lengths
    unsigned flag_len = PAYLOAD_OFFSET/sizeof(token_t); // Number of token_t elements reserved for flags
    unsigned mat_a_len = flag_len + (dim_m * dim_k);
    unsigned mat_b_len = flag_len + (dim_n * dim_k);
    unsigned mat_c_len = flag_len + (dim_m * dim_n);

    // Data offsets
    unsigned mat_a_offset = flag_len;
    unsigned mat_b_offset = mat_a_offset + mat_a_len;
    unsigned mat_c_offset = mat_b_offset + mat_b_len;

    // Sync flag offsets
    unsigned mat_a_valid_offset = VALID_OFFSET;
    unsigned mat_b_valid_offset = mat_a_valid_offset + mat_a_len;
    unsigned mat_c_valid_offset = mat_b_valid_offset + mat_b_len;

    // Allocate sufficient memory for this hpthread
    unsigned mem_size = (mat_c_offset + mat_c_len) * sizeof(token_t);
    token_t *mem = (token_t *) esp_alloc(mem_size);

    HIGH_DEBUG(printf("[APP] Memory allocated for size %d\n", mem_size));

    // Reference output for comparison
    token_t *gold = new unsigned[mat_c_offset + mat_c_len];

    // We will cast the synchronization flags from *mem to custom atomic flags
    atomic_flag_t input_a_flag ((volatile uint64_t *) &mem[mat_a_valid_offset]);
    atomic_flag_t input_b_flag ((volatile uint64_t *) &mem[mat_b_valid_offset]);
    atomic_flag_t output_flag ((volatile uint64_t *) &mem[mat_c_valid_offset]);

    // Assign arguments for the FFT task -- this will be used by the accelerator
    // or SW function to perform FFT and communicate with the SW.
    gemm_hpthread_args *args = new gemm_hpthread_args;
    *args = {
        (void *) mem,
        dim_m, dim_n, dim_k,
        mat_a_valid_offset,
        mat_b_valid_offset,
        mat_c_valid_offset
    };

    // Declare hpthread and assign attributes
    hpthread_t *th = new hpthread_t;
    th->setroutine(sw_gemm);
    th->setargs(args);
    th->attr_init();
    th->attr_setname("gemm");
    th->attr_setprimitive(hpthread_prim_t::GEMM);

    HIGH_DEBUG(printf("[APP] Before hpthread create request...\n"));

    // Create a hpthread
    if (th->create()) {
        perror("error in hpthread_create!");
        exit(1);
    }

    // --- At this point you have a virtual compute resource in hpthread th
    // capable of performing FFT and invoked through shared memory synchronization.

    const unsigned iterations = 100;
    unsigned errors = 0;

    for (unsigned i = 0; i < iterations; i++) {
        HIGH_DEBUG(printf("[APP] Starting iteration %d!\n", i);)

		// Accelerator is implicitly ready because computation is chained
        init_buffer(&mem[mat_a_offset], &mem[mat_b_offset],
                    &gold[mat_a_offset], &gold[mat_b_offset], &gold[mat_c_offset]);
		// Inform the accelerator to start.
		input_a_flag.store(1);
		input_b_flag.store(1);

		// Wait for the accelerator to send output.
		while(output_flag.load() != 1);
		errors += validate_buffer(&mem[mat_c_offset], &gold[mat_c_offset]);

		// Reset for next iteration.
		output_flag.store(0);

        if (i % 100 == 0) {
            LOW_DEBUG(printf("[APP] Iter %d done!\n", i);)
        }
    }

    if (errors) {
        printf("[APP] FAIL: errors = %d!\n", errors);
    } else {
        printf("[APP] PASS: no errors!\n");
    }

    th->join();

    delete gold;
    esp_free(mem);

    return errors;
}
