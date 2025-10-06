#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <common_helper.h>
#include <hpthread.h>
#include <gemm_params.h>
#include <nn_token.h>
#include <sw_gemm.h>
#include <pthread.h>

// Wrapper for GEMM to be mapped for the hpthread
void *sw_gemm(void *a) {
    printf("[SW GEMM] Started software thread for GeMM!\n");
    return NULL;
    // hpthread_args_t *args = (hpthread_args_t *) a;
    // nn_token_t *mem = (nn_token_t *) args->mem;
    // unsigned *context_base_ptr = (unsigned *) &mem[args->base_ptr];
    // bool *kill_pthread = args->kill_pthread;
    // HIGH_DEBUG(unsigned iter = 0;)

    // // Check if context base pointer is available
    // while (context_base_ptr[0] != CTXT_AVAIL);
    // unsigned *context_cur_ptr = (unsigned *) &mem[context_base_ptr[1]]; 
        
    // unsigned dim_m, dim_n, dim_k;
    // unsigned mat_a_valid_offset, mat_c_valid_offset;
    // unsigned mat_a_offset, mat_b_offset, mat_c_offset;

    // while (1) {
    //     if (context_cur_ptr[0] == 1) { // GEMM
    //         gemm_params_t *params = (gemm_params_t *) &context_cur_ptr[1];
    //         HIGH_DEBUG(
    //             printf("[SW_GEMM] Printing GEMM descriptor... %p\n", &context_cur_ptr[0]);
    //             print_descr(&context_cur_ptr[0]);
    //         )
    //         // Reading parameters from params
    //         unsigned flag_len = PAYLOAD_OFFSET/sizeof(nn_token_t);
    //         dim_m = params->dim_m;
    //         dim_n = params->dim_n;
    //         dim_k = params->dim_k;
    //         mat_a_valid_offset = params->input_base;
    //         mat_c_valid_offset = params->output_base;
    //         mat_a_offset = mat_a_valid_offset + flag_len;
    //         mat_b_offset = params->weight_base;
    //         mat_c_offset = mat_c_valid_offset + flag_len;

    //         context_cur_ptr += ACCEL_PARAM_SIZE;
    //     } else if (context_cur_ptr[0] == 2) { // JUMP
    //         HIGH_DEBUG(
    //             printf("[SW_GEMM] Printing JUMP descriptor... %p\n", &context_cur_ptr[0]);
    //             print_descr(&context_cur_ptr[0]);
    //         )
    //         context_cur_ptr = (unsigned *) &mem[context_cur_ptr[1]];
    //         continue;
    //     } else {
    //         HIGH_DEBUG(printf("Bad descriptor %p!\n", &context_cur_ptr[0]);)
    //         pthread_exit(NULL);
    //     }

    //     // We will cast the synchronization flags from *mem and atomically reset them
    //     volatile uint64_t *input_flag = (volatile uint64_t *) &mem[mat_a_valid_offset];
    //     volatile uint64_t *output_flag = (volatile uint64_t *) &mem[mat_c_valid_offset];

    //     // Wait for inputs to be ready
	// 	while(__atomic_load_n(input_flag, __ATOMIC_SEQ_CST) != 1) { if (*kill_pthread) pthread_exit(NULL); };
    //     HIGH_DEBUG(printf("[SW_GEMM] Iteration %d received!\n", iter);)

	// 	// Reset for next iteration.
	// 	__atomic_store_n(input_flag, 0, __ATOMIC_SEQ_CST);

    //     // Perform GeMM
    //     gemm(&mem[mat_a_offset], &mem[mat_b_offset], &mem[mat_c_offset], dim_m, dim_n, dim_k);

    //     // Set output to be full
	// 	__atomic_store_n(output_flag, 1, __ATOMIC_SEQ_CST);
    //     HIGH_DEBUG(printf("[SW_GEMM] Iteration %d complete!\n", iter++);)
    // }
}

// Tiled matrix multiply
void gemm(const nn_token_t* mat_a, const nn_token_t* mat_b, nn_token_t* mat_c, unsigned dim_m, unsigned dim_n, unsigned dim_k) {
    const unsigned block_size = 16;
    nn_token_t sum;

	for (unsigned m = 0; m < dim_m; m += block_size) {
	    for (unsigned n = 0; n < dim_n; n += block_size) {
	        for (unsigned k = 0; k < dim_k; k += block_size) {

                unsigned m_rem = (m + block_size < dim_m) ? m + block_size : dim_m;
                unsigned n_rem = (n + block_size < dim_n) ? n + block_size : dim_n;
                unsigned k_rem = (k + block_size < dim_k) ? k + block_size : dim_k;

                for (unsigned m_ = m; m_ < m_rem; m_++) {
                    for (unsigned n_ = n; n_ < n_rem; n_++) {
                        if (k == 0) sum = nn_token_from_float(0.0F);
                        else sum = mat_c[m_ * dim_n + n_];
                        for (unsigned k_ = k; k_ < k_rem; k_++) {
							sum = nn_token_add(sum, nn_token_mul(mat_a[m_ * dim_k + k_], mat_b[k_ * dim_n + n_]));
                        }
                        mat_c[m_ * dim_n + n_] = sum;
                    }
                }
            }
        }
    }
}
