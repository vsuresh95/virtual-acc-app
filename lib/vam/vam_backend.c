#define _GNU_SOURCE
#include <common_helper.h>
#include <hpthread_intf.h>
#include <vam_physical_accel.h>
#include <vam_backend.h>
#include <vam_accel_def.h>
#include <libesp.h>
#include <esp.h>
#include <esp_accelerator.h>
#include <string.h>
#include <sched.h>
#include <dirent.h>
#include <fnmatch.h>
#include <limits.h>

// ESP API for getting contig_alloc handle
extern contig_handle_t *lookup_handle(void *buf, enum contig_alloc_policy *policy);
// External instance of hpthread interface
extern hpthread_intf_t intf;
// Used in wakeup_vam()
pthread_t vam_th;
// Physical accelerator list
physical_accel_t *accel_list = NULL;
physical_accel_t *cpu_thread_list = NULL;
// Maximum loaded and minimuim loaded accel for load balancing
physical_accel_t *max_util_accel = NULL;
physical_accel_t *min_util_accel = NULL;
// Current max, min and avg util
float avg_util, max_util, min_util;
// Safeguard for not performing load balance repeatedly
float load_imbalance_reg;
// Counter for core affinity
static uint8_t core_affinity_ctr = 1;
// Number of CPUs online
long cpu_online;
// Physical accelerator list
hpthread_cand_t *hpthread_cand_list = NULL;

// Function to wake up VAM for the first time
void wakeup_vam() {
	HIGH_DEBUG(printf("[VAM] Launching a new thread for VAM BACKEND!\n");)
    // Find the number of cores available
    cpu_online = sysconf(_SC_NPROCESSORS_ONLN);
    // Create pthread attributes
    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0) {
        perror("attr_init");
    }
    // Set CPU affinity
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(core_affinity_ctr % cpu_online, &set);
    if (pthread_attr_setaffinity_np(&attr, sizeof(set), &set) != 0) {
        perror("pthread_attr_setaffinity_np");
    }
    // Set SCHED_RR scheduling policy with priority 1
    if (pthread_attr_setschedpolicy(&attr, SCHED_RR) != 0) {
        perror("pthread_attr_setschedpolicy");
    }
    struct sched_param sp = { .sched_priority = 1 };
    if (pthread_attr_setschedparam(&attr, &sp) != 0) {
        perror("pthread_attr_setschedparam");
    }
    // Set pthread attributes to be detached; no join required
    if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0) {
        perror("attr_setdetachstate");
    }
    // Create VAM pthread
    if (pthread_create(&vam_th, &attr, vam_run_backend, NULL) != 0) {
        perror("pthread_create");
        exit(1);
    }
    pthread_attr_destroy(&attr);
}

void vam_probe_accel() {
    // Open the devices directory to search for accels
    DIR *dir = opendir("/dev/");
    if (!dir) {
        perror("Failed to open directory");
        exit(1);
    }
    // Search for all stratus accelerators and fill into accel_list
    HIGH_DEBUG(printf("[VAM] Performing device probe.\n");)
    struct dirent *entry;
    unsigned device_id = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (fnmatch("*_stratus.*", entry->d_name, FNM_NOESCAPE) == 0) {
            physical_accel_t *accel_temp = (physical_accel_t *) malloc(sizeof(physical_accel_t));
            hpthread_cand_t *cand_temp = (hpthread_cand_t *) malloc(sizeof(hpthread_cand_t));
            accel_temp->accel_id = device_id++;
            cand_temp->accel_id = accel_temp->accel_id ;
            bitset_reset_all(accel_temp->valid_contexts);
            for (int i = 0; i < MAX_CONTEXTS; i++) {
                accel_temp->th[i] = NULL;
                accel_temp->context_start_cycles[i] = 0;
                accel_temp->context_active_cycles[i] = 0;
                accel_temp->context_util[i] = 0.0;
            }
            strcpy(accel_temp->devname, entry->d_name);
            accel_temp->init_done = false;
            accel_temp->effective_util = 0.0;
            // Print out debug message
            HIGH_DEBUG(printf("[VAM] Discovered device %d: %s\n", device_id, accel_temp->devname);)

            if (fnmatch("gemm*", entry->d_name, FNM_NOESCAPE) == 0) {
                gemm_probe(accel_temp);
                accel_temp->cpu_invoke = true;
                cand_temp->prim = accel_temp->prim;
                cand_temp->cpu_invoke = true;
            } else {
                printf("[ERROR] Device does not match any supported accelerators.\n");
            }

            char full_path[384];
            snprintf(full_path, 384, "/dev/%s", entry->d_name);
            accel_temp->fd = open(full_path, O_RDWR, 0);
            if (accel_temp->fd < 0) {
                fprintf(stderr, "Error: cannot open %s", full_path);
                exit(EXIT_FAILURE);
            }
            // Reset the accelerator to be sure
            if (!accel_temp->cpu_invoke) {
                struct esp_access *esp_access_desc = (struct esp_access *) accel_temp->esp_access_desc;
                if (ioctl(accel_temp->fd, accel_temp->reset_ioctl, esp_access_desc)) {
                    perror("ioctl");
                    exit(EXIT_FAILURE);
                }
            }
            insert_physical_accel(accel_temp);
            insert_hpthread_cand(cand_temp);
        }
    }

    closedir(dir);
}

