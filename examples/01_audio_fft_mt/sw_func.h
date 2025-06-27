#ifndef __SW_FUNC_H__
#define __SW_FUNC_H__

// Wrapper for fft2_comp to be mapped for the hpthread
void* sw_audio_fft(void *a) {
    printf("[SW FUNC] Started software thread for FFT!\n");

    audio_fft_hpthread_args *args = (audio_fft_hpthread_args *) a;

    // Temporary buffer for floating point data
    const unsigned num_samples = (1 << args->logn_samples);
    native_t *gold = new float[2 * num_samples];

    token_t *mem = (token_t *) args->mem;

    // Reading memory layout from args
    unsigned in_offset = args->input_queue_base;
    unsigned out_offset = args->output_queue_base;
    unsigned in_valid_offset = in_offset + VALID_OFFSET;
    unsigned out_valid_offset = out_offset + VALID_OFFSET;

    atomic_flag_t input_flag ((volatile uint64_t *) &mem[in_valid_offset]);
    atomic_flag_t output_flag ((volatile uint64_t *) &mem[out_valid_offset]);

    while (1) {
        // Wait for input to be ready
		while(input_flag.load() != 1);

        // Convert to floating point data
        for (unsigned j = 0; j < 2 * num_samples; j++) {
            gold[j] = fixed32_to_float(mem[in_offset + PAYLOAD_OFFSET + j], FX_IL);
        }

		// Reset for next iteration.
		input_flag.store(0);

        // Perform FFT
        fft2_comp(gold, 1 /* num_ffts */, num_samples, args->logn_samples, args->do_inverse, args->do_shift);

        // Convert to fixed point data
        for (unsigned j = 0; j < 2 * num_samples; j++) {
            mem[out_offset + PAYLOAD_OFFSET + j] = float_to_fixed32(gold[j], FX_IL);
        }

        // CPU is implicitly ready because computation is chained
        // Set output to be full
		output_flag.store(1);
    }

    delete gold;
}

#endif // __SW_FUNC_H__