#ifndef __GEMM_NODE_ARGS_H__
#define __GEMM_NODE_ARGS_H__

#include <gemm_hpthread_args.h>

// Parameter struct for GEMM
struct gemm_node_args : public gemm_hpthread_args {
    // Input file for read-only parameters
    char input_file[100];
};

#endif // __GEMM_NODE_ARGS_H__
