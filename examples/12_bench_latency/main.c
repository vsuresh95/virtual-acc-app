#define _GNU_SOURCE
#include <helper.h>
#include <nn_module.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

// Counter for core affinity
#ifdef DO_CPU_PIN
static uint8_t core_affinity_ctr = 0;
#endif

#define MAX_THREADS 4

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
    unsigned iters_done;
    uint64_t total_latency;
    unsigned violations;
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
    unsigned cmd_delay;
    unsigned cmd_period;
    unsigned cmd_deadline;
    unsigned iters_done = 0;
    uint64_t total_latency = 0;
    unsigned violations = 0;
    uint64_t next_iter_start = get_counter();

    while (1) {
        // Is there a new command?
        bool valid_check = __atomic_load_n(&(args->cmd_valid), __ATOMIC_ACQUIRE);
        if (valid_check == true) {
            args->iters_done = iters_done;
            args->total_latency = total_latency;
            args->violations = violations;
            // Clear valid
            __atomic_store_n(&(args->cmd_valid), false, __ATOMIC_RELEASE);
            // Need to exit?
            if (args->kill_thread) {
                return NULL;
            }
            iters_done = 0;
            total_latency = 0;
            violations = 0;
            // Check if we can start
            cmd_run = args->cmd_run;
            if (cmd_run) {
                cmd_module = args->cmd_module;
                cmd_delay = args->cmd_delay;
                cmd_period = args->cmd_period;
                cmd_deadline = args->cmd_deadline;
                next_iter_start = get_counter() + cmd_period + cmd_delay;
            }
            #ifndef DO_SCHED_RR
            // Set niceness based on priority
            pid_t tid = syscall(SYS_gettid);
            setpriority(PRIO_PROCESS, tid, nice_table[0]);
            #endif
        }

        if (cmd_run) {
            uint64_t latency_start = get_counter();
            if (latency_start >= next_iter_start) {
                nn_module_run(cmd_module, data, data, 0, 0, false);
                uint64_t latency_end = get_counter();
                uint64_t time_taken = latency_end - latency_start;
                total_latency += time_taken;
                iters_done++;
                uint64_t expected_next_iter = latency_start + cmd_period;
                next_iter_start = (latency_end > expected_next_iter) ? latency_end : expected_next_iter;
                if (time_taken > cmd_deadline) {
                    violations++;
                }
                HIGH_DEBUG(printf("[APP%d] Ran request %d...\n", args->t_id, args->iters_done);)
            }
        }
        SCHED_YIELD;
    }
}

