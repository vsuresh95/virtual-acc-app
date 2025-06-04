#ifndef __VAM_MEM_HELPER_H__
#define __VAM_MEM_HELPER_H__

struct mem_pool_t {
    void *hw_buf;
    std::atomic<size_t> allocated; // number of bytes already allocated in this instance/task

    mem_pool_t();

    ~mem_pool_t();

    size_t alloc(size_t mem_size);
};

struct mem_queue_t {
    volatile uint8_t *mem;
    size_t base;
    volatile std::atomic<uint64_t> *sync;

    void enqueue();

    void dequeue();

    bool is_full();

    volatile uint8_t *get_payload_base();

    mem_queue_t(mem_pool_t *mem_pool, size_t payload_size);

    ~mem_queue_t() {}
};

// Helper function to initialize the mem pool mutex
void init_mem_helper();

// Helper functions to synchronize app and worker threads
void init_sync_threads(unsigned num_threads);
void sync_threads();

#endif // __VAM_MEM_HELPER_H__
