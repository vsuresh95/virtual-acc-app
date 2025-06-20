#ifndef __HELPER_H__
#define __HELPER_H__

#include <stdio.h>
#include <common_helper.h>
#include <hpthread.h>
#include <audio_fft_hpthread_args.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <fft2_utils.h>

#ifdef __cplusplus
}
#endif

typedef unsigned token_t;
typedef float native_t;
typedef volatile std::atomic<uint64_t> sync_t;
#define FX_IL 14

const float ERR_TH = 0.05;

#define NUM_THREADS 4

#ifndef LOGN_SAMPLES
#define LOGN_SAMPLES 6
#endif

#ifndef DO_INVERSE
#define DO_INVERSE 0
#endif

#ifndef DO_SHIFT
#define DO_SHIFT 0
#endif

#endif // __HELPER_H__