void *vam_run_backend(void *arg) {
	HIGH_DEBUG(printf("[VAM] Hello from VAM BACKEND!\n");)
    // populate the list of physical accelerators in the system
    vam_probe_accel();

    uint64_t start_time = get_counter();
    uint64_t vam_sleep = VAM_SLEEP_MIN;

    // const float LB_RETRY_HIGH = 0.5;
    // const float LB_RETRY_LOW = 0.25;
    // const float LB_RETRY_DIFF = 0.25;
    // const float LB_RETRY_RESET = 0.1;
    // const unsigned MAX_LB_RETRY = 3;
    // unsigned NUM_LB_RETRY = MAX_LB_RETRY;

    // Run loop will run forever
    while (1) {
        // Test the interface state
        uint8_t state = hpthread_intf_test();

        switch(state) {
            case VAM_IDLE: {
                // // Examine the util across all accelerators in the system
                // if (get_counter() - start_time > vam_sleep) {
                //     float load_imbalance = vam_check_load_balance();
                    
                //     // Allow load balances if load is sufficiently low.
                //     if ((load_imbalance <= LB_RETRY_RESET)) {
                //         NUM_LB_RETRY = MAX_LB_RETRY;
                //         load_imbalance_reg = load_imbalance; // reset baseline
                //     }

                //     // If no retries allowed, but diff is large, allow retries
                //     if ((NUM_LB_RETRY == 0) && fabsf(load_imbalance - load_imbalance_reg) > LB_RETRY_DIFF) NUM_LB_RETRY = MAX_LB_RETRY;

                //     // Can we try a load balance?
                //     if (NUM_LB_RETRY != 0 && load_imbalance > LB_RETRY_LOW) {
                //         // Start the load balancing algorithm
                //         LOW_DEBUG(printf("[VAM] Trigerring load balancer, imbalance=%0.2f\n", load_imbalance);)
                //         if (!vam_load_balance()) {
                //             // If the load balancing was not successful, reduce the retry count
                //             NUM_LB_RETRY--;
                //         }
                //     }

                //     if (load_imbalance > LB_RETRY_HIGH) {
                //         // If load is very imbalanced, sleep less
                //         vam_sleep = VAM_SLEEP_MIN;
                //     } else {
                //         if (vam_sleep < VAM_SLEEP_MAX) vam_sleep += VAM_SLEEP_MIN;
                //     }

                //     // As a fallback, reset the retry every ~5 seconds of sleep
                //     if (vam_sleep >= VAM_SLEEP_MID) NUM_LB_RETRY = MAX_LB_RETRY;

                //     start_time = get_counter();
                // }
                sched_yield();
                break;
            }
            case VAM_CREATE: {
                HIGH_DEBUG(printf("[VAM] Received a request for creating hpthread %s\n", hpthread_get_name(intf.th));)
                vam_search_accel(intf.th);
                break;
            }
            case VAM_JOIN: {
                HIGH_DEBUG(printf("[VAM] Received a request for joining hpthread %s\n", hpthread_get_name(intf.th));)
                vam_release_accel(intf.th);
                break;
            }
            case VAM_SETPRIO: {
                HIGH_DEBUG(printf("[VAM] Received a request for changing priority hpthread %s to %d\n", hpthread_get_name(intf.th), intf.th->nprio);)
                vam_setprio_accel(intf.th);
                break;
            }
            case VAM_REPORT: {
                HIGH_DEBUG(printf("[VAM] Received a report request\n");)
                // print_report();
                break;
            }
            case VAM_QUERY: {
                HIGH_DEBUG(printf("[VAM] Received a query request\n");)
                intf.list = hpthread_cand_list;
                break;
            }
            default:
                break;
        }
        // If there was a request, set the state to done.
        if (state > VAM_DONE) {
            // Set the interface state to DONE
            hpthread_intf_set(VAM_DONE);
            HIGH_DEBUG(printf("[VAM] Completed the processing of request.\n");)
            // Reset trigger delay after servicing a request
            start_time = get_counter();
            vam_sleep = VAM_SLEEP_MIN;
        }
    }
    return NULL;
}

