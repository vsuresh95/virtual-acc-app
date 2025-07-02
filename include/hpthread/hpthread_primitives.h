#ifndef __HPTHREAD_PRIMITIVES_H__
#define __HPTHREAD_PRIMITIVES_H__

/////////////////////////////////////////
// Declaration of all supported primitives for hpthread

enum class hpthread_prim_t {
    NONE = 0,
    AUDIO_FFT = 1,
    AUDIO_FIR = 2,
    AUDIO_FFI = 3,
    GEMM = 4,
};

// Helper function for printing primitive names
// -- Defined in hpthread.cpp
const char *get_prim_name(hpthread_prim_t p);

#endif // __HPTHREAD_PRIMITIVES_H__
