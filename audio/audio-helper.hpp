#ifndef __AUDIO_HELPER_H__
#define __AUDIO_HELPER_H__

#include <common-helper.hpp>

typedef struct AudioInst : public VirtualInst {
    // Audio-specific configuration parameters
	unsigned logn_samples;
	unsigned do_inverse;
	unsigned do_shift;

    // Additional Audio-specific ASI offsets
    unsigned FltVldOffset;
    unsigned FltRdyOffset;
    unsigned FltInputOffset;
    unsigned TwdInputOffset;

    void print() {
        VirtualInst::print();

        printf("\tlogn_samples = %d, do_inverse = %d, do_shift = %d\n", logn_samples, do_inverse, do_shift);
        printf("\tFltVldOffset = %d, FltRdyOffset = %d\n", FltVldOffset, FltRdyOffset);
        printf("\tFltInputOffset = %d, TwdInputOffset = %d\n", FltInputOffset, TwdInputOffset);
    }
} AudioInst;

#endif /* __AUDIO_HELPER_H__ */