void vam_search_accel(hpthread_t *th) {
    HIGH_DEBUG(printf("[VAM] Searching accelerator for hpthread %s\n", hpthread_get_name(th));)
    // // First, update the active utilization of each accelerator
    // vam_check_utilization();

    // We will find a candidate accelerator that has the lowest utiilization.
    // If no accelerator candidates are found, we will consider the CPU as the only candidate.
    physical_accel_t *candidate_accel = NULL;
    float candidate_util = 2.0;
    bitset_t candidate_contexts = 255;
    bool accel_allocated = false;
    physical_accel_t *cur_accel = accel_list;

    while(cur_accel != NULL) {
        HIGH_DEBUG(
            printf("\n[VAM] Checking device %s.\n", physical_accel_get_name(cur_accel));
            physical_accel_dump(cur_accel);
        )
        // If the thread or accel requires CPU invocation, the other must too
        if (th->cpu_invoke ^ cur_accel->cpu_invoke) continue;
        // Is the accelerator suitable and not fully utilized for this primitive?
        if (cur_accel->prim == th->prim && !bitset_all(cur_accel->valid_contexts)) {
            // Check if this accelerator's total load is less than the previous min or fewer contexts (with similar util)
            if ((cur_accel->effective_util < candidate_util - 0.1) ||
                (fabsf(cur_accel->effective_util - candidate_util) <= 0.1 && (cur_accel->valid_contexts < candidate_contexts))) {
                candidate_accel = cur_accel;
                candidate_util = cur_accel->effective_util;
                candidate_contexts = cur_accel->valid_contexts;
                HIGH_DEBUG(printf("[VAM] Device %s is a candidate!\n", physical_accel_get_name(cur_accel));)
            }
            // Found one accelerator!
            accel_allocated = true;
        }
		cur_accel = cur_accel->next;
    }
    HIGH_DEBUG(printf("[VAM] Candidate for hpthread %s = %s!\n", hpthread_get_name(th), physical_accel_get_name(candidate_accel));)
    // Identify the valid context to allocate
    unsigned cur_context = 0;
    if (accel_allocated) {
        for (unsigned i = 0; i < MAX_CONTEXTS; i++) {
            if (!bitset_test(candidate_accel->valid_contexts, i)) {
                cur_context = i;
                break;
            }
        }
    }
    // If no candidate accelerator was found, we will create a new CPU thread for this node.
    if (accel_allocated == false) {
        // Create a new physical accelerator for this CPU thread
        physical_accel_t *cpu_thread = (physical_accel_t *) malloc(sizeof(physical_accel_t));
        cpu_thread->prim = PRIM_NONE;
        LOW_DEBUG(strcpy(cpu_thread->devname, "CPU");)
        candidate_accel = cpu_thread;
        insert_cpu_thread(cpu_thread);
        candidate_util = 0.0;
    }
    // Update the phy<->virt mapping for the chosen context with the hpthread
    candidate_accel->th[cur_context] = th;
    th->accel = candidate_accel;
    th->accel_context = cur_context;
    // Mark the context as allocated.
    bitset_set(candidate_accel->valid_contexts, cur_context);
    // Configure the device allocated
    if (accel_allocated) {
        if (th->cpu_invoke) {
            vam_configure_cpu_invoke(th, candidate_accel);
        } else {
            vam_configure_accel(th, candidate_accel, cur_context);
        }
    } else {
        vam_configure_cpu(th, candidate_accel);
    }
}

