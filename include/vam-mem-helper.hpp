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
    uint8_t *mem;
    size_t base;

    void enqueue() { mem[base] = 1; }

    void dequeue() { mem[base] = 0; }

    bool is_full() { return (mem[base] == 1); }

    mem_queue_t(mem_pool_t *mem_pool, size_t payload_size) {
        mem = (uint8_t *) mem_pool->hw_buf;
        base = mem_pool->alloc(PAYLOAD_OFFSET + payload_size);

        // Initialize queue to be empty.
        mem[base] = 0;
    }

    ~mem_queue_t() {}
};

#endif // __VAM_MEM_HELPER_H__
