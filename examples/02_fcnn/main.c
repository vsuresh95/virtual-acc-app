#define _GNU_SOURCE
#include <helper.h>
#include <nn_module.h>

// Example application for fully connected neural network (FCNN)
int main(int argc, char **argv) {
    printf("[APP] Starting app: FCNN!\n");
    unsigned errors = 0;

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

    // Load a model from a text file (and) register with NN frontend
    nn_module *m = (nn_module *) malloc (sizeof(nn_module));
    m->id = 0;
    m->nprio = 1;
    m->cpu_invoke = true;
    nn_module_load_and_register(m, "model.txt");

    // Push a new input to the model queue direct from file
    nn_token_t *output_data = nn_module_forward_file(m, "input.txt");

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
    LOW_DEBUG(printf("[APP] Relative error > %.02f for %d values out of 64\n", ERR_TH, errors);)

    nn_module_release(m);
    free(m);
    free(gold_data);

    printf("Errors = %d\n", errors);
}