void vam_configure_accel(hpthread_t *th, physical_accel_t *accel, unsigned context) {
    HIGH_DEBUG(printf("[VAM] Configuring accel...\n");)
    // Get the mem handle for the hpthread
    void *mem = th->args->mem;
    // ESP defined data type for the pointer to the memory pool for accelerators.
    enum contig_alloc_policy policy;
    contig_handle_t *handle = lookup_handle(mem, &policy);

    // Configring all device-independent fields in the esp desc
    struct esp_access *esp_access_desc = (struct esp_access *) accel->esp_access_desc;
    {
        esp_access_desc->contig = contig_to_khandle(*handle);
        esp_access_desc->ddr_node = contig_to_most_allocated(*handle);
        esp_access_desc->alloc_policy = policy;
        esp_access_desc->run = true;
        esp_access_desc->coherence = ACC_COH_RECALL;
        esp_access_desc->spandex_conf = 0;
        esp_access_desc->start_stop = 1;
        esp_access_desc->p2p_store = 0;
        esp_access_desc->p2p_nsrcs = 0;
        esp_access_desc->src_offset = 0;
        esp_access_desc->dst_offset = 0;
        esp_access_desc->context_id = context;
        // esp_access_desc->context_base_ptr = th->args->base_ptr;
        esp_access_desc->context_nprio = th->nprio;
        esp_access_desc->valid_contexts = accel->valid_contexts;
        esp_access_desc->sched_period = AVU_SCHED_PERIOD;
    }

    if (accel->init_done) {
        LOW_DEBUG(printf("[VAM] Adding to accel %s:%d for hpthread %s\n", physical_accel_get_name(accel), context, hpthread_get_name(th));)
        if (ioctl(accel->fd, accel->add_ctxt_ioctl, esp_access_desc)) {
            perror("ioctl");
            exit(EXIT_FAILURE);
        }
    } else {
        LOW_DEBUG(printf("[VAM] Initializing accel %s:0 for hpthread %s\n", physical_accel_get_name(accel), hpthread_get_name(th));)
        if (ioctl(accel->fd, accel->init_ioctl, esp_access_desc)) {
            perror("ioctl");
            exit(EXIT_FAILURE);
        }
        accel->init_done = true;
    }
    // Read the current time for when the accelerator is started.
    accel->context_start_cycles[context] = get_counter();
    accel->context_active_cycles[context] = 0;
}

void vam_configure_cpu_invoke(hpthread_t *th, physical_accel_t *accel) {
    LOW_DEBUG(printf("[VAM] Launch CPU invoke thread for hpthread %s\n", hpthread_get_name(th));)
    // Find SW kernel for this thread
    void *(*sw_kernel)(void *);
    switch(th->prim) {
        case PRIM_GEMM: sw_kernel = gemm_invoke; break;
        default: break;
    }    
    // Create a new CPU thread for the SW implementation of this node.
    pthread_t cpu_thread;
    th->args->kill_pthread = (bool *) malloc (sizeof(bool)); *(th->args->kill_pthread) = false;
    cpu_invoke_args_t *args = (cpu_invoke_args_t *) malloc (sizeof(cpu_invoke_args_t)); 
    args->args = th->args;
    args->accel = accel;
    // Create pthread attributes
    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0) {
        perror("attr_init");
    }
    // Set CPU affinity
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(core_affinity_ctr % cpu_online, &set);
    if (pthread_attr_setaffinity_np(&attr, sizeof(set), &set) != 0) {
        perror("pthread_attr_setaffinity_np");
    }
    // Set SCHED_RR scheduling policy with priority 1
    if (pthread_attr_setschedpolicy(&attr, SCHED_RR) != 0) {
        perror("pthread_attr_setschedpolicy");
    }
    struct sched_param sp = { .sched_priority = 1 };
    if (pthread_attr_setschedparam(&attr, &sp) != 0) {
        perror("pthread_attr_setschedparam");
    }
    if (pthread_create(&cpu_thread, &attr, sw_kernel, (void *) args) != 0) {
        perror("Failed to create CPU thread\n");
    }

    // Add this thread to the physical_accel struct
    accel->cpu_thread = cpu_thread;
}

