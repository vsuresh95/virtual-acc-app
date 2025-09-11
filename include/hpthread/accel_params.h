#ifndef __ACCEL_PARAMS_H__
#define __ACCEL_PARAMS_H__

#include <hpthread.h>

// Create a JUMP task descriptor
static inline void create_jump_descr(unsigned *descr_offset, unsigned jump_offset) {
	descr_offset[0] = 2;
	descr_offset[1] = jump_offset;       
}

// Set the stat descriptor to avail
static inline void set_context_avail(unsigned *stat_offset) {
	stat_offset[0] = CTXT_AVAIL;
	asm volatile ("fence");
}

// Set the descriptor offset (but keep context invalid) 
static inline void init_context_descr(unsigned *stat_offset, unsigned descr_offset) {
	stat_offset[0] = CTXT_INVALID;
	stat_offset[1] = descr_offset;
	asm volatile ("fence");
}

// Createa a JUMP task descriptor
static inline void print_descr(unsigned *descr_offset) {
    for (int i = 0; i < ACCEL_PARAM_SIZE; i++) {
        printf("\tdescr[%d] = 0x%x\n", i, descr_offset[i]);
    }
}

#endif // __ACCEL_PARAMS_H__
