#ifndef __GEMM_DEF_H__
#define __GEMM_DEF_H__

// Wrapper for GEMM to be mapped for the hpthread
void *sw_gemm(void *a);

// Device-dependent probe function
void gemm_probe(physical_accel_t *accel);

// GEMM accelerator invocation thread
void *gemm_invoke(void *a);

typedef struct gemm_invoke_args {
    unsigned *mem;
    unsigned queue_ptr;
    bool *kill_hpthread;
    physical_accel_t *accel;
    int ep;
    bool ongoing_exec;
    HIGH_DEBUG(unsigned invoke_count;)
    struct gemm_invoke_args *next;
} gemm_invoke_args;

#endif // __GEMM_DEF_H__
