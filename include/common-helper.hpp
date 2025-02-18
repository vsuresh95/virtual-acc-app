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
#define SYNC_VAR_SIZE 10
#define UPDATE_VAR_SIZE 2
#define VALID_FLAG_OFFSET 0
#define END_FLAG_OFFSET 2
#define READY_FLAG_OFFSET 4
#define FLT_VALID_FLAG_OFFSET 6
#define FLT_READY_FLAG_OFFSET 8

// Capabilities available to the application
typedef enum {
	AUDIO_FFT = 0,
	AUDIO_FIR = 1,
	AUDIO_FFI = 2
} Capability;

// Definition of a virtual instance that will be requested by
// user application. Pointer to this will be added to physical to
// virtual accelerator monitor in VAM.
typedef struct {
    unsigned thread_id;
    Capability capab;

    // Generic memory parameters (for working set of task)
    unsigned in_words;
    unsigned out_words;
    unsigned acc_len;
    unsigned mem_size;
    void *hw_buf;

    // ASI parameters
    unsigned ConsRdyOffset;
    unsigned ConsVldOffset;
    unsigned ProdRdyOffset;
    unsigned ProdVldOffset;
    unsigned InputOffset;
    unsigned OutputOffset;
} VirtualInst;

#endif // __COMMON_HELPER_H__