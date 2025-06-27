#include <helper.h>
#include <sw_func.h>

////////////////////////////////////
// Pipelined version of the example
// application with priorities using
// Audio FFT accelerator with the hpthread interface.

const unsigned logn_samples = LOGN_SAMPLES;
const unsigned do_inverse = DO_INVERSE;
const unsigned do_shift = DO_SHIFT;

// Mutex variable for thread synchronization
std::mutex worker_mutex;

// Iteration count for all workers
std::array<std::atomic<unsigned>, NUM_THREADS> iterations_done;

// Tracking iterations per second across epochs
std::vector<std::array<float, NUM_THREADS>> ips_report;

void audio_fft_pp_worker(unsigned worker_id) {
    LOW_DEBUG(printf("[APP%d] Starting worker %d!\n", worker_id, worker_id);)

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

    HIGH_DEBUG(printf("[APP%d] Memory allocated for size %d\n", worker_id, mem_size));

    // Reference output for comparison
    native_t *gold = new float[out_len];

    // We will cast the synchronization flags from *mem to custom atomic flags
    atomic_flag_t input_flag ((volatile uint64_t *) &mem[in_valid_offset]);
    atomic_flag_t output_flag ((volatile uint64_t *) &mem[out_valid_offset]);

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
    th->attr_setpriority((worker_id%4)+1);

    HIGH_DEBUG(printf("[APP%d] Before hpthread create request...\n", worker_id));

    // Create a hpthread
    if (th->create()) {
        perror("error in hpthread_create!");
        exit(1);
    }

    // --- At this point you have a virtual compute resource in hpthread th
    // capable of performing FFT and invoked through shared memory synchronization.

    unsigned inputs_remaining = NUM_ITERATIONS;
    unsigned outputs_remaining = NUM_ITERATIONS;

    LOW_DEBUG(uint64_t start_time = get_counter();)

    // Note: this app does not write inputs/read outputs to lighten CPU load
    while (inputs_remaining != 0 || outputs_remaining != 0) {
        if (inputs_remaining != 0) {
            if (input_flag.load() == 0) {
                // Inform the accelerator to start.
                input_flag.store(1);
                inputs_remaining--;
            } else {
                sched_yield();
            }
        }
        if (outputs_remaining != 0) {
            if (output_flag.load() == 1) {
                // Reset for next iteration.
                output_flag.store(0);
                outputs_remaining--;
                if (outputs_remaining == NUM_ITERATIONS / 2) {
                    // th->attr_setpriority(4-(worker_id%4));
                    printf("[APP%d] HALFWAY!\n", worker_id);
                }

                // Increment flag for main thread
                iterations_done[worker_id].fetch_add(1);

                HIGH_DEBUG(printf("[APP%d] Finshed iteration %d!\n", worker_id, iterations_done[worker_id].load());)
                LOW_DEBUG(
                    if (outputs_remaining % 1000 == 0) {
                        double ips = (double) pow(10, 9)/((get_counter() - start_time)/1000*12.8);
                        printf("[APP%d] %d; IPS=%0.2f; i/p=%d; o/p=%d\n", worker_id, iterations_done[worker_id].load(), ips, inputs_remaining, outputs_remaining);
                        start_time = get_counter();
                    }
                )
            } else {
                sched_yield();
            }
        }
    }

    printf("[APP%d] COMPLETE!\n", worker_id);

    th->join();

    delete gold;
    esp_free(mem);
}

int main(int argc, char **argv) {
    printf("[APP] Starting app: AUDIO FFT pipelined with priority, with %d threads!\n", NUM_THREADS);

    std::thread th[NUM_THREADS];

    // Start NUM_THREADS (default 4) threads for the same function
    for (int i = 0; i < NUM_THREADS; i++) {
        th[i] = std::thread(&audio_fft_pp_worker, i);
    }

    std::array<float, NUM_THREADS> old_iters = {};
    unsigned total_iterations = 0;
    const unsigned sleep_seconds = 2;
    while (total_iterations != NUM_ITERATIONS * NUM_THREADS) {
        sleep(sleep_seconds); // 2 seconds

        std::array<float, NUM_THREADS> iters;
        ips_report.push_back(iters);
        total_iterations = 0;
        LOW_DEBUG(printf("[APP] Iters = ");)
        for (int i = 0; i < NUM_THREADS; i++) {
            unsigned new_iters = iterations_done[i].load();
            float ips = (new_iters - old_iters[i]) / sleep_seconds;
            ips_report.back()[i] = ips;
            old_iters[i] = new_iters;
            total_iterations += new_iters;
            LOW_DEBUG(printf("T%d:%d, ", i, new_iters);)
        }
        LOW_DEBUG(printf("\n");)
    }

    // Join all threads when done
    for (int i = 0; i < NUM_THREADS; i++) {
        th[i].join();
    }

    hpthread_t::report();
    
    // Print out the iterations per second report
    printf("-------------------------------------------------------------------\n");
    printf("  #\t");
    for (int i = 0; i < NUM_THREADS; i++) {
        printf("%d\t", i);
    }
    printf("\n-------------------------------------------------------------------\n");
    for (size_t i = 0; i < ips_report.size(); i++) {
        printf("  %lu\t", i);
        for (int j = 0; j < NUM_THREADS; j++) {
            printf("%0.2f\t", ips_report[i][j]);
        }
        printf("\n");
    }

    return 0;
}
