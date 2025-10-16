#define _GNU_SOURCE
#include <helper.h>
#include <nn_module.h>

typedef struct {
    nn_module *m;
    unsigned iterations;
} rsp_thread_args;

void *rsp_thread(void *a) {
    HIGH_DEBUG(printf("[APP] Starting response thread...\n");)
    rsp_thread_args *args = (rsp_thread_args *) a;
    nn_module *m = args->m;
    unsigned iterations = args->iterations;
    nn_token_t *output_data = (nn_token_t *) malloc (1);

    for (int i = 0; i < iterations + 5; i++) {
        nn_module_rsp(m, output_data, 0);
        SCHED_YIELD;
    }
    return NULL;
}

// Example application for fully connected neural network (FCNN)
int main(int argc, char **argv) {
    unsigned iterations = 100;
    const char *model_file = "model.txt";
    if (argc > 2) {
        model_file = argv[2];
	}
    if (argc > 1) {
        iterations = atoi(argv[1]);
	}
    printf("[APP] Starting app: FCNN pipelined, %d iters from %s!\n", iterations, model_file);

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
    nn_module_load_and_register(m, model_file);

    rsp_thread_args *args = (rsp_thread_args *) malloc (sizeof(rsp_thread_args));
    args->m = m;
    args->iterations = iterations;

    // Start response thread on CPU 1
    pthread_t rsp_th;
    // Create pthread attributes
    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0) {
        perror("attr_init");
    }
    // Set CPU affinity
    if (online > 1) {
        cpu_set_t set;
        CPU_ZERO(&set);
        CPU_SET(0, &set);
        if (pthread_attr_setaffinity_np(&attr, sizeof(set), &set) != 0) {
            perror("pthread_attr_setaffinity_np");
        }
    }
    // Set SCHED_RR scheduling policy with priority 1
    if (pthread_attr_setschedpolicy(&attr, SCHED_RR) != 0) {
        perror("pthread_attr_setschedpolicy");
    }
    if (pthread_attr_setschedparam(&attr, &sp) != 0) {
        perror("pthread_attr_setschedparam");
    }
    // Create VAM pthread
    if (pthread_create(&rsp_th, &attr, rsp_thread, (void *) args) != 0) {
        perror("pthread_create");
        exit(1);
    }
    pthread_attr_destroy(&attr);

    // Warm up
    nn_token_t *input_data = (nn_token_t *) malloc (1);
    for (int i = 0; i < 5; i++) {
        nn_module_req(m, input_data, 0);
        SCHED_YIELD;
    }

    uint64_t t_start = get_counter();
    for (int i = 0; i < iterations; i++) {
        nn_module_req(m, input_data, 0);
        SCHED_YIELD;
        LOW_DEBUG( if (i % 100 == 0) printf("[APP] Iter %d done!\n", i); )
    }
    uint64_t t_loop = get_counter() - t_start;
    printf("[APP] Average time = %lu\n", t_loop/iterations);

    pthread_join(rsp_th, NULL);
    nn_module_release(m);
    free(m);
}
