#ifndef __COMMON_DEFS_H__
#define __COMMON_DEFS_H__

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

// AVU synchronization variable size and offsets (in bytes)
#define VALID_OFFSET 0
#define PAYLOAD_OFFSET 8

// Context descriptors status
#define QUEUE_INVALID 0
#define QUEUE_AVAIL 1
#define QUEUE_BUSY 2

// Different debug levels for prints
#if defined(HIGH_VERBOSE)
#define HIGH_DEBUG(x) x
#define LOW_DEBUG(x) x
#elif defined(LOW_VERBOSE)
#define HIGH_DEBUG(x)
#define LOW_DEBUG(x) x
#else
#define HIGH_DEBUG(x)
#define LOW_DEBUG(x)
#endif

#endif // __COMMON_DEFS_H__
