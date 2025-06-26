#ifndef __COMMON_DEFS_H__
#define __COMMON_DEFS_H__

#include <queue>
#include <unordered_set>
#include <map>
#include <vector>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <algorithm>
#include <cstring>
#include <thread>
#include <pthread.h>
#include <dirent.h>
#include <fnmatch.h>
#include <stdio.h>
#include <stdint.h>
#include <iostream>
#include <bitset>
#include <utility>
#include <climits>
#include <mutex>

// ASI synchronization variable size and offsets (in bytes)
#define VALID_OFFSET 0
#define PAYLOAD_OFFSET 8

//  Number of concurrent contexts possible in a single accelerator
#define MAX_CONTEXTS 4

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

#endif // __COMMON_DEFS_H__
