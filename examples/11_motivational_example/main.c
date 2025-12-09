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

#include <trace.h>

extern void vam_log_utilization();

typedef struct thread_args {
    unsigned t_id;
    bool cmd_valid;
    bool cmd_run;
    bool kill_thread;
    nn_module *cmd_module;
    unsigned cmd_period;
    unsigned cmd_nprio;
    unsigned iters_done;
#ifndef ENABLE_VAM
    uint64_t active_cycles;
    uint64_t prev_active_cycles;
#endif
    struct thread_args *next;
} thread_args;

void *req_thread(void *a) {
    thread_args *args = (thread_args *) a;
    HIGH_DEBUG(printf("[APP] Starting request thread %d...\n", args->t_id);)
    nn_token_t *data = (nn_token_t *) malloc (1);
    bool cmd_run = false;
    nn_module *cmd_module = NULL;
    unsigned cmd_period;
    unsigned input_iters_done = 0;
    unsigned output_iters_done = 0;
    bool drain_output = false;
    uint64_t next_iter_start = get_counter();

    while (1) {
        // Is there a new command?
        bool valid_check = __atomic_load_n(&(args->cmd_valid), __ATOMIC_ACQUIRE);
        if (valid_check == true) {
            if (input_iters_done == output_iters_done) {
                args->iters_done = output_iters_done;
                // Clear valid
                __atomic_store_n(&(args->cmd_valid), false, __ATOMIC_RELEASE);
                // Need to exit?
                if (args->kill_thread) {
                    return NULL;
                }
                input_iters_done = 0;
                output_iters_done = 0;
                drain_output = false;
                // Check if we can start
                cmd_run = args->cmd_run;
                if (cmd_run) {
                    cmd_module = args->cmd_module;
                    cmd_period = args->cmd_period;
                    next_iter_start = get_counter() + cmd_period;
                }
                #ifndef DO_SCHED_RR
                // Set niceness based on priority
                pid_t tid = syscall(SYS_gettid);
                unsigned nprio = args->cmd_nprio;
                setpriority(PRIO_PROCESS, tid, nice_table[nprio - 1]);
                #endif
            } else {
                drain_output = true;
            }
        }

        if (cmd_run) {
            #ifdef ENABLE_VAM
            // Drain not pending and timer elapsed
            uint64_t current_time = get_counter();
            if (current_time >= next_iter_start && !drain_output) {
                // Input queue not full
                if (nn_module_req_check(cmd_module, data , 0)) {
                    HIGH_DEBUG(printf("[APP%d] Sent req %d...\n", args->t_id, input_iters_done);)
                    next_iter_start = current_time + cmd_period;
                    input_iters_done++;
                }
            }
            if (nn_module_rsp_check(cmd_module, data, 0)) {
                HIGH_DEBUG(printf("[APP%d] Received rsp %d...\n", args->t_id, output_iters_done);)
                output_iters_done++;
            }
            #else
            uint64_t latency_start = get_counter();
            if (latency_start >= next_iter_start) {
                nn_module_run(cmd_module, data, data, 0, 0, false);
                uint64_t latency_end = get_counter();
                input_iters_done++;
                output_iters_done++;
                uint64_t expected_next_iter = latency_start + cmd_period;
                next_iter_start = (latency_end > expected_next_iter) ? latency_end : expected_next_iter;
                HIGH_DEBUG(printf("[APP%d] Ran request %d...\n", args->t_id, args->iters_done);)
            }
            #endif
        }
        SCHED_YIELD;
    }
}

// Example application for fully connected neural network (FCNN)
int main(int argc, char **argv) {
    unsigned sleep_seconds = 2;
    unsigned model_threads = 0;
    #if !defined(ENABLE_SM) || defined(ENABLE_MOZART)
    model_threads = 1; // Mozart: 1 thread per model
    if (argc > 1) {
        model_threads = atoi(argv[1]);
    }
    #endif
    printf("[FILTER] Starting motivational app %s %s, %s with %d threads per model\n", sm_print, mozart_print, vam_print, model_threads);

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
    pthread_t req_threads[N_THREADS];

    // Create N_THREADS of models in a thread_args list
    for (int i = 0; i < N_THREADS; i++) {
        thread_args *args = (thread_args *) malloc (sizeof(thread_args));
        args->t_id = i+1;
        args->cmd_run = false;
        args->cmd_valid = false;
        args->kill_thread = false;
        args->next = NULL;
        args->cmd_period = 0;
        args->cmd_nprio = 1;
        args->iters_done = 0;
        #ifndef ENABLE_VAM
        args->active_cycles = 0;
        args->prev_active_cycles = 0;
        #endif

        nn_module *cmd_module = (nn_module *) malloc (sizeof(nn_module));
        cmd_module->id = i+1;
        cmd_module->nprio = 1;
        cmd_module->n_threads = model_threads;
        #ifndef ENABLE_SM
        cmd_module->cpu_invoke = true; // Create a CPU thread to invoke the accelerator
        #else
        cmd_module->cpu_invoke = false;
        #endif
        nn_module_load_and_register(cmd_module, model_list[i]);
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

    unsigned num_epochs = sizeof(trace) / sizeof(trace[0]);
    for (unsigned epoch = 0; epoch < num_epochs; epoch++) {
        // Assign the next command to all thread
        thread_args *args = head;
        for (unsigned i = 0; i < N_THREADS; i++) {
            args->cmd_period = trace[epoch][i].period;
            args->cmd_nprio = trace[epoch][i].nprio;
            args->cmd_run = trace[epoch][i].run;
            args = args->next;
        }
        // Start all threads for this epoch
        args = head;
        for (unsigned i = 0; i < N_THREADS; i++) {
            __atomic_store_n(&(args->cmd_valid), true, __ATOMIC_SEQ_CST);
            args = args->next;
        }

        // Sleep for the rest of the epoch
        sleep(sleep_seconds);

        #ifdef ENABLE_VAM
        // Write the current utilization to the log
        vam_log_utilization();
        #else
        for (unsigned i = 0; i < N_THREADS; i++) {
            uint64_t new_active_cycles = args->cmd_module->active_cycles;
            args->active_cycles = new_active_cycles - args->prev_active_cycles;
            args->prev_active_cycles = new_active_cycles;
            args = args->next;
        }
        #endif

        // Synchronize all threads at the end of the epoch
        args = head;
        for (unsigned i = 0; i < N_THREADS; i++) {
            args->cmd_run = false;
            __atomic_store_n(&(args->cmd_valid), true, __ATOMIC_SEQ_CST);
            args = args->next;
        }
        args = head;
        for (unsigned i = 0; i < N_THREADS; i++) {
            while (__atomic_load_n(&(args->cmd_valid), __ATOMIC_SEQ_CST) == true) {
                SCHED_YIELD;
            }
            args = args->next;
        }
        // Monitor progress of threads
        printf("[FILTER] ");
        for (unsigned i = 0; i < N_THREADS; i++) {
            unsigned iters_done = args->iters_done;
            float throughput = (float) iters_done / (float) sleep_seconds;
            #ifndef ENABLE_VAM
            float util = (float) (args->active_cycles)/(sleep_seconds * 78125000);   
            printf("%0.2f, %0.2f%%(%d), ", throughput, util * 100, args->t_id);
            #else
            printf("%0.2f, ", throughput);
            #endif
            args = args->next;
        }
        printf("\n");
    }
    printf("[MAIN] Completed all epochs, exiting...\n");

    // Wait for all request threads to finish
    thread_args *args = head;
    for (int i = 0; i < N_THREADS; i++) {
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
