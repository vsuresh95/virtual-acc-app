#ifndef __HPTHREAD_TYPES_H__
#define __HPTHREAD_TYPES_H__

#include <hpthread_primitives.h>

/////////////////////////////////////////
// hpthread types
// These are part of the hpthread struct (for now)

// Attributes are used to convey information useful to VAM for 
// allocation and scheduling.
struct hpthread_attr_t {
    // NAME: optionally for debug purposes within VAM
    char name[100];

    // PRIMITIVE: defined in hpthread_primitives.hpp
    hpthread_prim_t prim;
    
    // PRIORITY: RANGE 1 - 10
    unsigned prio;
} ;

// Arguments required for operation of the hpthread
// -- these are common and accelerator specific
struct hpthread_args {
    // MEMORY POOL: memory allocated for the hpthread
    void *mem;
};

#endif // __HPTHREAD_TYPES_H__
