#include <helper.h>
#include <nn_module.h>

unsigned iterations_done[MAX_THREADS];

void *fcnn_worker(void *arg) {
    unsigned worker_id = (unsigned)(uintptr_t)arg;
    HIGH_DEBUG(printf("[WORKER%d] Starting worker %d!\n", worker_id, worker_id);)
    // Load a model from a text file (and) register with NN module 
    nn_module *m = (nn_module *) malloc (sizeof(nn_module));
    m->id = worker_id;
    m->nprio = (worker_id%4)+1;
    nn_module_load_and_register(m, "model.txt");

    nn_token_t *mem = (nn_token_t *) m->mem;
    volatile uint64_t *input_flag = (volatile uint64_t *) &mem[m->input_flag_offset];
    volatile uint64_t *output_flag = (volatile uint64_t *) &mem[m->output_flag_offset];
    nn_token_t *input_data = &mem[m->input_flag_offset + (PAYLOAD_OFFSET/sizeof(nn_token_t))];
    unsigned inputs_remaining = NUM_ITERATIONS;
    unsigned outputs_remaining = NUM_ITERATIONS;

    initialize_data("\n\0", input_data, 4096);
    LOW_DEBUG(uint64_t start_time = get_counter();)

    // Note: this app does not write inputs/read outputs to lighten CPU load
    while (inputs_remaining != 0 || outputs_remaining != 0) {
        if (inputs_remaining != 0) {
            if (__atomic_load_n(input_flag, __ATOMIC_SEQ_CST) == 0) {
                // Inform the accelerator to start.
                __atomic_store_n(input_flag, 1, __ATOMIC_SEQ_CST);
                inputs_remaining--;
            } else {
                sched_yield();
            }
        }
        if (outputs_remaining != 0) {
            if (__atomic_load_n(output_flag, __ATOMIC_SEQ_CST) == 1) {
                // Reset for next iteration.
                __atomic_store_n(output_flag, 0, __ATOMIC_SEQ_CST);
                outputs_remaining--;

                // Increment flag for main thread
                __atomic_fetch_add(&iterations_done[worker_id], 1, __ATOMIC_SEQ_CST);

                HIGH_DEBUG(printf("[APP%d] Finshed iteration %d!\n", worker_id, iterations_done[worker_id]);)
                LOW_DEBUG(
                    if (outputs_remaining % 1000 == 0) {
                        double ips = (double) pow(10, 9)/((get_counter() - start_time)/1000*12.8);
                        printf("[APP%d] %d; IPS=%0.2f; i/p=%d; o/p=%d\n", worker_id, iterations_done[worker_id], ips, inputs_remaining, outputs_remaining);
                        start_time = get_counter();
                    }
                )
            } else {
                sched_yield();
            }
        }
    }
    nn_module_release(m);
    free(m);
    LOW_DEBUG(printf("[APP%d] Joining worker %d\n", worker_id, worker_id);)
    return NULL;
}

// Example application for fully connected neural network (FCNN)
int main(int argc, char **argv) {
    if (argc > 2) {
		NUM_THREADS = atoi(argv[1]);
		NUM_ITERATIONS = atoi(argv[2]);
    } else if (argc > 1) {
		NUM_THREADS = atoi(argv[1]);
	}
    printf("[MAIN] Starting app: FCNN multithreaded T:%d, I:%d!\n", NUM_THREADS, NUM_ITERATIONS);

    // Start NUM_THREADS threads for the FCNN function
    pthread_t fcnn_th[MAX_THREADS];
    for (unsigned i = 0; i < NUM_THREADS; i++) {
        if (pthread_create(&fcnn_th[i], NULL, fcnn_worker, (void *)(uintptr_t)i) != 0) {
            perror("pthread_create");
            exit(1);
        }
    }

    // Periodically monitor number of iterations executed by workers
    unsigned old_iters[MAX_THREADS] = {};
    unsigned total_iterations = 0;
    const unsigned sleep_seconds = 4;
    unsigned old_total_iterations;
    while (total_iterations != NUM_ITERATIONS * NUM_THREADS) {
        sleep(sleep_seconds);
        total_iterations = 0;
        printf("[MAIN] IPS = ");
        for (unsigned i = 0; i < NUM_THREADS; i++) {
            unsigned new_iters = iterations_done[i];
            float ips = (new_iters - old_iters[i]) / sleep_seconds;
            old_iters[i] = new_iters;
            total_iterations += new_iters;
            printf("T%d:%0.2f(%d), ", i, ips, new_iters);
        }
        if (total_iterations != 0 && total_iterations == old_total_iterations) {
            printf("STALL!!!\n");
            goto exit;
        } else {
            old_total_iterations = total_iterations;
        }
        printf("\n");
    }
    // Join all threads when done
    for (unsigned i = 0; i < NUM_THREADS; i++) {
        pthread_join(fcnn_th[i], NULL);
    }
exit:
    return 0;
}
