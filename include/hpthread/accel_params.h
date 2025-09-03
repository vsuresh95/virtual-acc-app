#ifndef __ACCEL_PARAMS_H__
#define __ACCEL_PARAMS_H__

#include <hpthread.h>

// Createa a JUMP task descriptor
static inline void create_jump_descr(unsigned *descr_offset, unsigned jump_offset) {
	descr_offset[0] = 2;
	descr_offset[1] = jump_offset;       
}

// Createa a JUMP task descriptor
static inline void print_descr(unsigned *descr_offset) {
    for (int i = 0; i < ACCEL_PARAM_SIZE; i++) {
        printf("\tdescr[%d] = 0x%x\n", i, descr_offset[i]);
    }
}

#endif // __ACCEL_PARAMS_H__
