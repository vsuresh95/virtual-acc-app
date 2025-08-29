#ifndef __SW_GEMM_H__
#define __SW_GEMM_H__

// Wrapper for GEMM to be mapped for the hpthread
void *sw_gemm(void *a);

// Tiled matrix multiply
void gemm(const nn_token_t* A, const nn_token_t* B, nn_token_t* C, unsigned m, unsigned n, unsigned k);

#endif // __SW_GEMM_H__
