#include <helper.h>
#include <sw_func.h>

////////////////////////////////////
// Example application FFT-FIR-IFFT chain
// with the hpthread interface.

const unsigned logn_samples = LOGN_SAMPLES;
const unsigned do_shift = DO_SHIFT;

// Compare accelerator output with golden output
int validate_buffer(token_t *mem, native_t *gold)
{
    unsigned errors = 0;
    const unsigned num_samples = 1 << logn_samples;

    for (unsigned j = 0; j < 2 * num_samples; j++) {
        native_t val = fixed32_to_float(mem[j], FX_IL);

        if ((fabs(gold[j] - val) / fabs(gold[j])) > ERR_TH) {
            if (errors < 2) { HIGH_DEBUG(printf("\tGOLD[%u] = %f vs %f = out[%u]\n", j, gold[j], val, j);) }
            errors++;
        }
    }

    HIGH_DEBUG(printf("\tRelative error > %.02f for %d values out of %d\n", ERR_TH, errors,
           2 * num_samples);)

    return errors;
}

// Initialize input 
void init_buffer(token_t *mem, native_t *gold, unsigned len)
{
    const float LO = -2.0;
    const float HI = 2.0;
    const unsigned length = len;

    srand((unsigned int) time(NULL));

    for (unsigned j = 0; j < length; j++) {
        float scaling_factor = (float) rand() / (float) RAND_MAX;
        gold[j] = LO + scaling_factor * (HI - LO);
    }

    // convert input to fixed point
    for (unsigned j = 0; j < length; j++) {
        mem[j] = float_to_fixed32((native_t) gold[j], FX_IL);
    }   
}

// Software reference computation
void sw_comp(native_t *gold, native_t *filters) {
    const unsigned num_samples = 1 << logn_samples;

    // Staging buffer for FIR
    native_t *tmpbuf = new native_t[2 * (num_samples + 1)];
    native_t *twiddles = &filters[2 * (num_samples + 1)];

    fft2_comp(gold, 1 /* num_ffts */, num_samples, logn_samples, 0 /* do_inverse */, do_shift);
    fir_comp(gold, num_samples, filters, twiddles, tmpbuf);
    fft2_comp(gold, 1 /* num_ffts */, num_samples, logn_samples, 1 /* do_inverse */, do_shift);

    delete tmpbuf;
}

