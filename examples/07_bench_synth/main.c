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
    bool cmd_run;
    bool kill_thread;
    nn_module *cmd_module;
    unsigned cmd_delay;
    unsigned cmd_period;
    unsigned cmd_deadline;
    unsigned cmd_nprio;
    unsigned iters_done;
    unsigned deadline_violations;
    unsigned fps_violations;
#ifndef ENABLE_VAM
    uint64_t active_cycles;
    uint64_t prev_active_cycles;
#endif
    uint64_t total_latency;
    struct thread_args *next;
} thread_args;

void *req_thread(void *a) {
    thread_args *args = (thread_args *) a;
    HIGH_DEBUG(printf("[APP] Starting request thread %d...\n", args->t_id);)
    nn_token_t *data = (nn_token_t *) malloc (1);
    bool cmd_run = false;
    nn_module *cmd_module = NULL;
    unsigned cmd_delay;
    unsigned cmd_period;
    unsigned cmd_deadline;
    unsigned input_iters_done = 0;
    unsigned output_iters_done = 0;
    unsigned total_latency = 0;
    unsigned deadline_violations = 0;
    unsigned fps_violations = 0;
    bool drain_output = false;
    bool fps_violated = false;
    bool req_pending = false;
    uint64_t next_iter_start = get_counter();
    #if defined(ENABLE_VAM) && !defined(ENABLE_MOZART)
    const unsigned max_inflight = SM_QUEUE_SIZE * 4; // TODO: assumes max of 3 layers
    uint64_t iter_start[max_inflight];
    #endif

    while (1) {
        // Is there a new command?
        bool valid_check = __atomic_load_n(&(args->cmd_valid), __ATOMIC_ACQUIRE);
        if (valid_check == true) {
            // Are there any pending outputs?
            if (input_iters_done == output_iters_done) {
                args->iters_done = output_iters_done;
                args->total_latency = total_latency;
                args->deadline_violations = deadline_violations;
                args->fps_violations = fps_violations;
                // Clear valid
                __atomic_store_n(&(args->cmd_valid), false, __ATOMIC_SEQ_CST);
                // Need to exit?
                if (args->kill_thread) {
                    return NULL;
                }
                drain_output = false;
                fps_violated = false;
                input_iters_done = 0;
                output_iters_done = 0;
                total_latency = 0;
                deadline_violations = 0;
                fps_violations = 0;
                // Check if we can start
                cmd_run = args->cmd_run;
                if (cmd_run) {
                    cmd_module = args->cmd_module;
                    cmd_delay = args->cmd_delay;
                    cmd_period = args->cmd_period;
                    cmd_deadline = args->cmd_deadline;
                    req_pending = false;
                    next_iter_start = get_counter() + cmd_period + cmd_delay;
                }
                #ifndef DO_SCHED_RR
                // Set niceness based on priority
                pid_t tid = syscall(SYS_gettid);
                unsigned nprio = args->cmd_nprio;
                setpriority(PRIO_PROCESS, tid, nice_table[nprio - 1]);
                #endif
            } else {
                // Pending outputs exist, so drain the outputs first.
                drain_output = true;
            }
        }

        if (cmd_run) {
            // Drain not pending and timer elapsed
            if (!req_pending && !drain_output) {
                if (get_counter() >= next_iter_start) {
                    req_pending = true;
                }
            }
            #if defined(ENABLE_VAM) && !defined(ENABLE_MOZART)
            if (req_pending) {
                // Input queue not full
                if (nn_module_req_check(cmd_module, data , 0)) {
                    uint64_t latency_start = get_counter();
                    iter_start[input_iters_done % max_inflight] = latency_start;
                    HIGH_DEBUG(printf("[APP%d] Sent req %d...\n", args->t_id, input_iters_done);)
                    req_pending = false;
                    if (fps_violated) {
                        fps_violations++;
                        next_iter_start = latency_start + cmd_period;
                        fps_violated = false;
                    } else {
                        next_iter_start = next_iter_start + cmd_period;
                    }
                    input_iters_done++;
                } else {
                    fps_violated = true;
                }
            }
            if (nn_module_rsp_check(cmd_module, data, 0)) {
                HIGH_DEBUG(printf("[APP%d] Received rsp %d...\n", args->t_id, output_iters_done);)
                uint64_t latency_start = iter_start[output_iters_done % max_inflight];
                uint64_t latency_end = get_counter();
                total_latency += latency_end - latency_start;
                if (latency_end - latency_start > cmd_deadline) {
                    deadline_violations++;
                }
                output_iters_done++;
            }
            #else
            if (req_pending) {
                uint64_t latency_start = get_counter();
                nn_module_run(cmd_module, data, data, 0, 0, false);
                uint64_t latency_end = get_counter();
                total_latency += latency_end - latency_start;
                output_iters_done++;
                req_pending = false;
                uint64_t expected_next_iter = next_iter_start + cmd_period;
                if (latency_end > expected_next_iter) {
                    fps_violations++;
                    next_iter_start = latency_end;
                } else {
                    next_iter_start = expected_next_iter;
                }
                if (latency_end - latency_start > cmd_deadline) {
                    deadline_violations++;
                }
                HIGH_DEBUG(printf("[APP%d] Ran request %d...\n", args->t_id, args->iters_done);)
            }
            #endif
        }
        SCHED_YIELD;
    }
}

