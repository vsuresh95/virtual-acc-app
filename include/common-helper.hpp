#ifndef __COMMON_HELPER_H__
#define __COMMON_HELPER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <libesp.h>
#include <esp.h>
#include <esp_accelerator.h>

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

typedef enum {
	TASK_FFT = 0,
	TASK_FIR = 1,
	TASK_FFI = 2
} TaskID;

#endif // __COMMON_HELPER_H__