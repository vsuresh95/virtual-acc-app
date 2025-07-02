#ifndef __SW_FUNC_H__
#define __SW_FUNC_H__

// Wrapper for fft2_comp to be mapped for the hpthread
void* sw_gemm(void *a) {
    printf("[SW FUNC] Started software thread for GeMM!\n");

    gemm_hpthread_args *args = (gemm_hpthread_args *) a;

    token_t *mem = (token_t *) args->mem;

    // Reading parameters from args
    unsigned dim_m = args->dim_m;
    unsigned dim_n = args->dim_n;
    unsigned dim_k = args->dim_k;
    unsigned mat_a_valid_offset = args->input_1_queue_base;
    unsigned mat_b_valid_offset = args->input_2_queue_base;
    unsigned mat_c_valid_offset = args->output_queue_base;
    unsigned mat_a_offset = mat_a_valid_offset + PAYLOAD_OFFSET;
    unsigned mat_b_offset = mat_b_valid_offset + PAYLOAD_OFFSET;
    unsigned mat_c_offset = mat_c_valid_offset + PAYLOAD_OFFSET;

    atomic_flag_t input_a_flag ((volatile uint64_t *) &mem[mat_a_valid_offset]);
    atomic_flag_t input_b_flag ((volatile uint64_t *) &mem[mat_b_valid_offset]);
    atomic_flag_t output_flag ((volatile uint64_t *) &mem[mat_c_valid_offset]);

    while (1) {
        // Wait for inputs to be ready
		while(input_a_flag.load() != 1);
		while(input_b_flag.load() != 1);

		// Reset for next iteration.
		input_a_flag.store(0);
		input_b_flag.store(0);

        // Perform GeMM
        gemm(&mem[mat_a_offset], &mem[mat_b_offset], &mem[mat_c_offset], dim_m, dim_n, dim_k);

        // CPU is implicitly ready because computation is chained
        // Set output to be full
		output_flag.store(1);
    }
}

void gemm(const token_t* mat_a, const token_t* mat_b, token_t* mat_c, unsigned dim_m, unsigned dim_n, unsigned dim_k) {
    const unsigned block_size = 16;
    unsigned sum;

	for (unsigned m = 0; m < dim_m; m += block_size) {
	    for (unsigned n = 0; n < dim_n; n += block_size) {
	        for (unsigned k = 0; k < dim_k; k += block_size) {

                unsigned m_rem = (m + block_size < dim_m) ? m + block_size : dim_m;
                unsigned n_rem = (n + block_size < dim_n) ? n + block_size : dim_n;
                unsigned k_rem = (k + block_size < dim_k) ? k + block_size : dim_k;

                for (unsigned m_ = m; m_ < m_rem; m_++) {
                    for (unsigned n_ = n; n_ < n_rem; n_++) {
                        if (k == 0) sum = 0;
                        else sum = mat_c[m_ * dim_n + n_];
                        for (unsigned k_ = k; k_ < k_rem; k_++) {
							sum += mat_a[m_ * dim_k + k_] * mat_b[n_ * dim_k + k_];
                        }
                        mat_c[m_ * dim_n + n_] = sum;
                    }
                }
            }
        }
    }
}

#endif // __SW_FUNC_H__