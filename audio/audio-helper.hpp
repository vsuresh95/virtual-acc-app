#ifndef __AUDIO_HELPER_H__
#define __AUDIO_HELPER_H__

#include <common-helper.hpp>

#define NUM_AUDIO_THREADS 2
#define NUM_DEVICES 3

typedef uint64_t token_t;

typedef struct AudioInst : public VirtualInst {
    // Audio-specific configuration parameters
	unsigned logn_samples;
	unsigned do_inverse;

    // Additional Audio-specific ASI offsets
    unsigned FltVldOffset;
    unsigned FltRdyOffset;
    unsigned FltInputOffset;
    unsigned TwdInputOffset;
} AudioInst;

#endif /* __AUDIO_HELPER_H__ */
