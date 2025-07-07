#ifndef __GEMM_HPTHREAD_ARGS_H__
#define __GEMM_HPTHREAD_ARGS_H__

#include <hpthread.h>

struct gemm_hpthread_args : public hpthread_args {
    // Parameters
    unsigned dim_m;
    unsigned dim_n;
    unsigned dim_k;
    unsigned weight_base;

    // Input/output offsets
    unsigned input_base;
    unsigned output_base;
};

#endif // __GEMM_HPTHREAD_ARGS_H__
