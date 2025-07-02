#ifndef __GEMM_HPTHREAD_ARGS_H__
#define __GEMM_HPTHREAD_ARGS_H__

#include <hpthread.h>

struct gemm_hpthread_args : public hpthread_args {
    // Parameters
    unsigned dim_m;
    unsigned dim_n;
    unsigned dim_k;

    // Input/output offsets
    unsigned input_1_queue_base;
    unsigned input_2_queue_base;
    unsigned output_queue_base;
};

#endif // __GEMM_HPTHREAD_ARGS_H__
