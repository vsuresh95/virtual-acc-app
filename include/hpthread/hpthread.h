#ifndef __HPTHREAD_H__
#define __HPTHREAD_H__

#include <common_helper.h>

struct physical_accel_t;
typedef struct physical_accel_t physical_accel_t;
typedef uint8_t hpthread_prim_t;

// hpthread arguments
typedef struct {
    void *mem; // memory pool allocated for the hpthread
    unsigned base_ptr; // Thread base pointer
    bool *kill_pthread; // Thread base pointer
} hpthread_args_t;

// Device-agnostic thread abstraction for accelerators
typedef struct {
    unsigned id; // Integer ID
    hpthread_prim_t prim; // Compute primitive
    hpthread_args_t *args; // Arguments for the thread
    unsigned nprio; // Priority of the thread: 1 (highest) - 10 (lowest)
    unsigned active_load; // Active utilization %
    physical_accel_t *accel; // The accelerator this thread is mapped to
    unsigned accel_context; // The accelerator context this thread is mapped to
    // Debug variables
    char name[100]; // Name
    unsigned user_id; // ID of user app
} hpthread_t;

// User APIs for hpthread
void hpthread_create(hpthread_t *th);
int hpthread_join(hpthread_t *th);
void hpthread_setargs(hpthread_t *th, hpthread_args_t *a);
void hpthread_setname(hpthread_t *th, const char *n);
void hpthread_setprimitive(hpthread_t *th, hpthread_prim_t p);
void hpthread_setpriority(hpthread_t *th, unsigned p);
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

#endif // __HPTHREAD_H__