void vam_configure_cpu(hpthread_t *th, physical_accel_t *accel) {
    LOW_DEBUG(printf("[VAM] Configuring CPU for hpthread %s\n", hpthread_get_name(th));)
    // Find SW kernel for this thread
    void *(*sw_kernel)(void *);
    switch(th->prim) {
        case PRIM_GEMM: sw_kernel = sw_gemm; break;
        default: break;
    }    
    // Create a new CPU thread for the SW implementation of this node.
    pthread_t cpu_thread;
    th->args->kill_pthread = (bool *) malloc (sizeof(bool)); *(th->args->kill_pthread) = false;
    if (pthread_create(&cpu_thread, NULL, sw_kernel, (void *) th->args) != 0) {
        perror("Failed to create CPU thread\n");
    }

    // Add this thread to the physical_accel struct
    accel->cpu_thread = cpu_thread;
}

void vam_release_accel(hpthread_t *th) {
    physical_accel_t *accel = th->accel;
    unsigned context = th->accel_context;
    LOW_DEBUG(printf("[VAM] Releasing accel %s:%d for hpthread %s\n", physical_accel_get_name(accel), context, hpthread_get_name(th));)
    // Free the allocated context.
    bitset_reset(accel->valid_contexts, context);

    if (accel->prim == PRIM_NONE || accel->cpu_invoke) {
        *(th->args->kill_pthread) = true;
        pthread_join(accel->cpu_thread, NULL);
    } else {
        struct esp_access *esp_access_desc = accel->esp_access_desc;
        {
            esp_access_desc->context_id = context;
            esp_access_desc->valid_contexts = accel->valid_contexts;
        }
        if (ioctl(accel->fd, accel->del_ctxt_ioctl, esp_access_desc)) {
            perror("ioctl");
            exit(EXIT_FAILURE);
        }
        // If there is no context active, we should re-init the accelerator the next time
        if (bitset_none(accel->valid_contexts)) {
            accel->init_done = false;
        }
    }

    // Delete the entry for this context in the phy<->virt mapping
    accel->th[context] = NULL;
    th->accel = NULL;
}

void vam_setprio_accel(hpthread_t *th) {
    physical_accel_t *accel = th->accel;
    unsigned context = th->accel_context;
    LOW_DEBUG(printf("[VAM] Setting priority of accel %s:%d to %d for hpthread %s\n", physical_accel_get_name(accel), context, th->nprio, hpthread_get_name(th));)

    struct esp_access *esp_access_desc = accel->esp_access_desc;
    {
        esp_access_desc->context_id = context;
        esp_access_desc->context_nprio = th->nprio;
    }
    if (ioctl(accel->fd, accel->setprio_ioctl, esp_access_desc)) {
        perror("ioctl");
        exit(EXIT_FAILURE);
    }
}

void insert_physical_accel(physical_accel_t *accel) {
	accel->next = accel_list;
	accel_list = accel;
}

void insert_hpthread_cand(hpthread_cand_t *cand) {
    cand->next = hpthread_cand_list;
    hpthread_cand_list = cand;
}

void insert_cpu_thread(physical_accel_t *accel) {
	accel->next = cpu_thread_list;
	cpu_thread_list = accel;
}

void vam_check_utilization() {
    physical_accel_t *cur_accel = accel_list;
    while(cur_accel != NULL) {
        struct avu_mon_desc mon;
        HIGH_DEBUG(
            printf("[VAM] Util of %s: ", physical_accel_get_name(cur_accel));
        )
        if (ioctl(cur_accel->fd, ESP_IOC_MON, &mon)) {
            perror("ioctl");
            exit(EXIT_FAILURE);
        }
        uint64_t *mon_extended = (uint64_t *) mon.util;
        cur_accel->effective_util = 0;

        for (int i = 0; i < MAX_CONTEXTS; i++) {
            if (bitset_test(cur_accel->valid_contexts, i)) {
                // Get the utilization in the previous monitor period
                uint64_t elapsed_cycles = get_counter() - cur_accel->context_start_cycles[i];
                uint64_t util_cycles = mon_extended[i] - cur_accel->context_active_cycles[i];
                float util = (float) util_cycles/elapsed_cycles;
                if (util_cycles > elapsed_cycles) util = 0.01;
                // Set the cycles for the next period
                cur_accel->context_start_cycles[i] = get_counter();
                cur_accel->context_active_cycles[i] = mon_extended[i];
                cur_accel->context_util[i] = util;
                hpthread_t *th = cur_accel->th[i];
                th->th_util = util;
                cur_accel->effective_util += util / th->nprio;

                HIGH_DEBUG(
                    printf("C%d(%d)=%05.2f%%, ", i, th->nprio, util * 100);
                )
            }
        }
        HIGH_DEBUG(printf("e.util=%05.2f%%\n", cur_accel->effective_util * 100);)
		cur_accel = cur_accel->next;
    }
}