// Example application for fully connected neural network (FCNN)
int main(int argc, char **argv) {
    const char (*model_list)[256];
    unsigned test_type = 0;
    unsigned sleep_seconds = 4;
    float deadline_scale = 1.0;
    unsigned num_epochs = 100;
    if (argc > 2) deadline_scale = atof(argv[2]);
    if (argc > 1) test_type = atoi(argv[1]);
    unsigned *deadlines;
    switch (test_type) {
        case 0: {
            // Light workload
            model_list = light_models;
            deadlines = light_deadlines;
            break;
        }
        case 1: {
            // Heavy workload
            model_list = heavy_models;
            deadlines = heavy_deadlines;
            sleep_seconds = 8;
            num_epochs = 50;
            break;
        }
        case 2: {
            // Mixed workload
            model_list = mixed_models;
            deadlines = mixed_deadlines;
            sleep_seconds = 6;
            num_epochs = 75;
            break;
        }
        default: break;
    }
    printf("[FILTER] Starting app %s %s, %s for test %d\n", sm_print, mozart_print, vam_print, test_type);

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
    pthread_t req_threads[MAX_THREADS];

    // Create n_threads of models in a thread_args list
    for (int i = 0; i < MAX_THREADS; i++) {
        thread_args *args = (thread_args *) malloc (sizeof(thread_args));
        args->t_id = i+1;
        args->cmd_run = false;
        args->cmd_valid = false;
        args->kill_thread = false;
        args->next = NULL;
        args->cmd_delay = 0;
        args->cmd_period = 0;
        args->cmd_deadline = 0;
        args->iters_done = 0;
        args->violations = 0;
        args->total_latency = 0;
        #ifndef ENABLE_VAM
        args->active_cycles = 0;
        args->prev_active_cycles = 0;
        #endif

        nn_module *cmd_module = (nn_module *) malloc (sizeof(nn_module));
        cmd_module->id = i+1;
        cmd_module->nprio = 1;
        cmd_module->n_threads = model_threads[i];
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
    srand(time(NULL));
    sleep(1);

    unsigned cur_period[MAX_THREADS] = {0};
    for (unsigned i = 0; i < MAX_THREADS; i++) {
        cur_period[i] = deadlines[i];
    }

    const float scale_min = 1.0f, scale_max = 4.0f;
    for (unsigned epoch = 0; epoch < num_epochs; epoch++) {
        // Random scaling factor for one thread
        float scale_factor = scale_min + (scale_max - scale_min) * ((float)rand() / (float)RAND_MAX);
        cur_period[epoch % MAX_THREADS] = (unsigned) (scale_factor * (float) deadlines[epoch % MAX_THREADS]);
        // Assign the next command to all thread
        thread_args *args = head;
        for (unsigned i = 0; i < MAX_THREADS; i++) {
            args->cmd_period = cur_period[i];
            args->cmd_deadline = deadline_scale * deadlines[i];
            args->cmd_delay = (unsigned) ((float) (args->cmd_deadline / MAX_THREADS) * i);
            args->cmd_run = true;
            args = args->next;
        }
        // Start all threads for this epoch
        args = head;
        for (unsigned i = 0; i < MAX_THREADS; i++) {
            __atomic_store_n(&(args->cmd_valid), true, __ATOMIC_SEQ_CST);
            args = args->next;
        }

        // Sleep for the rest of the epoch
        sleep(sleep_seconds);

        #ifdef ENABLE_VAM
        // Write the current utilization to the log
        vam_log_utilization();
        #else
        for (unsigned i = 0; i < MAX_THREADS; i++) {
            uint64_t new_active_cycles = args->cmd_module->active_cycles;
            args->active_cycles = new_active_cycles - args->prev_active_cycles;
            args->prev_active_cycles = new_active_cycles;
            args = args->next;
        }
        #endif

        // Synchronize all threads at the end of the epoch
        args = head;
        for (unsigned i = 0; i < MAX_THREADS; i++) {
            args->cmd_run = false;
            __atomic_store_n(&(args->cmd_valid), true, __ATOMIC_SEQ_CST);
            args = args->next;
        }
        args = head;
        for (unsigned i = 0; i < MAX_THREADS; i++) {
            while (__atomic_load_n(&(args->cmd_valid), __ATOMIC_SEQ_CST) == true) {
                SCHED_YIELD;
            }
            args = args->next;
        }
        // Monitor progress of threads
        printf("[FILTER] ");
        for (unsigned i = 0; i < MAX_THREADS; i++) {
            unsigned iters_done = args->iters_done;
            uint64_t avg_latency = (iters_done > 0) ? (args->total_latency / iters_done) : 0;
            float violation_rate = (iters_done > 0) ? (float) args->violations / iters_done * 100 : 0;
            #ifndef ENABLE_VAM
            float util = (float) (args->active_cycles)/(sleep_seconds * 78125000);   
            printf("%lu(%0.2f%%), %0.2f%%, ", avg_latency, violation_rate, util * 100);
            #else
            printf("%lu(%0.2f%%), ", avg_latency, violation_rate);
            #endif
            args = args->next;
        }
        printf("\n");
    }
    printf("[MAIN] Completed all epochs, exiting...\n");

    // Wait for all request threads to finish
    thread_args *args = head;
    for (int i = 0; i < MAX_THREADS; i++) {
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
