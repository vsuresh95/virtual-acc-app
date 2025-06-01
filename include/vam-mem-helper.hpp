#ifndef __VAM_MEM_HELPER_H__
#define __VAM_MEM_HELPER_H__

static pthread_mutex_t mem_helper_lock;

struct mem_pool_t {
    void *hw_buf;
    std::atomic<size_t> allocated; // number of bytes already allocated in this instance/task

    mem_pool_t() {
	    const unsigned mem_size = 2 * 1024 * 1024; // 2MB
	    pthread_mutex_lock(&mem_helper_lock);
        hw_buf = esp_alloc(mem_size);
	    pthread_mutex_unlock(&mem_helper_lock);
        allocated.store(0);
    }

    ~mem_pool_t() {
        esp_free(hw_buf);
    }

    size_t alloc(size_t mem_size) {
        return allocated.fetch_add(mem_size, std::memory_order_seq_cst);
    }
};

struct mem_queue_t {
    volatile uint8_t *mem;
    size_t base;
    volatile std::atomic<uint64_t> *sync;

    bool enqueue() {
        uint64_t expected = 0;
        return (sync->compare_exchange_strong(expected, 1, std::memory_order_seq_cst) == 0);
    }

    bool dequeue() {
        uint64_t expected = 1;
        return (sync->compare_exchange_strong(expected, 0, std::memory_order_seq_cst) == 1);
    }

    bool is_full() {
        return (sync->load(std::memory_order_seq_cst) == 1);
    }

    volatile uint8_t *get_payload_base() { return &(mem[base + PAYLOAD_OFFSET]); }

    mem_queue_t(mem_pool_t *mem_pool, size_t payload_size) {
        mem = (uint8_t *) mem_pool->hw_buf;
        base = mem_pool->alloc(PAYLOAD_OFFSET + payload_size);

        // Zero out payload offset so queue is initialized to be empty.
        volatile __uint128_t *offset = (volatile __uint128_t *) &(mem[base]);
        offset[0] = 0;

        sync = reinterpret_cast<volatile std::atomic<uint64_t> *>(&mem[base]);
    }

    ~mem_queue_t() {}
};

#endif // __VAM_MEM_HELPER_H__
