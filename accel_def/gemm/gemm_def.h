#ifndef __GEMM_DEF_H__
#define __GEMM_DEF_H__

// Device-dependent probe function
void gemm_probe(physical_accel_t *accel);

// Device-dependent configuration function
void gemm_cfg(hpthread_t *th, esp_access *generic_esp_access);

#endif // __GEMM_DEF_H__
