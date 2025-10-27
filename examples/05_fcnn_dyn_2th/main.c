#define _GNU_SOURCE
#include <helper.h>
#include <sys/timerfd.h>
#include <string.h>
#include <nn_module.h>

// Counter for core affinity
static uint8_t core_affinity_ctr = 0;

typedef struct thread_args {
    nn_module *m;
    unsigned iterations;
    unsigned fps;
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

static inline void th_sleep(unsigned microseconds) {
    // Number of CPU cycles to sleep
    uint64_t cycles_to_sleep = (uint64_t) microseconds * (uint64_t) 72; // Assuming 72 MHz CPU clock
    uint64_t start_cycles = get_counter();
    while ((get_counter() - start_cycles) < cycles_to_sleep) {
        SCHED_YIELD;
    }
}

void *req_thread(void *a) {
    thread_args *args = (thread_args *) a;
    nn_module *m = args->m;
    HIGH_DEBUG(printf("[APP] Starting request thread %d...\n", m->id);)
    unsigned iterations = args->iterations;
    nn_token_t *input_data = (nn_token_t *) malloc (1);
    bool *start = args->start;
    unsigned fps = args->fps;
    while(!(*start)) { SCHED_YIELD; } // Wait for signal to start

    for (int i = 0; i < iterations; i++) {
        // Make new request
        nn_module_req(m, input_data, 0);
        th_sleep(1000000 / fps);
        HIGH_DEBUG(printf("[APP] Thread %d sending request %d...\n", m->id, i);)
    }
    nn_module_release(m);
    free(m);
    return NULL;
}

// Example application for fully connected neural network (FCNN)
int main(int argc, char **argv) {
    unsigned bg_iterations, fg_iterations, bg_fps, fg_fps, bg_prio, fg_prio;
    char bg_model_file[100], fg_model_file[100];
    const char *test_file = "scripts/test.txt";
    if (argc > 1) test_file = argv[1];
    FILE *test = fopen(test_file, "r");
	if (!test) {
		perror("Failed to read test description file");
		exit(1);
	}
	char in_line_buf[100];
    // Read background model info
    if (fgets(in_line_buf, 100, test) != NULL) {
        if ((strlen(in_line_buf) > 0) && (in_line_buf[strlen (in_line_buf) - 1] == '\n')) {
            in_line_buf[strlen(in_line_buf) - 1] = '\0';
        }

        sscanf(in_line_buf, "%s %d %d %d", bg_model_file, &bg_iterations, &bg_fps, &bg_prio);
    }
    // Read foreground model info
    if (fgets(in_line_buf, 100, test) != NULL) {
        if ((strlen(in_line_buf) > 0) && (in_line_buf[strlen (in_line_buf) - 1] == '\n')) {
            in_line_buf[strlen(in_line_buf) - 1] = '\0';
        }

        sscanf(in_line_buf, "%s %d %d %d", fg_model_file, &fg_iterations, &fg_fps, &fg_prio);
    }
    printf("[APP] Starting app: FCNN for BG %s %d prio %d iters %d FPS and FG %s %d prio %d iters %d FPS!\n", bg_model_file, bg_prio, bg_iterations, bg_fps, fg_model_file, fg_prio, fg_iterations, fg_fps);

    // Attributes for all created threads
    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0) {
        perror("attr_init");
    }
    #ifdef DO_CPU_PIN
    // Pin threads to alternating cores in the system (if available)
    long cpu_online = sysconf(_SC_NPROCESSORS_ONLN);
    cpu_set_t set;
    // For self
    CPU_ZERO(&set);
    CPU_SET((core_affinity_ctr++) % cpu_online, &set);
    if (pthread_setaffinity_np(pthread_self(), sizeof(set), &set) != 0) {
        perror("pthread_setaffinity_np");
    }
    // For other threads
    CPU_ZERO(&set);
    CPU_SET((core_affinity_ctr++) % cpu_online, &set);
    if (pthread_attr_setaffinity_np(&attr, sizeof(set), &set) != 0) {
        perror("pthread_attr_setaffinity_np");
    }
    #endif
    #ifdef DO_SCHED_RR
    // Set SCHED_RR scheduling policy with priority 1
    // For self
    struct sched_param sp = { .sched_priority = 1 };
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp) != 0) {
        perror("pthread_setschedparam");
    }
    // For other threads
    if (pthread_attr_setschedpolicy(&attr, SCHED_FIFO) != 0) {
        perror("pthread_attr_setschedpolicy");
    }
    if (pthread_attr_setschedparam(&attr, &sp) != 0) {
        perror("pthread_attr_setschedparam");
    }
    #endif

    // Test start flag
    bool *start = (bool *) malloc (sizeof(bool)); *start = false;

    // Create 1 thread for high frequency low-demand background task
    pthread_t bg_thread;
    thread_args *bg_args = (thread_args *) malloc (sizeof(thread_args));
    nn_module *bg_model = (nn_module *) malloc (sizeof(nn_module));
    bg_model->id = 1;
    bg_model->nprio = bg_prio;
    bg_model->cpu_invoke = true;
    nn_module_load_and_register(bg_model, bg_model_file);
    bg_args->m = bg_model;    
    bg_args->iterations = bg_iterations;
    bg_args->start = start;
    bg_args->fps = bg_fps;
    if (pthread_create(&bg_thread, &attr, req_thread, (void *) bg_args) != 0) {
        perror("pthread_create");
        exit(1);
    }

    // Create 1 thread for low frequency high-demand foreground task
    #ifdef DO_CPU_PIN
    CPU_ZERO(&set);
    CPU_SET((core_affinity_ctr++) % cpu_online, &set);
    if (pthread_attr_setaffinity_np(&attr, sizeof(set), &set) != 0) {
        perror("pthread_attr_setaffinity_np");
    }
    #endif
    pthread_t fg_thread;
    thread_args *fg_args = (thread_args *) malloc (sizeof(thread_args));
    nn_module *fg_model = (nn_module *) malloc (sizeof(nn_module));
    fg_model->id = 2;
    fg_model->nprio = fg_prio;
    fg_model->cpu_invoke = true;
    nn_module_load_and_register(fg_model, fg_model_file);
    fg_args->m = fg_model;    
    fg_args->iterations = fg_iterations;
    fg_args->start = start;
    fg_args->fps = fg_fps;
    if (pthread_create(&fg_thread, &attr, req_thread, (void *) fg_args) != 0) {
        perror("pthread_create");
        exit(1);
    }
    // Circular list
    bg_args->next = fg_args;
    fg_args->next = bg_args;

    // Start a response thread for the thread_args list
    pthread_t rsp_th;
    if (pthread_create(&rsp_th, &attr, rsp_thread, (void *) bg_args) != 0) {
        perror("pthread_create");
        exit(1);
    }
    pthread_attr_destroy(&attr);

    // Wait for all threads to wake up
    sleep(1);
    // Signal all threads to start
    *start = true;

    // Periodically monitor number of iterations executed by workers
    const unsigned sleep_seconds = 2;
    unsigned old_iters_remaining[2] = {bg_iterations, fg_iterations};
    unsigned total_remaining, old_remaining;
    total_remaining = old_remaining = old_iters_remaining[0] + old_iters_remaining[1];
    while (total_remaining != 0) {
        sleep(sleep_seconds);
        printf("[MAIN] IPS = ");
        total_remaining = 0;
        // Background thread
        unsigned new_iters_remaining = __atomic_load_n(&bg_args->iterations, __ATOMIC_ACQUIRE);
        float ips = (float) (old_iters_remaining[0] - new_iters_remaining) / sleep_seconds;
        old_iters_remaining[0] = new_iters_remaining;
        total_remaining += new_iters_remaining;
        printf("BG:%0.2f(%d), ", ips, new_iters_remaining);
        // Foreground thread
        new_iters_remaining = __atomic_load_n(&fg_args->iterations, __ATOMIC_ACQUIRE);
        ips = (float) (old_iters_remaining[1] - new_iters_remaining) / sleep_seconds;
        old_iters_remaining[1] = new_iters_remaining;
        total_remaining += new_iters_remaining;
        printf("FG:%0.2f(%d), ", ips, new_iters_remaining);
        if (total_remaining != 0 && total_remaining == old_remaining) {
            printf("STALL!!!\n");
            goto exit;
        } else {
            old_remaining = total_remaining;
        }
        printf("\n");
    }

    // Wait for all request threads to finish
    pthread_join(bg_thread, NULL);
    pthread_join(fg_thread, NULL);

    // Release all modules
    free(bg_args);
    free(fg_args);
exit:
    hpthread_report();
}
