#ifndef __FCNN_H__
#define __FCNN_H__

#include <nn_frontend.h>

// Iteration count for all workers
std::array<std::atomic<unsigned>, NUM_THREADS> iterations_done;

// Tracking iterations per second across epochs
std::vector<std::array<float, NUM_THREADS>> ips_report;

void fcnn_worker(unsigned worker_id) {
    HIGH_DEBUG(printf("[WORKER%d] Starting worker %d!\n", worker_id, worker_id);)

    // Load a model from a text file (and) register with NN frontend
    nn_module m;
    m.load_and_register("model_5.txt");

    initialize_data("input.txt", m.input_data, 4096);

    unsigned inputs_remaining = NUM_ITERATIONS;
    unsigned outputs_remaining = NUM_ITERATIONS;

    LOW_DEBUG(uint64_t start_time = get_counter();)

    // Note: this app does not write inputs/read outputs to lighten CPU load
    while (inputs_remaining != 0 || outputs_remaining != 0) {
        if (inputs_remaining != 0) {
            if (m.input_flag.load() == 0) {
                // Inform the accelerator to start.
                m.input_flag.store(1);
                inputs_remaining--;
            } else {
                sched_yield();
            }
        }
        if (outputs_remaining != 0) {
            if (m.output_flag.load() == 1) {
                // Reset for next iteration.
                m.output_flag.store(0);
                outputs_remaining--;

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

    // Release all resources for this model
    m.release();
}

#endif // __FCNN_H__
