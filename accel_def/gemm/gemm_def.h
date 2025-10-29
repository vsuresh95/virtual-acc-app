#ifndef __GEMM_DEF_H__
#define __GEMM_DEF_H__

// Wrapper for GEMM to be mapped for the hpthread
void *sw_gemm(void *a);

// Device-dependent probe function for baseline accelerator
void gemm_probe(physical_accel_t *accel);

// Device-dependent probe function for SM accelerator
void gemm_sm_probe(physical_accel_t *accel);

// GEMM accelerator invocation thread
void *gemm_invoke(void *a);

#endif // __GEMM_DEF_H__
