#define _GNU_SOURCE
#include <helper.h>
#include <nn_module.h>
#include <sys/resource.h>
#include <sys/syscall.h>

// Counter for core affinity
#ifdef DO_CPU_PIN
static uint8_t core_affinity_ctr = 0;
#endif

extern void vam_log_utilization();

typedef struct thread_args {
    nn_module *m;
    unsigned iterations;
    bool *start;
    uint64_t average_latency;
    float average_throughput;
    struct thread_args *next;
} thread_args;

void *req_thread(void *a) {
    thread_args *args = (thread_args *) a;
    nn_module *m = args->m;
    HIGH_DEBUG(printf("[APP] Starting request thread %d...\n", m->id);)
    unsigned iterations = args->iterations;
    uint64_t total_latency = 0;
    nn_token_t *input_data = (nn_token_t *) malloc (1);
    bool *start = args->start;
    #ifndef DO_SCHED_RR
    // Set niceness based on priority
    pid_t tid = syscall(SYS_gettid);
    setpriority(PRIO_PROCESS, tid, nice_table[2]);
    #endif
    while(!(*start)) { SCHED_YIELD; } // Wait for signal to start

    uint64_t t_tput_start = get_counter();
    unsigned iters_done = 0;
    while (iters_done < iterations) { 
        uint64_t t_start = get_counter();
        nn_module_run(m, input_data, input_data, 0, 0, false);
        uint64_t t_end = get_counter();
        total_latency += (t_end - t_start);
        iters_done++;
        __atomic_fetch_sub(&args->iterations, 1, __ATOMIC_RELEASE);
    }
    uint64_t t_tput_end = get_counter();
    float seconds_taken = (float)(t_tput_end - t_tput_start) / 78125000.0;
    args->average_throughput = (float) (iterations) / seconds_taken;
    args->average_latency = total_latency / iterations;
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
    printf("[MAIN] Starting app: FCNN multithreaded, %d iters on %d threads from %s!\n", iterations, n_threads, model_file);

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
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp) != 0) {
        perror("pthread_setschedparam");
    }
    #else
    // Set niceness based on priority
    pid_t tid = syscall(SYS_gettid);
    setpriority(PRIO_PROCESS, tid, nice_table[4]);
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
        m->id = i+1;
        m->nprio = 1; // Higher priority for lower thread ID
        m->n_threads = 0;
        #ifndef ENABLE_SM
        m->cpu_invoke = true; // Create a CPU thread to invoke the accelerator
        #else
        m->cpu_invoke = false;
        #endif
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
        if (pthread_attr_setschedpolicy(&attr, SCHED_FIFO) != 0) {
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
        args->next = NULL;
        if (!head) {
            head = args;
            continue;
        }
        thread_args *tmp = head;
        while (tmp->next) tmp = tmp->next;
        tmp->next = args;
    }
    // Create a circular list for thread_args
    thread_args *tail = head;
    while (tail->next != NULL) tail = tail->next;
    tail->next = head;
    
    unsigned old_iters_remaining[n_threads];
    unsigned total_remaining, old_remaining;
    thread_args *th_args = head;
    for (unsigned i = 0; i < n_threads; i++) {
        old_iters_remaining[i] = th_args->iterations;
        total_remaining += old_iters_remaining[i];
        old_remaining += old_iters_remaining[i];
        th_args = th_args->next;
    }

    // Wait for all threads to wake up
    sleep(1);
    // Signal all threads to start
    *start = true;

    // Periodically monitor number of iterations executed by workers
    unsigned sleep_seconds = 2;
    unsigned stall_counter = 0;
    while (total_remaining != 0) {
        thread_args *args = head;
        sleep(sleep_seconds);
        // Write the current utilization to the log
        vam_log_utilization();
        printf("[MAIN] IPS = ");
        total_remaining = 0;
        for (unsigned i = 0; i < n_threads; i++) {
            unsigned new_iters_remaining = __atomic_load_n(&args->iterations, __ATOMIC_ACQUIRE);
            float ips = (float) (old_iters_remaining[i] - new_iters_remaining) / sleep_seconds;
            old_iters_remaining[i] = new_iters_remaining;
            total_remaining += new_iters_remaining;
            printf("T%d:%0.2f(%d), ", i, ips, new_iters_remaining);
            args = args->next;
        }
        if (total_remaining != 0 && total_remaining == old_remaining) {
            printf("STALL!!!");
            stall_counter++;
            if (stall_counter >= 3) {
                printf("\n[MAIN] Detected stall for 3 consecutive periods, exiting...\n");
                goto exit;
            }
        } else {
            old_remaining = total_remaining;
        }
        printf("\n");
        while(sleep_seconds <= 5) sleep_seconds++;
    }

    // Wait for all request threads to finish
    printf("[FILTER] Average time: ");
    uint64_t average_latency = 0;
    float average_throughput = 0.0;
    th_args = head;
    for (int i = 0; i < n_threads; i++) {
        pthread_join(req_threads[i], NULL);
        printf("(%lu, %0.2f), ", th_args->average_latency, th_args->average_throughput);
        average_latency += th_args->average_latency;
        average_throughput += th_args->average_throughput;
        th_args = th_args->next;
    }
    printf("overall: (%lu, %0.2f)\n", average_latency/n_threads, average_throughput);

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
#ifdef ENABLE_VAM
    hpthread_report();
#else
    return 0;
#endif
}
