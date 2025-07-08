#ifndef __SW_KERNELS_H__
#define __SW_KERNELS_H__

#include <nn_helper.h>

void tiled_gemm(const nn_token_t* A, const nn_token_t* B, nn_token_t* C, unsigned m, unsigned n, unsigned k);

#endif // __SW_KERNELS_H__
