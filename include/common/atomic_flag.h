#ifndef __ATOMIC_FLAG_H__
#define __ATOMIC_FLAG_H__

// Custom implementation of atomic flag for performance
struct atomic_flag_t {
    volatile uint64_t *flag;

    uint64_t load() volatile {
        uint64_t val;

        asm volatile (
            "mv t0, %0;"
            "lr.d.aq t1, (t0);"
            "mv %0, t1;"
            : "=r" (val)
            : "r" (flag)
            : "t0", "t1", "memory"
            );

        return val;
    }

    void store (uint64_t val) volatile {
        uint64_t old_val;

        asm volatile (
            "mv t0, %2;"
            "mv t2, %1;"
            "amoswap.d.aqrl t1, t0, (t2);"
            "mv %0, t1;"
            : "=r" (old_val)
            : "r" (flag), "r" (val)
            : "t0", "t1", "t2", "memory"
            );
    }

    atomic_flag_t (volatile uint64_t *f) {
        flag = f;
        store(0);
    }
};

#endif // __ATOMIC_FLAG_H__