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

extern void vam_log_utilization();

typedef struct thread_args {
    unsigned t_id;
    bool cmd_valid;
    bool kill_thread;
    nn_module *cmd_module;
    unsigned cmd_delay;
    unsigned iters_done;
#ifndef ENABLE_VAM
    uint64_t active_cycles;
#endif
    struct thread_args *next;
} thread_args;

void *req_thread(void *a) {
    thread_args *args = (thread_args *) a;
    HIGH_DEBUG(printf("[APP] Starting request thread %d...\n", args->t_id);)
    nn_token_t *data = (nn_token_t *) malloc (1);
    bool cmd_valid = false;
    nn_module *cmd_module = NULL;
    unsigned cmd_delay;
    unsigned input_iters_done = 0;
    unsigned output_iters_done = 0;
    bool drain_output = false;
    uint64_t start_cycles = 0;
    #ifndef DO_SCHED_RR
    // Set niceness based on priority
    pid_t tid = syscall(SYS_gettid);
    setpriority(PRIO_PROCESS, tid, nice_table[1]);
    #endif

    while (1) {
        // Is there a new command?
        bool valid_check = __atomic_load_n(&(args->cmd_valid), __ATOMIC_ACQUIRE);
        if (valid_check == true) {
            // Are there any pending outputs?
            if (input_iters_done == output_iters_done) {
                // Clear valid
                __atomic_store_n(&(args->cmd_valid), false, __ATOMIC_RELEASE);
                // If a module is locked currently, free it
                if (cmd_module) {
                    __atomic_store_n(&(cmd_module->module_lock), false, __ATOMIC_SEQ_CST);
                }
                // Need to exit?
                if (args->kill_thread) {
                    return NULL;
                }
                cmd_module = args->cmd_module;
                cmd_delay = args->cmd_delay;
                cmd_valid = true;
                input_iters_done = 0;
                output_iters_done = 0;
                drain_output = false;
                // Wait for the module to be freed by the previous 
                while (__atomic_load_n(&(cmd_module->module_lock), __ATOMIC_SEQ_CST) == true) { SCHED_YIELD; };
                __atomic_store_n(&(cmd_module->module_lock), true, __ATOMIC_SEQ_CST);
            } else {
                // Pending outputs exist, so drain the outputs first.
                drain_output = true;
            }
        }

        if (cmd_valid) {
            // Drain not pending and timer elapsed
            #ifdef ENABLE_VAM
            if (!drain_output && (get_counter() - start_cycles >= cmd_delay)) {
                // Input queue not full
                if (nn_module_req_check(cmd_module, data , 0)) {
                    input_iters_done++;
                    HIGH_DEBUG(printf("[APP%d] Sent req %d...\n", args->t_id, input_iters_done);)
                    start_cycles = get_counter();
                }
            }
            if (nn_module_rsp_check(cmd_module, data, 0)) {
                output_iters_done++;
                HIGH_DEBUG(printf("[APP%d] Received rsp %d...\n", args->t_id, output_iters_done);)
                __atomic_fetch_add(&(args->iters_done), 1, __ATOMIC_RELEASE);
            }
            #else
            if (!drain_output && (get_counter() - start_cycles >= cmd_delay)) {
                nn_module_run(cmd_module, data, data, 0, 0, false);
                input_iters_done++;
                output_iters_done++;
                __atomic_fetch_add(&(args->iters_done), 1, __ATOMIC_RELEASE);
                HIGH_DEBUG(printf("[APP%d] Ran request %d...\n", args->t_id, input_iters_done);)
                start_cycles = get_counter();
            }
            #endif
        }
        SCHED_YIELD;
    }
}

// Example application for fully connected neural network (FCNN)
int main(int argc, char **argv) {
    char model_list[MAX_THREADS][256];
    unsigned model_delay[MAX_THREADS];
    unsigned n_threads = 4;
    char *test_file = "test.txt";
    if (argc > 2) n_threads = atoi(argv[2]);
    if (argc > 1) test_file = argv[1];
    FILE *test = fopen(test_file, "r");
	if (!test) {
		perror("Failed to read test description file");
		exit(1);
	}
	char in_line_buf[256];
    for (int i = 0; i < n_threads; i++) {
        // Read model info
        if (fgets(in_line_buf, 100, test) != NULL) {
            if ((strlen(in_line_buf) > 0) && (in_line_buf[strlen (in_line_buf) - 1] == '\n')) {
                in_line_buf[strlen(in_line_buf) - 1] = '\0';
            }

            sscanf(in_line_buf, "%s %d", model_list[i], &model_delay[i]);
        }
    }
    fclose(test);
    printf("[FILTER] Starting app %s, %s for %d threads from %s!\n", sm_print, vam_print, n_threads, test_file);

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
        args->cmd_valid = false;
        args->kill_thread = false;
        args->next = NULL;
        args->cmd_delay = model_delay[i] * n_threads;
        args->iters_done = 0;
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

    // Variables for monitoring
    unsigned old_iters_done[MAX_THREADS] = {0};
    unsigned total_done, old_done;

    // Wait for all threads to wake up
    sleep(1);

    unsigned num_epochs_done = 10;
    unsigned sleep_seconds = 2;
    unsigned stall_counter = 0;

    // STATIC ASSIGNMENT
    while (num_epochs_done != 0) {
        // Assign the next command to all thread
        thread_args *args = head;
        for (unsigned i = 0; i < n_threads; i++) {
            __atomic_store_n(&(args->cmd_valid), true, __ATOMIC_SEQ_CST);
            args = args->next;
        }
        // Sleep for the rest of the epoch
        sleep(sleep_seconds);
        // Monitor progress of threads
        total_done = 0;
        printf("[FILTER] ");
        for (unsigned i = 0; i < n_threads; i++) {
            unsigned new_iters_done = __atomic_load_n(&args->iters_done, __ATOMIC_ACQUIRE);
            float ips = (float) (new_iters_done - old_iters_done[i]) / sleep_seconds;
            old_iters_done[i] = new_iters_done;
            total_done += new_iters_done;
            #ifndef ENABLE_VAM
            uint64_t util_cycles = args->cmd_module->active_cycles - args->active_cycles;
            args->active_cycles = args->cmd_module->active_cycles;
            float util = (float) (util_cycles)/(sleep_seconds * 78125000);   
            printf("%0.2f, %0.2f%%, ", ips, util * 100);
            #else
            printf("%0.2f, ", ips);
            #endif
            args = args->next;
        }
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
        num_epochs_done--;
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
exit:
#ifdef ENABLE_VAM
    hpthread_report();
#else
    return 0;
#endif
}