float vam_check_load_balance() {
    // First, update the active utilization of each accelerator
    vam_check_utilization();
    // Print out the total utilization
    LOW_DEBUG({
        physical_accel_t *cur_accel = accel_list;
        while(cur_accel != NULL) {
            printf("[VAM] Util of %s: ", physical_accel_get_name(cur_accel));
            float total_util = 0.0;
            for (int i = 0; i < MAX_CONTEXTS; i++) {
                if (bitset_test(cur_accel->valid_contexts, i)) {
                    hpthread_t *th = cur_accel->th[i];
                    printf("C%d(%d)=%05.2f%%, ", i, th->nprio, cur_accel->context_util[i] * 100);
                    total_util += cur_accel->context_util[i];
                } else {
                    printf("C%d(-)=--.--%%, ", i);
                }
            }
            LOW_DEBUG(printf("total=%05.2f%%, e.util=%05.2f%%\n", total_util * 100, cur_accel->effective_util * 100);)
		    cur_accel = cur_accel->next;
        }
    })
    // Check whether there is load imbalance across accelerators
    // - calculate average and max/min util across all accelerators
    float local_avg_util = 0.001; float local_max_util = 0.0; float local_min_util = 10.0;
    unsigned count = 0;
    bool skip_load_balance = true;

    physical_accel_t *cur_accel = accel_list;
    physical_accel_t *tmp_max, *tmp_min;
    while(cur_accel != NULL) {
        float cur_util = cur_accel->effective_util;
        if (cur_util < local_min_util) { local_min_util = cur_util; tmp_min = cur_accel; }
        if (cur_util > local_max_util) { local_max_util = cur_util; tmp_max = cur_accel; }
        local_avg_util += cur_util;
        count++;
        if (bitset_count(cur_accel->valid_contexts) > 1) skip_load_balance = false;
        HIGH_DEBUG(printf("[VAM] Current load of %s = %0.2f\n", physical_accel_get_name(cur_accel), cur_util);)
        cur_accel = cur_accel->next;
    }
    // If none of the accelerators have more than one valid context, there's no need for load balancing
    if (skip_load_balance) return false;

    local_avg_util = local_avg_util/count;
    HIGH_DEBUG(printf("[VAM] Max util = %0.2f, min util = %0.2f, avg util = %0.2f\n", local_max_util, local_min_util, local_avg_util);)
    max_util_accel = tmp_max; min_util_accel = tmp_min;
    avg_util = local_avg_util; max_util = local_max_util; min_util = local_min_util; 
    return (local_max_util - local_min_util) / (local_avg_util);
}

