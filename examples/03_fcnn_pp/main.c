#define _GNU_SOURCE
#include <helper.h>
#include <nn_module.h>

unsigned iterations = 100;

// Example application for fully connected neural network (FCNN)
int main(int argc, char **argv) {
    if (argc > 1) {
        iterations = atoi(argv[1]);
	}
    printf("[APP] Starting app: FCNN pipelined, %d iters!\n", iterations);
    uint64_t t_load_register;
    uint64_t t_forward;
    uint64_t t_release;
    uint64_t t_start;

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
        nn_module_load_and_register(m, "model.txt");
        t_load_register = get_counter() - t_start;

        // Push a new input to the model queue direct from file
        t_start = get_counter();
        nn_module_forward(m);
        t_forward = get_counter() - t_start;

        t_start = get_counter();
        nn_module_release(m);
        free(m);
        t_release = get_counter() - t_start;
    }
    printf("Load register = %lu\n", t_load_register/iterations);
    printf("Forward = %lu\n", t_forward/iterations);
    printf("Release = %lu\n", t_release/iterations);
}
