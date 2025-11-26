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

extern void vam_log_utilization();

const char model_list[4][256] = {
    "models/model_16_2.txt",
    "models/model_32_2.txt",
    "models/model_48_2.txt",
    "models/model_64_2.txt",
};

unsigned model_period[4] = {
    55000, 160000, 360000, 760000
};

typedef struct thread_args {
    unsigned t_id;
    bool kill_thread;
    bool cmd_run;
    nn_module *cmd_module;
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

void *fg_func(void *a) {
    thread_args *args = (thread_args *) a;
    HIGH_DEBUG(printf("[APP] Starting FG request thread...\n");)
    nn_token_t *data = (nn_token_t *) malloc (1);
    nn_module *cmd_module = args->cmd_module;
    unsigned cmd_period = args->cmd_period;
    unsigned cmd_deadline = args->cmd_deadline;
    uint64_t next_iter_start = get_counter();

    #ifndef DO_SCHED_RR
    // Set niceness based on priority
    pid_t tid = syscall(SYS_gettid);
    setpriority(PRIO_PROCESS, tid, nice_table[0]);
    #endif

    while(!__atomic_load_n(&args->cmd_run, __ATOMIC_ACQUIRE)) SCHED_YIELD; // Wait for start signal

    while (1) {
        if (args->kill_thread) {
            return NULL;
        }
        uint64_t latency_start = get_counter();
        if (latency_start >= next_iter_start) {
            nn_module_run(cmd_module, data, data, 0, 0, false);
            uint64_t latency_end = get_counter();
            uint64_t time_taken = latency_end - latency_start;
            __atomic_fetch_add(&(args->total_latency), time_taken, __ATOMIC_RELEASE);
            __atomic_fetch_add(&(args->iters_done), 1, __ATOMIC_RELEASE);
            uint64_t expected_next_iter = latency_start + cmd_period;
            next_iter_start = (latency_end > expected_next_iter) ? latency_end : expected_next_iter;
            if (time_taken > cmd_deadline) {
                __atomic_fetch_add(&(args->violations), 1, __ATOMIC_RELEASE);
            }
            HIGH_DEBUG(printf("[APP%d] Ran request %d...\n", args->t_id, args->iters_done);)
        }
        SCHED_YIELD;
    }
}

void *bg_func(void *a) {
    thread_args *args = (thread_args *) a;
    HIGH_DEBUG(printf("[APP] Starting BG request thread...\n");)
    nn_token_t *data = (nn_token_t *) malloc (1);
    nn_module *cmd_module = args->cmd_module;
    unsigned input_iters_done = 0;
    unsigned output_iters_done = 0;
    bool drain_output = false;

    #ifndef DO_SCHED_RR
    // Set niceness based on priority
    pid_t tid = syscall(SYS_gettid);
    setpriority(PRIO_PROCESS, tid, nice_table[1]);
    #endif

    while(!__atomic_load_n(&args->cmd_run, __ATOMIC_ACQUIRE)) SCHED_YIELD; // Wait for start signal

    while (1) {
        // Need to exit?
        if (args->kill_thread) {
            if (input_iters_done != output_iters_done) {
                drain_output = true;
            } else {
                return NULL;
            }
        }

        #ifdef ENABLE_VAM
        if (!drain_output) {
            if (nn_module_req_check(cmd_module, data , 0)) {
                HIGH_DEBUG(printf("[APP%d] Sent req %d...\n", args->t_id, input_iters_done);)
                input_iters_done++;
            }
        }
        if (nn_module_rsp_check(cmd_module, data, 0)) {
            __atomic_fetch_add(&(args->iters_done), 1, __ATOMIC_RELEASE);
            output_iters_done++;
            HIGH_DEBUG(printf("[APP%d] Received rsp %d...\n", args->t_id, output_iters_done);)
        }
        #else
        nn_module_run(cmd_module, data, data, 0, 0, false);
        __atomic_fetch_add(&(args->iters_done), 1, __ATOMIC_RELEASE);
        HIGH_DEBUG(printf("[APP%d] Ran request %d...\n", args->t_id, args->iters_done);)
        #endif
        SCHED_YIELD;
    }
}

// Example application for fully connected neural network (FCNN)
int main(int argc, char **argv) {
    unsigned bg_model = 0;
    unsigned fg_model = 0;
    if (argc > 2) fg_model = atoi(argv[2]);
    if (argc > 1) bg_model = atoi(argv[1]);
    const char *fg_model_file = model_list[fg_model];
    const char *bg_model_file = model_list[bg_model];
    printf("[FILTER%d%d] Starting app %s %s, %s for BG model=%s, FG model=%s\n", bg_model, fg_model, sm_print, mozart_print, vam_print, bg_model_file, fg_model_file);

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

    pthread_t fg_thread, bg_thread;

    // Create 1 thread for high frequency low-priority background task
    thread_args *bg_args = (thread_args *) malloc (sizeof(thread_args));
    bg_args->t_id = 1;
    bg_args->kill_thread = false;
    bg_args->next = NULL;
    bg_args->cmd_period = 0;
    bg_args->cmd_deadline = 0;
    bg_args->iters_done = 0;
    bg_args->violations = 0;
    bg_args->total_latency = 0;
    #ifndef ENABLE_VAM
    bg_args->active_cycles = 0;
    bg_args->prev_active_cycles = 0;
    #endif
        
    nn_module *bg_module = (nn_module *) malloc (sizeof(nn_module));
    bg_module->id = 1;
    bg_module->nprio = 1;
    #ifndef ENABLE_SM
    bg_module->cpu_invoke = true; // Create a CPU thread to invoke the accelerator
    #else
    bg_module->cpu_invoke = false;
    #endif
    nn_module_load_and_register(bg_module, bg_model_file);
    bg_args->cmd_module = bg_module;
    
    if (pthread_create(&bg_thread, &attr, bg_func, (void *) bg_args) != 0) {
        perror("pthread_create");
        exit(1);
    }
      
    // Create 1 thread for low frequency high-priority foreground task  
    thread_args *fg_args = (thread_args *) malloc (sizeof(thread_args));
    fg_args->t_id = 2;
    fg_args->kill_thread = false;
    fg_args->next = NULL;
    fg_args->cmd_period = 5 * model_period[fg_model];
    fg_args->cmd_deadline = model_period[fg_model];
    fg_args->iters_done = 0;
    fg_args->violations = 0;
    fg_args->total_latency = 0;
    #ifndef ENABLE_VAM
    fg_args->active_cycles = 0;
    fg_args->prev_active_cycles = 0;
    #endif
        
    nn_module *fg_module = (nn_module *) malloc (sizeof(nn_module));
    fg_module->id = 2;
    fg_module->nprio = 1;
    #ifndef ENABLE_SM
    fg_module->cpu_invoke = true; // Create a CPU thread to invoke the accelerator
    #else
    fg_module->cpu_invoke = false;
    #endif
    nn_module_load_and_register(fg_module, fg_model_file);
    fg_args->cmd_module = fg_module;
    
    if (pthread_create(&fg_thread, &attr, fg_func, (void *) fg_args) != 0) {
        perror("pthread_create");
        exit(1);
    }

    pthread_attr_destroy(&attr);

    // Wait for all threads to wake up
    sleep(1);

    __atomic_store_n(&(bg_args->cmd_run), true, __ATOMIC_SEQ_CST);
    __atomic_store_n(&(fg_args->cmd_run), true, __ATOMIC_SEQ_CST);

    unsigned sleep_seconds = 2;
    unsigned num_epochs = 10;
    for (int epoch = 0; epoch < num_epochs; epoch++) {
        sleep(sleep_seconds);

        #ifdef ENABLE_VAM
        // Write the current utilization to the log
        vam_log_utilization();
        #else
        uint64_t new_active_cycles = bg_args->cmd_module->active_cycles;
        bg_args->active_cycles = new_active_cycles - bg_args->prev_active_cycles;
        bg_args->prev_active_cycles = new_active_cycles;
        new_active_cycles = fg_args->cmd_module->active_cycles;
        fg_args->active_cycles = new_active_cycles - fg_args->prev_active_cycles;
        fg_args->prev_active_cycles = new_active_cycles;
        #endif

        printf("[FILTER%d%d] ", bg_model, fg_model);
        unsigned bg_iters = __atomic_load_n(&(bg_args->iters_done), __ATOMIC_ACQUIRE);
        unsigned fg_iters = __atomic_load_n(&(fg_args->iters_done), __ATOMIC_ACQUIRE);
        printf("Epoch %d: BG iters=%d, FG iters=%d\n", epoch, bg_iters, fg_iters);
    }

    // Monitor progress of threads
    printf("[FILTER%d%d] ", bg_model, fg_model);
    // Measure the throughput of BG thread
    unsigned bg_iters_done = __atomic_load_n(&(bg_args->iters_done), __ATOMIC_ACQUIRE);
    float bg_throughput = (float) bg_iters_done / (float) (sleep_seconds * num_epochs);
    printf("BG: %0.2f iters/s, ", bg_throughput);
    #ifndef ENABLE_VAM
    float bg_util = (float) (bg_args->active_cycles)/(sleep_seconds * 78125000 * num_epochs);   
    printf("%0.2f%%, ", bg_util * 100);
    #endif

    // Measure the latency and deadline violations of FG thread
    uint64_t fg_total_latency = __atomic_load_n(&(fg_args->total_latency), __ATOMIC_ACQUIRE);
    unsigned fg_iters_done = __atomic_load_n(&(fg_args->iters_done), __ATOMIC_ACQUIRE);
    unsigned fg_violations = __atomic_load_n(&(fg_args->violations), __ATOMIC_ACQUIRE);
    uint64_t fg_avg_latency = (fg_iters_done > 0) ? (fg_total_latency / fg_iters_done) : 0;
    float fg_violation_rate = (fg_iters_done > 0) ? (float) fg_violations / fg_iters_done * 100 : 0;
    printf("FG: %lu(%0.2f%%), ", fg_avg_latency, fg_violation_rate);
    #ifndef ENABLE_VAM
    float fg_util = (float) (fg_args->active_cycles)/(sleep_seconds * 78125000 * num_epochs);   
    printf("%0.2f%%", fg_util * 100);
    #endif
    printf("\n");

    // Wait for all request threads to finish
    __atomic_store_n(&(bg_args->kill_thread), true, __ATOMIC_SEQ_CST);
    __atomic_store_n(&(fg_args->kill_thread), true, __ATOMIC_SEQ_CST);
    pthread_join(bg_thread, NULL);
    pthread_join(fg_thread, NULL);

    nn_module_release(bg_module);
    free(bg_module);
    free(bg_args);
    nn_module_release(fg_module);
    free(fg_module);
    free(fg_args);

#ifdef ENABLE_VAM
    hpthread_report();
#endif
}
