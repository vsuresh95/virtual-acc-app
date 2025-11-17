#define _GNU_SOURCE
#include <helper.h>
#include <nn_module.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <string.h>

// Counter for core affinity
#ifdef DO_CPU_PIN
static uint8_t core_affinity_ctr = 0;
#endif

#define MAX_THREADS 6

#include <trace.h>

extern void vam_log_utilization();

typedef struct thread_args {
    unsigned t_id;
    bool cmd_valid;
    bool kill_thread;
    bool cpu_thread;
    nn_module *cmd_module;
    unsigned cmd_delay;
    unsigned cmd_nprio;
    unsigned iters_done;
#ifndef ENABLE_VAM
    uint64_t active_cycles;
#endif
    uint64_t total_latency;
    struct thread_args *next;
} thread_args;

void *req_thread(void *a) {
    thread_args *args = (thread_args *) a;
    HIGH_DEBUG(printf("[APP] Starting request thread %d...\n", args->t_id);)
    nn_token_t *data = (nn_token_t *) malloc (1);
    bool cmd_valid = false;
    bool cpu_thread = args->cpu_thread;
    nn_module *cmd_module = NULL;
    unsigned cmd_delay;
    unsigned input_iters_done = 0;
    unsigned output_iters_done = 0;
    bool drain_output = false;
    uint64_t start_cycles = 0;
    #ifdef ENABLE_VAM
    const unsigned max_inflight = SM_QUEUE_SIZE * 7; // TODO: assumes max of 6 layers
    uint64_t iter_start[max_inflight];
    #endif
    #ifndef DO_SCHED_RR
    if (cpu_thread) {
        pid_t tid = syscall(SYS_gettid);
        setpriority(PRIO_PROCESS, tid, nice_table[2]);
    }
    #endif
    while (1) {
        // Is there a new command?
        bool valid_check = __atomic_load_n(&(args->cmd_valid), __ATOMIC_ACQUIRE);
        if (valid_check == true) {
            // Are there any pending outputs?
            if (input_iters_done == output_iters_done) {
                // Clear valid
                __atomic_store_n(&(args->cmd_valid), false, __ATOMIC_RELEASE);
                // Need to exit?
                if (args->kill_thread) {
                    return NULL;
                }
                cmd_module = args->cmd_module;
                cmd_delay = args->cmd_delay;
                cmd_valid = true;
                input_iters_done = 0;
                output_iters_done = 0;
                __atomic_store_n(&(args->iters_done), 0, __ATOMIC_RELEASE);
                __atomic_store_n(&(args->total_latency), 0, __ATOMIC_RELEASE);
                drain_output = false;
                #ifndef DO_SCHED_RR
                // Set niceness based on priority
                if (!cpu_thread) {
                    pid_t tid = syscall(SYS_gettid);
                    unsigned nprio = cmd_module->nprio;
                    setpriority(PRIO_PROCESS, tid, nice_table[nprio - 1]);
                }
                #endif
            } else {
                // Pending outputs exist, so drain the outputs first.
                drain_output = true;
            }
        }

        if (cmd_valid) {
            #ifdef ADD_CPU_THREAD            
            if (cpu_thread) {
                const unsigned inner_iters = 16;
                float a0=1.1, a1=1.2, a2=1.3, a3=1.4;
                float b0=0.9, b1=0.8, b2=0.7, b3=0.6;
                float c0=0.0, c1=0.0, c2=0.0, c3=0.0;
                for (unsigned k = 0; k < inner_iters; k++) {
                    c0 = c0 * 1.000001 + a0 * b0;
                    c1 = c1 * 0.999999 + a1 * b1;
                    c2 = c2 * 1.000003 + a2 * b2;
                    c3 = c3 * 0.999997 + a3 * b3;
                    a0 += 0.00001; a1 -= 0.00002;
                    b0 -= 0.00003; b1 += 0.00004;
                }
                __atomic_fetch_add(&(args->iters_done), 1, __ATOMIC_RELEASE);
            } else {
            #endif
                // Drain not pending and timer elapsed
                #ifdef ENABLE_VAM
                if (!drain_output && (get_counter() - start_cycles >= cmd_delay)) {
                    // Input queue not full
                    if (nn_module_req_check(cmd_module, data , 0)) {
                        iter_start[input_iters_done % max_inflight] = get_counter();
                        HIGH_DEBUG(printf("[APP%d] Sent req %d...\n", args->t_id, input_iters_done);)
                        start_cycles = get_counter();
                        input_iters_done++;
                    }
                }
                if (nn_module_rsp_check(cmd_module, data, 0)) {
                    HIGH_DEBUG(printf("[APP%d] Received rsp %d...\n", args->t_id, output_iters_done);)
                    __atomic_fetch_add(&(args->total_latency), get_counter() - iter_start[output_iters_done % max_inflight], __ATOMIC_RELEASE);
                    __atomic_fetch_add(&(args->iters_done), 1, __ATOMIC_RELEASE);
                    output_iters_done++;
                }
                #else
                if (!drain_output && (get_counter() - start_cycles >= cmd_delay)) {
                    uint64_t i_start = get_counter();
                    nn_module_run(cmd_module, data, data, 0, 0, false);
                    __atomic_fetch_add(&(args->total_latency), get_counter() - i_start, __ATOMIC_RELEASE);
                    __atomic_fetch_add(&(args->iters_done), 1, __ATOMIC_RELEASE);
                    HIGH_DEBUG(printf("[APP%d] Ran request %d...\n", args->t_id, args->iters_done);)
                    start_cycles = get_counter();
                }
                #endif
            #ifdef ADD_CPU_THREAD            
            }
            #endif
        }
        SCHED_YIELD;
    }
}