// Example application for fully connected neural network (FCNN)
int main(int argc, char **argv) {
    const char (*model_list)[256];
    float delay_scale = 1.0;
    unsigned th_offset = 0;
    unsigned n_threads = 4;
    unsigned test_type = 0;
    unsigned sleep_seconds = 2;
    if (argc > 4) th_offset = atoi(argv[4]);
    if (argc > 3) n_threads = atoi(argv[3]);
    if (argc > 2) delay_scale = atof(argv[2]);
    if (argc > 1) test_type = atoi(argv[1]);
    const trace_entry_t (*trace)[MAX_THREADS];
    unsigned *deadlines;
    unsigned num_epochs = 0;
    switch (test_type) {
        case 0: {
            // Light workload
            model_list = light_models;
            trace = light_trace;
            deadlines = light_deadlines;
            num_epochs = sizeof(light_trace) / sizeof(light_trace[0]);
            break;
        }
        case 1: {
            // Heavy workload
            model_list = heavy_models;
            trace = heavy_trace;
            deadlines = heavy_deadlines;
            num_epochs = sizeof(heavy_trace) / sizeof(heavy_trace[0]);
            sleep_seconds = 4;
            break;
        }
        case 2: {
            // Mixed workload
            model_list = mixed_models;
            trace = mixed_trace;
            deadlines = mixed_deadlines;
            num_epochs = sizeof(mixed_trace) / sizeof(mixed_trace[0]);
            sleep_seconds = 3;
            break;
        }
        default: break;
    }
    printf("[FILTER] Starting app %s %s, %s for %d threads (offset %d) for test %d (delay %0.2f)\n", sm_print, mozart_print, vam_print, n_threads, th_offset, test_type, delay_scale);

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
    pthread_t req_threads[n_threads];

    // Create n_threads of models in a thread_args list
    for (int i = 0; i < n_threads; i++) {
        thread_args *args = (thread_args *) malloc (sizeof(thread_args));
        args->t_id = i+1;
        args->cmd_run = false;
        args->cmd_valid = false;
        args->kill_thread = false;
        args->next = NULL;
        args->cmd_delay = 0;
        args->cmd_period = 0;
        args->cmd_deadline = 0;
        args->cmd_nprio = 1;
        args->iters_done = 0;
        args->deadline_violations = 0;
        args->fps_violations = 0;
        args->total_latency = 0;
        #ifndef ENABLE_VAM
        args->active_cycles = 0;
        #endif

        nn_module *cmd_module = (nn_module *) malloc (sizeof(nn_module));
        cmd_module->id = i+1;
        cmd_module->nprio = 1;
        #ifndef ENABLE_SM
        cmd_module->cpu_invoke = true; // Create a CPU thread to invoke the accelerator
        #else
        cmd_module->cpu_invoke = false;
        #endif
        nn_module_load_and_register(cmd_module, model_list[i+th_offset]);
        args->cmd_module = cmd_module;
        
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

    // Wait for all threads to wake up
    sleep(1);

    int epoch = 0; int mini_epoch = 0;
    while (epoch < num_epochs) {
        // Assign the next command to all thread
        thread_args *args = head;
        for (unsigned i = 0; i < n_threads; i++) {
            args->cmd_period = trace[epoch][i+th_offset].period;
            args->cmd_deadline = (unsigned) ((float) delay_scale * deadlines[i+th_offset]);
            args->cmd_nprio = trace[epoch][i+th_offset].nprio;
            args->cmd_delay = (unsigned) ((float) (args->cmd_deadline / n_threads) * i);
            #ifdef ENABLE_VAM
            nn_module_setprio(args->cmd_module, args->cmd_nprio);
            #endif
            args->cmd_run = trace[epoch][i+th_offset].run;
            args = args->next;
        }
        // Start all threads for this epoch
        args = head;
        for (unsigned i = 0; i < n_threads; i++) {
            __atomic_store_n(&(args->cmd_valid), true, __ATOMIC_SEQ_CST);
            args = args->next;
        }

        // Sleep for the rest of the epoch
        sleep(sleep_seconds);

        #ifdef ENABLE_VAM
        // Write the current utilization to the log
        vam_log_utilization();
        #else
        for (unsigned i = 0; i < n_threads; i++) {
            uint64_t new_active_cycles = args->cmd_module->active_cycles;
            args->active_cycles = new_active_cycles - args->prev_active_cycles;
            args->prev_active_cycles = new_active_cycles;
            args = args->next;
        }
        #endif

        // Synchronize all threads at the end of the epoch
        args = head;
        for (unsigned i = 0; i < n_threads; i++) {
            args->cmd_run = false;
            __atomic_store_n(&(args->cmd_valid), true, __ATOMIC_SEQ_CST);
            args = args->next;
        }
        args = head;
        for (unsigned i = 0; i < n_threads; i++) {
            while (__atomic_load_n(&(args->cmd_valid), __ATOMIC_SEQ_CST) == true) {
                SCHED_YIELD;
            }
            args = args->next;
        }
        // Monitor progress of threads
        printf("[FILTER] ");
        for (unsigned i = 0; i < n_threads; i++) {
            unsigned iters_done = args->iters_done;
            uint64_t avg_latency = args->total_latency / ((iters_done > 0) ? iters_done : UINT64_MAX);
            unsigned deadline_violations = args->deadline_violations;
            unsigned fps_violations = args->fps_violations;
            #ifndef ENABLE_VAM
            float util = (float) (args->active_cycles)/(sleep_seconds * 78125000);   
            printf("%lu(%d/%d,%d/%d), %0.2f%%, ", avg_latency, deadline_violations, iters_done, fps_violations, iters_done, util * 100);
            #else
            printf("%lu(%d/%d,%d/%d), ", avg_latency, deadline_violations, iters_done, fps_violations, iters_done);
            #endif
            args = args->next;
        }
        printf("\n");
        mini_epoch++;
        epoch = mini_epoch / 8; // Each epoch lasts 8 mini-epochs
    }
    printf("[MAIN] Completed all epochs, exiting...\n");

    // Wait for all request threads to finish
    thread_args *args = head;
    for (int i = 0; i < n_threads; i++) {
        args->kill_thread = true;
        __atomic_store_n(&(args->cmd_valid), true, __ATOMIC_SEQ_CST);
        pthread_join(req_threads[i], NULL);
        args = args->next;
    }

    // Release all modules
    args = head;
    do {
        nn_module *cmd_module = args->cmd_module;
        nn_module_release(cmd_module);
        free(cmd_module);
        thread_args *next = args->next;
        free(args);
        args = next;     
    } while (args != head);
#ifdef ENABLE_VAM
    hpthread_report();
#endif
}