int main(int argc, char **argv) {
    printf("[APP] Starting app: FFT-FIR-IFFT single threaded!\n");

    // Compute memory layout parameters
    unsigned num_samples = 1 << logn_samples;
    unsigned flag_len = PAYLOAD_OFFSET/sizeof(token_t); // Number of token_t elements reserved for flags

    unsigned fft_in_len = flag_len + 2 * num_samples;
    unsigned fir_in_len = flag_len + 2 * num_samples;
    unsigned fir_flt_len = flag_len + 2 * (num_samples + 1) + num_samples;
    unsigned ifft_in_len = flag_len + 2 * num_samples;
    unsigned ifft_out_len = flag_len + 2 * num_samples;

    unsigned in_offset = flag_len;
    unsigned flt_offset = (fft_in_len + fir_in_len) + in_offset;
    unsigned out_offset = (fir_flt_len + ifft_in_len) + flt_offset;
    unsigned mem_size = (out_offset + ifft_out_len) * sizeof(token_t);

    unsigned fft_in_valid_offset = VALID_OFFSET;
    unsigned fir_in_valid_offset = fft_in_len + fft_in_valid_offset;
    unsigned fir_flt_valid_offset = fir_in_len + fir_in_valid_offset;
    unsigned ifft_in_valid_offset = fir_flt_len + fir_flt_valid_offset;
    unsigned ifft_out_valid_offset = ifft_in_len + ifft_in_valid_offset;

    // Allocate sufficient memory for this hpthread
    // Note: for composable, complex functions, VAM will allocate additional memory
    // within this memory pool -- therefore, it is necessary to allocate extra.
    token_t *mem = (token_t *) esp_alloc(mem_size);

    HIGH_DEBUG(printf("[APP] Memory allocated for size %d\n", mem_size));

    // Reference output for comparison
    native_t *gold = new float[ifft_out_len];
    native_t *flt = new float[fir_flt_len];

    // We will cast the synchronization flags from *mem to custom atomic flags
    atomic_flag_t input_flag ((volatile uint64_t *) &mem[fft_in_valid_offset]);
    atomic_flag_t filter_flag ((volatile uint64_t *) &mem[fir_flt_valid_offset]);
    atomic_flag_t output_flag ((volatile uint64_t *) &mem[ifft_out_valid_offset]);

    // Assign arguments for all tasks -- this will be used by the accelerator
    // or SW function to perform the tasks and communicate with the SW.
    audio_fft_hpthread_args *fft_args = new audio_fft_hpthread_args;
    *fft_args = { (void *) mem, logn_samples, 0 /* do_inverse */, do_shift, fft_in_valid_offset, fir_in_valid_offset };
    audio_fir_hpthread_args *fir_args = new audio_fir_hpthread_args;
    *fir_args = { (void *) mem, logn_samples, fir_in_valid_offset, fir_flt_valid_offset, ifft_in_valid_offset };
    audio_fft_hpthread_args *ifft_args = new audio_fft_hpthread_args;
    *ifft_args = { (void *) mem, logn_samples, 1 /* do_inverse */ , do_shift, ifft_in_valid_offset, ifft_out_valid_offset };

    // Declare FFT hpthread and assign attributes
    hpthread_t *fft_th = new hpthread_t;
    fft_th->setroutine(sw_audio_fft);
    fft_th->setargs(fft_args);
    fft_th->attr_init();
    fft_th->attr_setname("AUDIO_FFT");
    fft_th->attr_setprimitive(hpthread_prim_t::AUDIO_FFT);

    // Declare FIR hpthread and assign attributes
    hpthread_t *fir_th = new hpthread_t;
    fir_th->setargs(fir_args);
    fir_th->attr_init();
    fir_th->attr_setname("AUDIO_FIR");
    fir_th->attr_setprimitive(hpthread_prim_t::AUDIO_FIR);

    // Declare IFFT hpthread and assign attributes
    hpthread_t *ifft_th = new hpthread_t;
    ifft_th->setroutine(sw_audio_fft);
    ifft_th->setargs(ifft_args);
    ifft_th->attr_init();
    ifft_th->attr_setname("AUDIO_FFT");
    ifft_th->attr_setprimitive(hpthread_prim_t::AUDIO_FFT);

    HIGH_DEBUG(printf("[APP] Before hpthread create request...\n"));

    // Create a hpthread
    if (fft_th->create() || fir_th->create() || ifft_th->create()) {
        perror("error in hpthread_create!");
        exit(1);
    }

    // --- At this point you have a virtual compute resource in hpthread th
    // capable of performing FFT and invoked through shared memory synchronization.

    const unsigned iterations = 500;
    unsigned errors = 0;

    for (unsigned i = 0; i < iterations; i++) {
        HIGH_DEBUG(printf("[APP] Starting iteration %d!\n", i);)

        // Write new FFT inputs
        init_buffer(&mem[in_offset], gold, 2 * num_samples);
		// Inform the accelerator to start.
		input_flag.store(1);

		// Write new FIR filters
        init_buffer(&mem[flt_offset], flt, 2 * (num_samples + 1) + num_samples);
		// Inform the accelerator to start.
		filter_flag.store(1);

        // Produce reference software
        sw_comp(gold, flt);

		// Wait for IFFT output and read
		while(output_flag.load() != 1);
		errors += validate_buffer(&mem[out_offset], gold);

		// Reset for next iteration.
		output_flag.store(0);

        if (i % 50 == 0) {
            printf("[APP] Iter %d done, errors = %d!\n", i, errors);
        }
    }

    if (errors) {
        printf("[APP] FAIL: errors = %d!\n", errors);
    } else {
        printf("[APP] PASS: no errors!\n");
    }

    fft_th->join();
    fir_th->join();
    ifft_th->join();

    delete gold;
    esp_free(mem);

    return errors;
}
