#ifndef __HPTHREAD_H__
#define __HPTHREAD_H__

#include <common_helper.h>

struct physical_accel_t;
typedef struct physical_accel_t physical_accel_t;
typedef uint8_t hpthread_prim_t;
struct hpthread_cand_t;
typedef struct hpthread_cand_t hpthread_cand_t;

// hpthread arguments
typedef struct {
    void *mem; // Memory pool allocated for the hpthread
    unsigned queue_ptr; // Queue base pointer
    bool *kill_pthread; // Kill the CPU pthread
} hpthread_args_t;

// Device-agnostic thread abstraction for accelerators
typedef struct {
    unsigned id; // Integer ID
    hpthread_prim_t prim; // Compute primitive
    hpthread_args_t *args; // Arguments for the thread
    unsigned nprio; // Priority of the thread: 1 (highest) - 10 (lowest)
    float th_util; // Active utilization %
    physical_accel_t *accel; // The accelerator this thread is mapped to
    unsigned accel_context; // The accelerator context this thread is mapped to
	bool is_active; // Is the thread currently active?
    uint64_t th_last_move; // When was this thread last migrated?
    bool cpu_invoke; // Is the accelerator invoked by a CPU thread?
    // Debug variables
    char name[100]; // Name
    unsigned user_id; // ID of user app
} hpthread_t;

// User APIs for hpthread
void hpthread_init(hpthread_t *th, unsigned user_id);
void hpthread_create(hpthread_t *th);
int hpthread_join(hpthread_t *th);
void hpthread_setargs(hpthread_t *th, hpthread_args_t *a);
void hpthread_setname(hpthread_t *th, const char *n);
void hpthread_setprimitive(hpthread_t *th, hpthread_prim_t p);
void hpthread_setpriority(hpthread_t *th, unsigned p);
hpthread_cand_t *hpthread_query();
static inline hpthread_prim_t hpthread_get_prim(hpthread_t *th) { return th->prim; }

// Debug API
static inline char *hpthread_get_name(hpthread_t *th) { return th->name; }
const char *hpthread_get_prim_name(hpthread_prim_t p);

// hpthread compute primitives
#define PRIM_NONE 0
#define PRIM_AUDIO_FFT 1
#define PRIM_AUDIO_FIR 2
#define PRIM_AUDIO_FFI 3
#define PRIM_GEMM 4

struct hpthread_cand_t {
    unsigned accel_id;
    hpthread_prim_t prim;
    bool cpu_invoke;
    hpthread_cand_t *next;
};

#endif // __HPTHREAD_H__
