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

void audio_fft_mt_worker(unsigned worker_id) {
    printf("[APP%d] Starting worker %d!\n", worker_id, worker_id);

    // Compute memory layout parameters
    unsigned num_samples = 1 << logn_samples;
    unsigned flag_len = PAYLOAD_OFFSET/sizeof(token_t); // Number of token_t elements reserved for flags
    unsigned in_len = flag_len + 2 * num_samples;
    unsigned out_len = flag_len + 2 * num_samples;
    unsigned mem_size = (in_len + out_len) * sizeof(token_t);
    unsigned in_valid_offset = VALID_OFFSET;
    unsigned out_valid_offset = in_len + VALID_OFFSET;

    // Allocate sufficient memory for this hpthread
    // Note: for composable, complex functions, VAM will allocate additional memory
    // within this memory pool -- therefore, it is necessary to allocate extra.
    worker_mutex.lock();
    token_t *mem = (token_t *) esp_alloc(mem_size);
    worker_mutex.unlock();

    DEBUG(printf("[APP%d] Memory allocated for size %d\n", worker_id, mem_size));
    
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
    
    DEBUG(printf("[APP%d] Before hpthread create request...\n", worker_id));

    // Create a hpthread
    if (th->create()) {
        perror("error in hpthread_create!");
        exit(1);
    }
    
    // --- At this point you have a virtual compute resource in hpthread th
    // capable of performing FFT and invoked through shared memory synchronization.

    const unsigned iterations = 10000;

    unsigned inputs_remaining = iterations;
    unsigned outputs_remaining = iterations;

    // Note: this app does not write inputs/read outputs to lighten CPU load
    while (inputs_remaining != 0 || outputs_remaining != 0) {
        if (inputs_remaining && input_full->load() == 0) {
            // Inform the accelerator to start.
            input_full->store(1);
            inputs_remaining--;
        }
        if (outputs_remaining && output_full->load() == 1) {
            // Reset for next iteration.
            output_full->store(0);
            DEBUG(printf("[APP%d] Starting iteration %d!\n", worker_id, iterations-outputs_remaining);)
            if (outputs_remaining % 1000 == 0) {
                printf("[APP%d] Iter %d done!\n", worker_id, iterations-outputs_remaining);
            }
            outputs_remaining--;
        }
    }

    printf("[APP%d] PASS: no errors!\n", worker_id);

    th->join();

    delete gold;
    esp_free(mem);
}

int main(int argc, char **argv) {
    printf("[APP] Starting app: AUDIO FFT pipelined wiht priority!\n");

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