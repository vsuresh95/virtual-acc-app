#ifndef __AUDIO_FFT_HPTHREAD_ARGS_H__
#define __AUDIO_FFT_HPTHREAD_ARGS_H__

#include <hpthread.h>

struct audio_fft_hpthread_args : public hpthread_args {
    // Parameters
    unsigned logn_samples;
    unsigned do_inverse;
    unsigned do_shift;

    // Input/output offsets
    unsigned input_queue_base;
    unsigned output_queue_base;
};

#endif // __AUDIO_FFT_HPTHREAD_ARGS_H__
