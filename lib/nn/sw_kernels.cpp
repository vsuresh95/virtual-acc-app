#include <sw_kernels.h>

// Tiled implementation of GeMM for arbitrary sizes
void tiled_gemm(const nn_token_t* mat_a, const nn_token_t* mat_b, nn_token_t* mat_c, unsigned dim_m, unsigned dim_n, unsigned dim_k) {
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
