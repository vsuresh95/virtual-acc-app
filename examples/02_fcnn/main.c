#define _GNU_SOURCE
#include <helper.h>
#include <nn_module.h>

unsigned iterations = 100;

// Example application for fully connected neural network (FCNN)
int main(int argc, char **argv) {
    if (argc > 1) {
        iterations = atoi(argv[1]);
	}
    printf("[APP] Starting app: FCNN, %d iters!\n", iterations);
    unsigned errors = 0;
    uint64_t t_load = 0;
    uint64_t t_register = 0;
    uint64_t t_req = 0;
    uint64_t t_rsp = 0;
    uint64_t t_release = 0;
    uint64_t t_start = 0;

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
    #endif
    #ifdef DO_SCHED_RR
    // Set scheduling attributes
    struct sched_param sp = { .sched_priority = 1 };
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp) != 0) {
        perror("pthread_setschedparam");
    }
    #endif

    for (int i = 0; i < iterations; i++) {
        // Load a model from a text file (and) register with NN frontend
        t_start = get_counter();
        nn_module *m = (nn_module *) malloc (sizeof(nn_module));
        m->id = 0;
        m->nprio = 1;
        #ifndef ENABLE_SM
        m->cpu_invoke = true; // Create a CPU thread to invoke the accelerator
        #else
        m->cpu_invoke = false;
        #endif
        nn_module_load(m, "model.txt");
        t_load += get_counter() - t_start;
        t_start = get_counter();
        nn_module_register(m);
        t_register += get_counter() - t_start;

        // Push a new input to the model queue
        t_start = get_counter();
        nn_token_t *input_data = NULL, *output_data = NULL;
        unsigned input_len = 0, output_len = 0;
        LOW_DEBUG(
            input_len = 4096;
            output_len = 64;
            input_data = (nn_token_t *) malloc (input_len * sizeof(nn_token_t));
            initialize_data("input.txt", input_data, input_len);
            output_data = (nn_token_t *) malloc (output_len * sizeof(nn_token_t));
        )
        nn_module_req(m, input_data, input_len * sizeof(nn_token_t));
        t_req += get_counter() - t_start;
        // Wait for the output to be ready
        t_start = get_counter();
        nn_module_rsp(m, output_data, output_len * sizeof(nn_token_t));
        t_rsp += get_counter() - t_start;

        LOW_DEBUG(
            // Load reference output
            nn_token_t *gold_data = (nn_token_t *) malloc (output_len * sizeof(nn_token_t));
            initialize_data("output.txt", gold_data, output_len);

            // Compare with actual output
            for (unsigned j = 0; j < output_len; j++) {
                if ((fabs(nn_token_to_float(nn_token_sub(gold_data[j], output_data[j]))) / fabs(nn_token_to_float(gold_data[j]))) > ERR_TH) {
                    if (errors < 2) { HIGH_DEBUG(printf("\tGOLD[%u] = %f vs %f = out[%u]\n", j, nn_token_to_float(gold_data[j]), nn_token_to_float(output_data[j]), j);) }
                    errors++;
                }
            }
            printf("[APP] Relative error > %.02f for %d values out of %d\n", ERR_TH, errors, output_len);
        )

        t_start = get_counter();
        nn_module_release(m);
        free(m);
        LOW_DEBUG(free(gold_data);)
        t_release += get_counter() - t_start;

        if (i % 10 == 0) printf("[APP] Iter %d done!\n", i);
    }

    printf("Errors = %d\n", errors);
    printf("Load = %lu\n", t_load/iterations);
    printf("Register = %lu\n", t_register/iterations);
    printf("Request = %lu\n", t_req/iterations);
    printf("Response = %lu\n", t_rsp/iterations);
    printf("Release = %lu\n", t_release/iterations);
}
