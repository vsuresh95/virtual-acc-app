#define _GNU_SOURCE
#include <helper.h>
#include <nn_module.h>

// Counter for core affinity
static uint8_t core_affinity_ctr = 0;

typedef struct thread_args {
    nn_module *m;
    unsigned iterations;
    bool *start;
    struct thread_args *next;
} thread_args;

void *rsp_thread(void *a) {
    HIGH_DEBUG(printf("[APP] Starting response thread...\n");)
    thread_args *args = (thread_args *) a;
    nn_token_t *output_data = (nn_token_t *) malloc (1);
    bool *start = args->start;
    while(!(*start)) { SCHED_YIELD; } // Wait for signal to start
    // Iterate through all thread_args and accumulate iterations
    thread_args *head = args;
    unsigned total_iterations = 0;
    do {
        total_iterations += args->iterations;
        args = args->next;        
    } while (args != head);

    while (total_iterations > 0) {
        if (args->iterations == 0) {
            // If no remaining iterations for current thread, skip
            args = args->next;
            continue;
        }
        // Else, check if module_rsp is available
        HIGH_DEBUG(printf("[APP] Rsp thread checking module %d, remaining iters %d...\n", args->m->id, args->iterations);)
        if (nn_module_rsp_check(args->m, output_data, 0)) {
            __atomic_fetch_sub(&args->iterations, 1, __ATOMIC_RELEASE);
            total_iterations--;
        }
        // Iterate to next thread_args
        args = args->next;
        SCHED_YIELD;
    }
    return NULL;
}

void *req_thread(void *a) {
    thread_args *args = (thread_args *) a;
    nn_module *m = args->m;
    HIGH_DEBUG(printf("[APP] Starting request thread %d...\n", m->id);)
    unsigned iterations = args->iterations;
    nn_token_t *input_data = (nn_token_t *) malloc (1);
    bool *start = args->start;
    while(!(*start)) { SCHED_YIELD; } // Wait for signal to start

    for (int i = 0; i < iterations; i++) {
        nn_module_req(m, input_data, 0);
        SCHED_YIELD;
    }
    return NULL;
}

