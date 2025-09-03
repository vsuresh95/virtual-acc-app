#ifndef __GEMM_PARAMS_H__
#define __GEMM_PARAMS_H__

#define ACCEL_PARAM_SIZE 8

#include <hpthread.h>
#include <accel_params.h>

// Task parameters for GEMM
typedef struct {
    // Parameters
    unsigned dim_m;
    unsigned dim_n;
    unsigned dim_k;
    unsigned weight_base;
    unsigned input_base;
    unsigned output_base;
} gemm_params_t;

// Create a GEMM task descriptor
static inline void create_gemm_descr(unsigned *descr_offset, gemm_params_t *params) {
	descr_offset[0] = 1;
	descr_offset[1] = params->dim_m;       
	descr_offset[2] = params->dim_n;       
	descr_offset[3] = params->dim_k;       
	descr_offset[4] = params->weight_base; 
	descr_offset[5] = params->input_base;  
	descr_offset[6] = params->output_base; 
}

#endif // __GEMM_PARAMS_H__
