#ifndef __VAM_MEM_HELPER_H__
#define __VAM_MEM_HELPER_H__

struct mem_pool_t {
    void *hw_buf;
    size_t allocated; // number of bytes already allocated in this instance/task

    mem_pool_t() {
	    const unsigned mem_size = 2 * 1024 * 1024; // 2MB
        hw_buf = esp_alloc(mem_size);
        allocated = 0;
    }

    ~mem_pool_t() {
        esp_free(hw_buf);
    }

    size_t alloc(size_t mem_size) {
        unsigned curr_allocated = allocated; 
        allocated += mem_size;
        return curr_allocated;
    }
};

struct mem_queue_t {
    volatile uint8_t *mem;
    size_t base;

    bool enqueue() {
        volatile uint64_t *sync = (volatile uint64_t *) &(mem[base]);
        uint64_t old_val;
        uint64_t value = 1;

        asm volatile (
            "mv t0, %2;"
            "mv t2, %1;"
            "amoswap.d.aqrl t1, t0, (t2);"
            "mv %0, t1"
            : "=r" (old_val)
            : "r" (sync), "r" (value)
            : "t0", "t1", "t2", "memory"
            );

        return (old_val == 0);
    }

    bool dequeue() {
        volatile uint64_t *sync = (volatile uint64_t *) &(mem[base]);
        uint64_t old_val;
        uint64_t value = 0;

        asm volatile (
            "mv t0, %2;"
            "mv t2, %1;"
            "amoswap.d.aqrl t1, t0, (t2);"
            "mv %0, t1"
            : "=r" (old_val)
            : "r" (sync), "r" (value)
            : "t0", "t1", "t2", "memory"
            );

        return (old_val == 1);
    }

    bool is_full() {
        volatile uint64_t *sync = (volatile uint64_t *) &(mem[base]);
        uint64_t val;

        asm volatile (
            "mv t0, %0;"
            "lr.d.aq t1, (t0);"
            "mv %0, t1"
            : "=r" (val)
            : "r" (sync)
            : "t0", "t1", "memory"
            );

        return (val == 1);
    }

    volatile uint8_t *get_payload_base() { return &(mem[base + PAYLOAD_OFFSET]); }

    mem_queue_t(mem_pool_t *mem_pool, size_t payload_size) {
        mem = (uint8_t *) mem_pool->hw_buf;
        base = mem_pool->alloc(PAYLOAD_OFFSET + payload_size);

        // Zero out payload offset so queue is initialized to be empty.
        volatile __uint128_t *offset = (volatile __uint128_t *) &(mem[base]);
        offset[0] = 0;
    }

    ~mem_queue_t() {}
};

#endif // __VAM_MEM_HELPER_H__