// Example application for fully connected neural network (FCNN)
int main(int argc, char **argv) {
    unsigned iterations = 100;
    unsigned n_threads = 2;
    const char *model_file = "models/model_16_1.txt";
    if (argc > 3) {
        model_file = argv[3];
	}
    if (argc > 2) {
        n_threads = atoi(argv[2]);
	}    
    if (argc > 1) {
        iterations = atoi(argv[1]);
	}
    printf("[APP] Starting app: FCNN multithreaded, %d iters on %d threads from %s!\n", iterations, n_threads, model_file);

    #ifdef DO_CPU_PIN
    // Run main thread on CPU 0 always.
    long cpu_online = sysconf(_SC_NPROCESSORS_ONLN);
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET((core_affinity_ctr++) % cpu_online, &set);
    if (pthread_setaffinity_np(pthread_self(), sizeof(set), &set) != 0) {
        perror("pthread_setaffinity_np");
    }
    #endif
    #ifdef DO_SCHED_RR
    // Set scheduling attributes
    struct sched_param sp = { .sched_priority = 1 };
    if (pthread_setschedparam(pthread_self(), SCHED_RR, &sp) != 0) {
        perror("pthread_setschedparam");
    }
    #endif

    // Create n_threads of models in a thread_args list
    thread_args *head = NULL;
    pthread_t req_threads[n_threads];
    bool *start = (bool *) malloc (sizeof(bool)); *start = false;
    for (int i = 0; i < n_threads; i++) {
        thread_args *args = (thread_args *) malloc (sizeof(thread_args));
        args->iterations = iterations;
        args->start = start;
        args->next = NULL;

        nn_module *m = (nn_module *) malloc (sizeof(nn_module));
        m->id = i;
        m->nprio = 1;
        m->cpu_invoke = true;
        nn_module_load_and_register(m, model_file);
        args->m = m;
        
        // Start a request thread for this model
        pthread_attr_t attr;
        if (pthread_attr_init(&attr) != 0) {
            perror("attr_init");
        }
        #ifdef DO_CPU_PIN
        // Set CPU affinity
        CPU_ZERO(&set);
        CPU_SET((core_affinity_ctr++) % cpu_online, &set);
        if (pthread_attr_setaffinity_np(&attr, sizeof(set), &set) != 0) {
            perror("pthread_attr_setaffinity_np");
        }
        #endif
        #ifdef DO_SCHED_RR
        // Set SCHED_RR scheduling policy with priority 1
        if (pthread_attr_setschedpolicy(&attr, SCHED_RR) != 0) {
            perror("pthread_attr_setschedpolicy");
        }
        if (pthread_attr_setschedparam(&attr, &sp) != 0) {
            perror("pthread_attr_setschedparam");
        }
        #endif
        // Create request pthread
        if (pthread_create(&req_threads[i], &attr, req_thread, (void *) args) != 0) {
            perror("pthread_create");
            exit(1);
        }
        pthread_attr_destroy(&attr);

        // Insert into thread_args list
        args->next = head;
        head = args;
    }
    // Create a circular list for thread_args
    thread_args *tail = head;
    while (tail->next != NULL) tail = tail->next;
    tail->next = head;
    
    // Start a response thread for the thread_args list
    pthread_t rsp_th;
    // Create pthread attributes
    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0) {
        perror("attr_init");
    }
    #ifdef DO_CPU_PIN
    // Set CPU affinity
    CPU_ZERO(&set);
    CPU_SET((core_affinity_ctr++) % cpu_online, &set);
    if (pthread_attr_setaffinity_np(&attr, sizeof(set), &set) != 0) {
        perror("pthread_attr_setaffinity_np");
    }
    #endif
    #ifdef DO_SCHED_RR
    // Set SCHED_RR scheduling policy with priority 1
    if (pthread_attr_setschedpolicy(&attr, SCHED_RR) != 0) {
        perror("pthread_attr_setschedpolicy");
    }
    if (pthread_attr_setschedparam(&attr, &sp) != 0) {
        perror("pthread_attr_setschedparam");
    }
    #endif
    if (pthread_create(&rsp_th, &attr, rsp_thread, (void *) head) != 0) {
        perror("pthread_create");
        exit(1);
    }
    pthread_attr_destroy(&attr);

    // Wait for all threads to wake up
    sleep(1);
    // Signal all threads to start
    *start = true;

    // Periodically monitor number of iterations executed by workers
    unsigned old_iters_remaining[n_threads];
    for (unsigned i = 0; i < n_threads; i++) old_iters_remaining[i] = iterations;
    unsigned total_remaining = n_threads * iterations;
    const unsigned sleep_seconds = 2;
    unsigned old_remaining = n_threads * iterations;
    while (total_remaining != 0) {
        thread_args *args = head;
        sleep(sleep_seconds);
        printf("[MAIN] IPS = ");
        for (unsigned i = 0; i < n_threads; i++) {
            unsigned new_iters_remaining = __atomic_load_n(&args->iterations, __ATOMIC_ACQUIRE);
            unsigned diff = old_iters_remaining[i] - new_iters_remaining;
            float ips = (diff) / sleep_seconds;
            old_iters_remaining[i] = new_iters_remaining;
            total_remaining -= diff;
            printf("T%d:%0.2f(%d), ", i, ips, new_iters_remaining);
            args = args->next;
        }
        if (total_remaining != 0 && total_remaining == old_remaining) {
            printf("STALL!!!\n");
            goto exit;
        } else {
            old_remaining = total_remaining;
        }
        printf("\n");
    }

    // Wait for all request threads to finish
    for (int i = 0; i < n_threads; i++) {
        pthread_join(req_threads[i], NULL);
    }
    pthread_join(rsp_th, NULL);

    // Release all modules
    thread_args *args = head;
    do {
        nn_module *m = args->m;
        nn_module_release(m);
        free(m);
        thread_args *next = args->next;
        free(args);
        args = next;     
    } while (args != head);
exit:
    hpthread_report();
}