// Example application for fully connected neural network (FCNN)
int main(int argc, char **argv) {
    const char (*model_list)[256];
    unsigned n_threads = 4;
    unsigned test_type = 0;
    if (argc > 2) n_threads = atoi(argv[2]);
    if (argc > 1) test_type = atoi(argv[1]);
    const trace_entry_t (*trace)[MAX_THREADS];
    unsigned num_epochs = 0;
    switch (test_type) {
        case 0: {
            // Light workload
            model_list = light_models;
            trace = light_trace;
            num_epochs = sizeof(light_trace) / sizeof(light_trace[0]);
            break;
        }
        case 1: {
            // Heavy workload
            model_list = heavy_models;
            trace = heavy_trace;
            num_epochs = sizeof(heavy_trace) / sizeof(heavy_trace[0]);
            break;
        }
        case 2: {
            // Mixed workload
            model_list = mixed_models;
            trace = mixed_trace;
            num_epochs = sizeof(mixed_trace) / sizeof(mixed_trace[0]);
            break;
        }
        default: break;
    }
    printf("[FILTER] Starting app %s, %s for %d threads for test %d\n", sm_print, vam_print, n_threads, test_type);

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

    // Create 4 models
    thread_args *head = NULL;
    #ifdef ADD_CPU_THREAD
    unsigned n_cpu_threads = 1;
    #else
    unsigned n_cpu_threads = 0;
    #endif
    pthread_t req_threads[n_threads + 1];

    // Create n_threads of models in a thread_args list
    for (int i = 0; i < n_threads + n_cpu_threads; i++) {
        thread_args *args = (thread_args *) malloc (sizeof(thread_args));
        args->t_id = i+1;
        args->cmd_valid = false;
        args->kill_thread = false;
        args->next = NULL;
        args->cmd_delay = 0;
        args->iters_done = 0;
        args->cpu_thread = (i == n_threads);
        #ifndef ENABLE_VAM
        args->active_cycles = 0;
        #endif

        if (i < n_threads) {
            nn_module *cmd_module = (nn_module *) malloc (sizeof(nn_module));
            cmd_module->id = i+1;
            cmd_module->nprio = 1;
            #ifndef ENABLE_SM
            cmd_module->cpu_invoke = true; // Create a CPU thread to invoke the accelerator
            #else
            cmd_module->cpu_invoke = false;
            #endif
            nn_module_load_and_register(cmd_module, model_list[i]);
            args->cmd_module = cmd_module;
        }
        
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

    // Variables for monitoring
    unsigned total_done, old_done;

    // Wait for all threads to wake up
    sleep(1);

    unsigned sleep_seconds = 4;
    unsigned stall_counter = 0;

    for (int epoch = 0; epoch < num_epochs; epoch++) {
        // Assign the next command to all thread
        thread_args *args = head;
        for (unsigned i = 0; i < n_threads + n_cpu_threads; i++) {
            args->cmd_delay = trace[epoch][i].delay;
            args->cmd_nprio = trace[epoch][i].nprio;
            __atomic_store_n(&(args->cmd_valid), trace[epoch][i].valid, __ATOMIC_SEQ_CST);
            args = args->next;
        }
        // Sleep for the rest of the epoch
        sleep(sleep_seconds);
        // Monitor progress of threads
        total_done = 0;
        printf("[FILTER] ");
        for (unsigned i = 0; i < n_threads; i++) {
            unsigned new_iters_done = __atomic_load_n(&args->iters_done, __ATOMIC_ACQUIRE);
            uint64_t avg_latency = __atomic_load_n(&args->total_latency, __ATOMIC_ACQUIRE) / ((new_iters_done > 0) ? new_iters_done : UINT64_MAX);
            total_done += new_iters_done;
            #ifndef ENABLE_VAM
            uint64_t util_cycles = args->cmd_module->active_cycles - args->active_cycles;
            args->active_cycles = args->cmd_module->active_cycles;
            float util = (float) (util_cycles)/(sleep_seconds * 78125000);   
            printf("%lu, %0.2f%%, ", avg_latency, util * 100);
            #else
            printf("%lu, ", avg_latency);
            #endif
            args = args->next;
        }
        #ifdef ADD_CPU_THREAD
        // Monitor CPU thread progress
        {
            unsigned new_iters_done = __atomic_load_n(&args->iters_done, __ATOMIC_ACQUIRE);
            float ips = (float) (new_iters_done) / sleep_seconds;
            printf("%0.2f", ips);
        }
        #endif
        if (total_done != 0 && total_done == old_done) {
            printf("STALL!!!");
            stall_counter++;
            if (stall_counter >= 3) {
                printf("\n[MAIN] Detected stall for 3 consecutive periods, exiting...\n");
                goto exit;
            }
        } else {
            old_done = total_done;
            stall_counter = 0;
        }
        printf("\n");
        #ifdef ENABLE_VAM
        // Write the current utilization to the log
        vam_log_utilization();
        #endif
    }
    printf("[MAIN] Completed all epochs, exiting...\n");

    // Wait for all request threads to finish
    thread_args *args = head;
    for (int i = 0; i < n_threads + n_cpu_threads; i++) {
        args->kill_thread = true;
        __atomic_store_n(&(args->cmd_valid), true, __ATOMIC_SEQ_CST);
        pthread_join(req_threads[i], NULL);
        args = args->next;
    }

    // Release all modules
    args = head;
    do {
        nn_module *cmd_module = args->cmd_module;
        if (!args->cpu_thread) {
            nn_module_release(cmd_module);
            free(cmd_module);
        }
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
