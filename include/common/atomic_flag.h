#ifndef __ATOMIC_FLAG_H__
#define __ATOMIC_FLAG_H__

// Custom implementation of atomic flag for performance
typedef struct {
    volatile uint64_t *flag;
} atomic_flag_t;

static inline uint64_t atomic_flag_load(atomic_flag_t *atom) {
    return __atomic_load_n(atom->flag, __ATOMIC_SEQ_CST);
}

static inline void atomic_flag_store(atomic_flag_t *atom, uint64_t val) {
    __atomic_store_n(atom->flag, val, __ATOMIC_SEQ_CST);
}

static inline void atomic_flag_init(atomic_flag_t *atom, volatile uint64_t *f) {
	atom->flag = f;
	atomic_flag_store(atom, 0);
}

#endif // __ATOMIC_FLAG_H__
