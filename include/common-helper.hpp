#ifndef __COMMON_HELPER_H__
#define __COMMON_HELPER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <libesp.h>
#include <esp.h>
#include <esp_accelerator.h>

extern contig_handle_t* lookup_handle(void *buf, enum contig_alloc_policy *policy);

#ifdef __cplusplus
}
#endif

// ASI synchronization variable size and offsets
#define VALID_OFFSET 0
#define PAYLOAD_OFFSET 2

// Capabilities available to the application
typedef enum {
	AUDIO_FFT = 0,
	AUDIO_FIR = 1,
	AUDIO_FFI = 2
} capability_t;

struct hw_buf_pool_t {
    void *hw_buf;
    unsigned hw_buf_alloc; // number of bytes already allocated in this instance/task

    hw_buf_pool_t() {
	    const unsigned mem_size = 2 * 1024 * 1024; // 2MB
        hw_buf = esp_alloc(mem_size);
        hw_buf_alloc = 0;
    }

    unsigned alloc(unsigned mem_size) {
        hw_buf_alloc += mem_size;
        return hw_buf_alloc;
    }
};

template <typename T>
struct user_queue_t {
    T *mem;
    unsigned base;

    void enqueue() { mem[base] = 1; }

    void dequeue() { mem[base] = 0; }

    bool is_full() { return (mem[base] == 1); }

    void init(void *hw_buf, unsigned base_val) {
        mem = (T *) hw_buf;
        base = base_val;
        // Initialize queue to be empty.
        mem[base] = 0;
    }
};

// Definition of a virtual instance that will be requested by
// user application. Pointer to this will be added to physical to
// virtual accelerator monitor in VAM.
struct virtual_inst_t {
    unsigned thread_id;
    capability_t capab;

    // Common memory pool buffer
    hw_buf_pool_t *hw_buf_pool;
    
    void print() {
        printf("\tthread_id = %d\n", thread_id);
        switch(capab) {
            case AUDIO_FFT: printf("\tcapab = AUDIO_FFT\n"); break;
            case AUDIO_FIR: printf("\tcapab = AUDIO_FIR\n"); break;
            case AUDIO_FFI: printf("\tcapab = AUDIO_FFI\n"); break;
        }
        printf("\thw_buf = %p\n", hw_buf_pool->hw_buf);
    }
};

#ifdef VERBOSE
#define DEBUG(x) x
#else
#define DEBUG(x)
#endif

#endif // __COMMON_HELPER_H__