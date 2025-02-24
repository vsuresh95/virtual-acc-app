#ifndef __AUDIO_HELPER_H__
#define __AUDIO_HELPER_H__

#include <common-helper.hpp>

typedef unsigned token_t;
typedef uint64_t token64_t;

struct audio_inst_t : public virtual_inst_t {
    // Audio-specific configuration parameters
	unsigned logn_samples;
	unsigned do_inverse;
	unsigned do_shift;

    // Two user mode queues for input and output and one for filters/twiddles
    user_queue_t<token_t> input_queue;
    user_queue_t<token_t> output_queue;
    user_queue_t<token_t> filter_queue;

    void print() {
        virtual_inst_t::print();

        printf("\tlogn_samples = %d, do_inverse = %d, do_shift = %d\n", logn_samples, do_inverse, do_shift);
        printf("\tinput_queue.mem = %p, output_queue.mem = %p, filter_queue.mem = %p\n", input_queue.mem, output_queue.mem, filter_queue.mem);
        printf("\tinput_queue.base = %d, output_queue.base = %d, filter_queue.base = %d\n", input_queue.base, output_queue.base, filter_queue.base);
    }
};

#endif /* __AUDIO_HELPER_H__ */
