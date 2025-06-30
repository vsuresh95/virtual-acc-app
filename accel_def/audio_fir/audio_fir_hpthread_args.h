#ifndef __AUDIO_FIR_HPTHREAD_ARGS_H__
#define __AUDIO_FIR_HPTHREAD_ARGS_H__

#include <hpthread.h>

struct audio_fir_hpthread_args : public hpthread_args {
    // Parameters
    unsigned logn_samples;

    // Input/output offsets
    unsigned input_queue_base;
    unsigned filter_queue_base;
    unsigned output_queue_base;
};

#endif // __AUDIO_FIR_HPTHREAD_ARGS_H__
