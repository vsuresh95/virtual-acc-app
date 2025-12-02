#define _GNU_SOURCE
#include <helper.h>
#include <nn_module.h>

bool test_active = false;

// Example application for fully connected neural network (FCNN)
int main(int argc, char **argv) {
    unsigned iterations = 100;
    const char *model_file = "models/model_16_1.txt";
    if (argc > 2) {
        model_file = argv[2];
	}
    if (argc > 1) {
        iterations = atoi(argv[1]);
	}
    printf("[APP] Starting app: FCNN Pipelined, %d iters from %s!\n", iterations, model_file);

    #ifdef DO_CPU_PIN
    // Run main thread on CPU 0 always.
    long online = sysconf(_SC_NPROCESSORS_ONLN);
    if (online > 1) {
        cpu_set_t set;
        CPU_ZERO(&set);
        CPU_SET(0, &set);
        if (pthread_setaffinity_np(pthread_self(), sizeof(set), &set) != 0) {
            perror("pthread_setaffinity_np");
        }
    }
    #endif // DO_CPU_PIN
    #ifdef DO_SCHED_RR
    // Set scheduling attributes
    struct sched_param sp = { .sched_priority = 1 };
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp) != 0) {
        perror("pthread_setschedparam");
    }
    #endif // DO_SCHED_RR

    // Load a model from a text file (and) register with NN frontend
    nn_module *m = (nn_module *) malloc (sizeof(nn_module));
    m->id = 0;
    m->nprio = 1;
    m->n_threads = 0;
    #ifndef ENABLE_SM
    m->cpu_invoke = true; // Create a CPU thread to invoke the accelerator
    #else
    m->cpu_invoke = false;
    #endif
    nn_module_load_and_register(m, model_file);

    unsigned print_iterations = iterations / 2;
    unsigned input_iters_remaining = iterations;
    unsigned output_iters_remaining = iterations;
    nn_token_t *data = (nn_token_t *) malloc (1);

    test_active = true;

    uint64_t t_tput_start = get_counter();
    while (input_iters_remaining > 0 || output_iters_remaining > 0) {
        if (input_iters_remaining > 0) {
            if (nn_module_req_check(m, data, 0)) {
                input_iters_remaining--;
                HIGH_DEBUG(printf("[APP] Sent req %d...\n", iterations - input_iters_remaining);)
            }
        }
        if (output_iters_remaining > 0) {
            if (nn_module_rsp_check(m, data, 0)) {
                output_iters_remaining--;
                HIGH_DEBUG(printf("[APP] Received rsp %d...\n", iterations - output_iters_remaining);)
                if (output_iters_remaining % print_iterations == 0) {
                    printf("%d\n", iterations - output_iters_remaining);
                }
            }
        }
        SCHED_YIELD;
    }
    uint64_t t_tput_end = get_counter();

    test_active = false;

    nn_module_release(m);
    free(m);
    hpthread_report();
    printf("[APP] Average throughput = %lu\n", (t_tput_end - t_tput_start)/iterations);
}
