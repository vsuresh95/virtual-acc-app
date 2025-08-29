#ifndef __GEMM_DEF_H__
#define __GEMM_DEF_H__

// Wrapper for GEMM to be mapped for the hpthread
void *sw_gemm(void *a);

// Device-dependent probe function
void gemm_probe(physical_accel_t *accel);

#endif // __GEMM_DEF_H__
