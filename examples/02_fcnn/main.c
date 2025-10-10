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
    uint64_t t_forward = 0;
    uint64_t t_release = 0;
    uint64_t t_start = 0;

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
    // Set scheduling attributes
    struct sched_param sp = { .sched_priority = 1 };
    if (pthread_setschedparam(pthread_self(), SCHED_RR, &sp) != 0) {
        perror("pthread_setschedparam");
    }

    for (int i = 0; i < iterations; i++) {
        // Load a model from a text file (and) register with NN frontend
        t_start = get_counter();
        nn_module *m = (nn_module *) malloc (sizeof(nn_module));
        m->id = 0;
        m->nprio = 1;
        m->cpu_invoke = true;
        nn_module_load(m, "model.txt");
        t_load += get_counter() - t_start;
        t_start = get_counter();
        nn_module_register(m);
        t_register += get_counter() - t_start;

        // Push a new input to the model queue direct from file
        t_start = get_counter();
        nn_token_t *output_data = nn_module_forward_file(m, "input.txt");
        t_forward += get_counter() - t_start;

        LOW_DEBUG(
            // Load reference output
            nn_token_t *gold_data = (nn_token_t *) malloc (64 * sizeof(nn_token_t));
            initialize_data("output.txt", gold_data, 64);

            // Compare with actual output
            for (unsigned j = 0; j < 64; j++) {
                if ((fabs(nn_token_to_float(nn_token_sub(gold_data[j], output_data[j]))) / fabs(nn_token_to_float(gold_data[j]))) > ERR_TH) {
                    if (errors < 2) { HIGH_DEBUG(printf("\tGOLD[%u] = %f vs %f = out[%u]\n", j, nn_token_to_float(gold_data[j]), nn_token_to_float(output_data[j]), j);) }
                    errors++;
                }
            }
            printf("[APP] Relative error > %.02f for %d values out of 64\n", ERR_TH, errors);
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
    printf("Forward = %lu\n", t_forward/iterations);
    printf("Release = %lu\n", t_release/iterations);
}
