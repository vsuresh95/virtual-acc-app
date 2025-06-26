#include <helper.h>
#include <sw_func.h>

////////////////////////////////////
// Multithreaded version of the example
// application using Audio FFT accelerator
// with the hpthread interface.

const unsigned logn_samples = LOGN_SAMPLES;
const unsigned do_inverse = DO_INVERSE;
const unsigned do_shift = DO_SHIFT;

// Mutex variable for thread synchronization
std::mutex worker_mutex;

// Compare accelerator output with golden output
// -- Reused from ESP master FFT2 test
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

// Initialize input and calculate golden output
// -- Reused from ESP master FFT2 test
void init_buffer(token_t *mem, native_t *gold)
{
    const float LO = -2.0;
    const float HI = 2.0;
    const unsigned num_samples = (1 << logn_samples);

    srand((unsigned int) time(NULL));

    for (unsigned j = 0; j < 2 * num_samples; j++) {
        float scaling_factor = (float) rand() / (float) RAND_MAX;
        gold[j] = LO + scaling_factor * (HI - LO);
    }

    // convert input to fixed point
    for (unsigned j = 0; j < 2 * num_samples; j++) {
        mem[j] = float_to_fixed32((native_t) gold[j], FX_IL);
    }

    // Compute golden output
    fft2_comp(gold, 1 /* num_ffts */, num_samples, logn_samples, do_inverse, do_shift);
}

void audio_fft_mt_worker(unsigned worker_id) {
    printf("[APP%d] Starting worker %d!\n", worker_id, worker_id);

    // Compute memory layout parameters
    unsigned num_samples = 1 << logn_samples;
    unsigned flag_len = PAYLOAD_OFFSET/sizeof(token_t); // Number of token_t elements reserved for flags
    unsigned in_len = flag_len + 2 * num_samples;
    unsigned out_len = flag_len + 2 * num_samples;
    unsigned in_offset = flag_len;
    unsigned out_offset = in_len + flag_len;
    unsigned mem_size = (in_len + out_len) * sizeof(token_t);
    unsigned in_valid_offset = VALID_OFFSET;
    unsigned out_valid_offset = in_len + VALID_OFFSET;

    // Allocate sufficient memory for this hpthread
    // Note: for composable, complex functions, VAM will allocate additional memory
    // within this memory pool -- therefore, it is necessary to allocate extra.
    worker_mutex.lock();
    token_t *mem = (token_t *) esp_alloc(mem_size);
    worker_mutex.unlock();

    HIGH_DEBUG(printf("[APP%d] Memory allocated for size %d\n", worker_id, mem_size));

    // Reference output for comparison
    native_t *gold = new float[out_len];

    // We will cast the synchronization flags from *mem to std atomic flags
    // Note: while this offers correctness, atomic_flag is not optimal in terms of performance
    // Use custom ASM blocks with amo_swap on void pointers for performance.
    sync_t *input_full = reinterpret_cast<sync_t *>(&mem[in_valid_offset]);
    sync_t *output_full = reinterpret_cast<sync_t *>(&mem[out_valid_offset]);
    input_full->store(0);
    output_full->store(0);

    // Assign arguments for the FFT task -- this will be used by the accelerator
    // or SW function to perform FFT and communicate with the SW.
    audio_fft_hpthread_args *args = new audio_fft_hpthread_args;
    *args = {
        (void *) mem,
        logn_samples,
        do_inverse,
        do_shift,
        in_valid_offset,
        out_valid_offset
    };

    // Declare hpthread and assign attributes
    hpthread_t *th = new hpthread_t;
    th->setroutine(sw_audio_fft);
    th->setargs(args);
    th->attr_init();
    char name[100];
    sprintf(name, "AUDIO_FFT%d", worker_id);
    th->attr_setname(name);
    th->attr_setprimitive(hpthread_prim_t::AUDIO_FFT);

    HIGH_DEBUG(printf("[APP%d] Before hpthread create request...\n", worker_id));

    // Create a hpthread
    if (th->create()) {
        perror("error in hpthread_create!");
        exit(1);
    }

    // --- At this point you have a virtual compute resource in hpthread th
    // capable of performing FFT and invoked through shared memory synchronization.

    const unsigned iterations = 1000;
    unsigned errors = 0;

    for (unsigned i = 0; i < iterations; i++) {
        HIGH_DEBUG(printf("[APP%d] Starting iteration %d!\n", worker_id, i);)

		// Accelerator is implicitly ready because computation is chained
        init_buffer(&mem[in_offset], gold);
		// Inform the accelerator to start.
		input_full->store(1);

		// Wait for the accelerator to send output.
		while(output_full->load() != 1);
		errors += validate_buffer(&mem[out_offset], gold);

		// Reset for next iteration.
		output_full->store(0);

        if (i % 100 == 0) {
            printf("[APP%d] Iter %d done!\n", worker_id, i);
        }
    }

    if (errors) {
        printf("[APP%d] FAIL: errors = %d!\n", worker_id, errors);
    } else {
        printf("[APP%d] PASS: no errors!\n", worker_id);
    }

    th->join();

    delete gold;
    esp_free(mem);
}

int main(int argc, char **argv) {
    printf("[APP] Starting app: AUDIO FFT multi threaded!\n");

    std::thread th[NUM_THREADS];

    // Start NUM_THREADS (default 4) threads for the same function
    for (int i = 0; i < NUM_THREADS; i++) {
        th[i] = std::thread(&audio_fft_mt_worker, i);
    }

    // Join all threads when done
    for (int i = 0; i < NUM_THREADS; i++) {
        th[i].join();
    }

    return 0;
}