bool vam_load_balance() {
    // Contexts we are migrating (swap only if both accel are full)
    unsigned best_context_min, best_context_max; 
    hpthread_t *best_th_max = NULL; hpthread_t *any_th_max = NULL;
    hpthread_t *best_th_min = NULL; hpthread_t *any_th_min = NULL;
    hpthread_t *move_th_max, *move_th_min;
    float best_util_max = 0.0; float any_util_max = 0.0;
    float best_util_min = 10.0; float any_util_min = 10.0;
    bool no_min_contexts = false;
    // Find the most loaded thread on most loaded accel that was not recently moved
    for (int i = 0; i < MAX_CONTEXTS; i++) {
        if (bitset_test(max_util_accel->valid_contexts, i)) {
            hpthread_t *th = max_util_accel->th[i];
            uint64_t t_last_move = get_counter() - th->th_last_move;
            float th_util = th->th_util / th->nprio;
            if (th_util > any_util_max) {
                any_th_max = th;
                any_util_max = th_util;
            }
            if (th_util > best_util_max && t_last_move > TH_MOVE_COOLDOWN) {
                best_th_max = th;
                best_util_max = th_util;
            }
        }
    }
    move_th_max = best_th_max ? best_th_max : any_th_max;
    best_context_max = move_th_max->accel_context;
    move_th_max->th_last_move = get_counter();

    // Check if there exist any valid contexts on least loaded accel
    if (bitset_all(min_util_accel->valid_contexts)) {
        // if not, we need to release the least loaded thread on it.
        for (int i = 0; i < MAX_CONTEXTS; i++) {
            if (bitset_test(min_util_accel->valid_contexts, i)) {
                hpthread_t *th = min_util_accel->th[i];
                uint64_t t_last_move = get_counter() - th->th_last_move;
                float th_util = th->th_util / th->nprio;
                if (th_util < any_util_min) {
                    any_th_min = th;
                    any_util_min = th_util;
                }
                if (th_util < best_util_min && t_last_move > TH_MOVE_COOLDOWN) {
                    best_th_min = th;
                    best_util_min = th_util;
                }
            }
        }
        move_th_min = best_th_min ? best_th_min : any_th_min;
        best_context_min = move_th_min->accel_context;
        move_th_min->th_last_move = get_counter();
        no_min_contexts = true;
    } else {
        // if yes, identify one context to allocate
        for (unsigned i = 0; i < MAX_CONTEXTS; i++) {
            if (!bitset_test(min_util_accel->valid_contexts, i)) {
                best_context_min = i;
                break;
            }
        }
    }

    // Roughly check if the util improvement is worth the migration
    float local_max_util = max_util;
    float local_min_util = min_util;
    local_max_util -= move_th_max->th_util / move_th_max->nprio; local_min_util += move_th_max->th_util / move_th_max->nprio;
    LOW_DEBUG(printf("[VAM] Map %s from %s:%d to %s:%d\n", hpthread_get_name(move_th_max),
                        physical_accel_get_name(max_util_accel), best_context_max, physical_accel_get_name(min_util_accel), best_context_min);)
    if (no_min_contexts) {
        local_max_util += move_th_min->th_util / move_th_min->nprio; local_min_util -= move_th_min->th_util / move_th_min->nprio;
        LOW_DEBUG(printf("[VAM] Map %s from %s:%d to %s:%d\n", hpthread_get_name(move_th_min),
                            physical_accel_get_name(min_util_accel), best_context_min, physical_accel_get_name(max_util_accel), best_context_max);)
    }
    float old_load_imbalance = (max_util - min_util) / (avg_util);
    float new_load_imbalance = (fabsf(local_max_util - local_min_util)) / (avg_util);
    const float LB_RETRY_DIFF = 0.25;
    if (old_load_imbalance - new_load_imbalance < LB_RETRY_DIFF) {
        load_imbalance_reg = old_load_imbalance;
        LOW_DEBUG(printf("[VAM] Skipping load balance, new imbalance = %f\n", new_load_imbalance);)
        return false;
    }

    // Release the context first
    vam_release_accel(move_th_max);
    if (no_min_contexts) {
        // Release the least loaded context, if needed
        vam_release_accel(move_th_min);
    }

    // Update the phy<->virt mapping for the chosen context with the hpthread
    min_util_accel->th[best_context_min] = move_th_max;
    move_th_max->accel = min_util_accel;
    move_th_max->accel_context = best_context_min;
    // Mark the context as allocated.
    bitset_set(min_util_accel->valid_contexts, best_context_min);
    // Configure the device allocated
    vam_configure_accel(move_th_max, min_util_accel, best_context_min);

    if (no_min_contexts) {
        // Update the phy<->virt mapping for the chosen context with the hpthread
        max_util_accel->th[best_context_max] = move_th_min;
        move_th_min->accel = max_util_accel;
        move_th_min->accel_context = best_context_max;
        // Mark the context as allocated.
        bitset_set(max_util_accel->valid_contexts, best_context_max);
        // Configure the device allocated
        vam_configure_accel(move_th_min, max_util_accel, best_context_max);
    }
    return true;
}
