#ifndef __COMMON_HELPER_H__
#define __COMMON_HELPER_H__

#include <common_defines.h>

// Helper function to get cycle counter
static inline uint64_t get_counter() {
    uint64_t t;
    asm volatile (
        "li t0, 0;"
        "csrr t0, cycle;"
        "mv %0, t0"
        : "=r" (t)
        :
        : "t0"
    );

    return t;
}

#endif // __COMMON_HELPER_H__