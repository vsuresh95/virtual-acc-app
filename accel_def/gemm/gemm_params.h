#ifndef __GEMM_PARAMS_H__
#define __GEMM_PARAMS_H__

#include <hpthread.h>

#define GEMM_PARAM_SIZE 8

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

// Createa a JUMP task descriptor
static inline void create_jump_descr(unsigned *descr_offset, unsigned jump_offset) {
	descr_offset[0] = 2;
	descr_offset[1] = jump_offset;       
}

// Createa a JUMP task descriptor
static inline void print_descr(unsigned *descr_offset) {
    for (int i = 0; i < GEMM_PARAM_SIZE; i++) {
        printf("\tdescr[%d] = 0x%x\n", i, descr_offset[i]);
    }
}

#endif // __GEMM_PARAMS_H__
