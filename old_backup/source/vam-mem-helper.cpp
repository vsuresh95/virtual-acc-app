#include <common-defs.hpp>
#include <vam-mem-helper.hpp>

pthread_mutex_t mem_helper_lock;

mem_pool_t::mem_pool_t() {
    const unsigned mem_size = 2 * 1024 * 1024; // 2MB
    pthread_mutex_lock(&mem_helper_lock);
    hw_buf = esp_alloc(mem_size);
    pthread_mutex_unlock(&mem_helper_lock);
    allocated.store(0);
}

mem_pool_t::~mem_pool_t() {
    esp_free(hw_buf);
}

size_t mem_pool_t::alloc(size_t mem_size) {
    return allocated.fetch_add(mem_size, std::memory_order_seq_cst);
}

void init_mem_helper() {
    if (pthread_mutex_init(&mem_helper_lock, NULL) != 0) {
        printf("\n mutex init for mem helper has failed\n");
        exit(1);
    }
}

void mem_queue_t::enqueue() {
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

    // return (old_val == 0);
}

void mem_queue_t::dequeue() {
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

    // return (old_val == 1);
}

bool mem_queue_t::is_full() {
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

volatile uint8_t *mem_queue_t::get_payload_base() { return &(mem[base + PAYLOAD_OFFSET]); }

mem_queue_t::mem_queue_t(mem_pool_t *mem_pool, size_t payload_size) {
    mem = (uint8_t *) mem_pool->hw_buf;
    base = mem_pool->alloc(PAYLOAD_OFFSET + payload_size);

    // Zero out payload offset so queue is initialized to be empty.
    volatile __uint128_t *offset = (volatile __uint128_t *) &(mem[base]);
    offset[0] = 0;
}

std::atomic<unsigned> thread_barrier;

void init_sync_threads(unsigned num_threads) {
    thread_barrier.store(num_threads, std::memory_order_seq_cst);
}

void sync_threads() {
    std::atomic_thread_fence(std::memory_order_seq_cst);
    thread_barrier.fetch_sub(1, std::memory_order_seq_cst);
    while (thread_barrier.load(std::memory_order_seq_cst) != 0);
    std::atomic_thread_fence(std::memory_order_seq_cst);
}
