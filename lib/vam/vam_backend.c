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

// ESP API for getting contig_alloc handle
extern contig_handle_t *lookup_handle(void *buf, enum contig_alloc_policy *policy);
// External instance of hpthread interface
extern hpthread_intf_t intf;
// Used in wakeup_vam()
pthread_t vam_th;
// Physical accelerator list
physical_accel_t *accel_list = NULL;
physical_accel_t *cpu_thread_list = NULL;

// Function to wake up VAM for the first time
void wakeup_vam() {
	HIGH_DEBUG(printf("[VAM BE] Launching a new thread for VAM BACKEND!\n");)
    // Create an object of VAM and launch a std::thread for VAM
    pthread_attr_t attr;
    // Create pthread attributes
    if (pthread_attr_init(&attr) != 0) {
        perror("attr_init");
        exit(1);
    }
    // Set pthread attributes to be detached; no join required
    if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0) {
        perror("attr_setdetachstate");
        exit(1);
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
    HIGH_DEBUG(printf("[VAM BE] Performing device probe.\n");)
    struct dirent *entry;
    unsigned device_id = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (fnmatch("*_stratus.*", entry->d_name, FNM_NOESCAPE) == 0) {
            physical_accel_t *accel_temp = (physical_accel_t *) malloc(sizeof(physical_accel_t));
            accel_temp->accel_id = device_id++;
            bitset_reset_all(accel_temp->valid_contexts);
            for (int i = 0; i < MAX_CONTEXTS; i++) {
                accel_temp->th[i] = NULL;
                accel_temp->context_load[i] = 0;
                accel_temp->effective_load[i] = 0;
                accel_temp->context_start_cycles[i] = 0;
                accel_temp->context_active_cycles[i] = 0;
                accel_temp->context_util[i] = 0.0;
            }
            LOW_DEBUG(strcpy(accel_temp->devname, entry->d_name);)
            accel_temp->init_done = false;

            // Print out debug message
            HIGH_DEBUG(printf("[VAM BE] Discovered device %d: %s\n", device_id, accel_temp->devname);)

            if (fnmatch("gemm*", entry->d_name, FNM_NOESCAPE) == 0) {
                gemm_probe(accel_temp);
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
            struct esp_access *esp_access_desc = (struct esp_access *) accel_temp->esp_access_desc;
            if (ioctl(accel_temp->fd, accel_temp->reset_ioctl, esp_access_desc)) {
                perror("ioctl");
                exit(EXIT_FAILURE);
            }

            insert_physical_accel(accel_temp);
        }
    }

    closedir(dir);
}

void *vam_run_backend(void *arg) {
	HIGH_DEBUG(printf("[VAM BE] Hello from VAM BACKEND!\n");)
    // populate the list of physical accelerators in the system
    vam_probe_accel();

    uint64_t start_time = get_counter();
    uint64_t vam_sleep = VAM_SLEEP_MIN;

    // Run loop will run forever
    while (1) {
        // Test the interface state
        uint8_t state = hpthread_intf_test();

        switch(state) {
            case VAM_IDLE: {
                // Examine the load across all accelerators in the system
                if (get_counter() - start_time > vam_sleep) {
                    // if(check_load_balance()) {
                    //     LOW_DEBUG(printf("[VAM BE] Trigerring load balancer...\n");)

                    //     // Start the load balancing algorithm
                    //     load_balance();
                    //     start_time = get_counter();
                    // }
                    start_time = get_counter();
                    if (vam_sleep < VAM_SLEEP_MAX) vam_sleep += VAM_SLEEP_MIN;
                }
                sched_yield();
                break;
            }
            case VAM_CREATE: {
                HIGH_DEBUG(printf("[VAM BE] Received a request for creating hpthread %s\n", hpthread_get_name(intf.th));)
                vam_search_accel(intf.th);
                break;
            }
            case VAM_JOIN: {
                HIGH_DEBUG(printf("[VAM BE] Received a request for joining hpthread %s\n", hpthread_get_name(intf.th));)
                vam_release_accel(intf.th);
                break;
            }
            case VAM_SETPRIO: {
                HIGH_DEBUG(printf("[VAM BE] Received a request for changing priority hpthread %s to %d\n", hpthread_get_name(intf.th), intf.th->nprio);)
                vam_setprio_accel(intf.th);
                break;
            }
            case VAM_REPORT: {
                HIGH_DEBUG(printf("[VAM BE] Received a report request\n");)
                // print_report();
                break;
            }
            default:
                break;
        }
        // If there was a request, set the state to done.
        if (state > VAM_DONE) {
            // Set the interface state to DONE
            hpthread_intf_set(VAM_DONE);
            HIGH_DEBUG(printf("[VAM BE] Completed the processing of request.\n");)
            // Reset trigger delay after servicing a request
            start_time = get_counter();
            vam_sleep = VAM_SLEEP_MIN;
        }
    }
    return NULL;
}

void vam_search_accel(hpthread_t *th) {
    HIGH_DEBUG(printf("[VAM BE] Searching accelerator for hpthread %s\n", hpthread_get_name(th));)
    // // First, update the active utilization of each accelerator
    // check_utilization();

    // We will find a candidate accelerator that has the lowest assigned load.
    // If no accelerator candidates are found, we will consider the CPU as the only candidate.
    physical_accel_t *candidate_accel = NULL;
    // unsigned candidate_load = INT_MAX;
    bool accel_allocated = false;
    physical_accel_t *cur_accel = accel_list;

    while(cur_accel != NULL) {
        HIGH_DEBUG(
            printf("\n[VAM BE] Checking device %s.\n", physical_accel_get_name(cur_accel));
            physical_accel_dump(cur_accel);
        )
        // Is the accelerator suitable and not fully utilized for this primitive?
        if (cur_accel->prim == th->prim && !bitset_all(cur_accel->valid_contexts)) {
            // // Check if this accelerator's total load is less than the previous min
            // if (accel.get_effective_load() < candidate_accel.second) {
                candidate_accel = cur_accel;
                // candidate_load = accel.get_effective_load()};
                HIGH_DEBUG(printf("[VAM BE] Device %s is a candidate!\n", physical_accel_get_name(cur_accel));)
            // }
            // Found one accelerator!
            accel_allocated = true;
        }
		cur_accel = cur_accel->next;
    }
    // If no candidate accelerator was found, we will create a new CPU thread for this node.
    if (accel_allocated == false) {
        // Create a new physical accelerator for this CPU thread
        physical_accel_t *cpu_thread = (physical_accel_t *) malloc(sizeof(physical_accel_t));
        cpu_thread->prim = PRIM_NONE;
        LOW_DEBUG(strcpy(cpu_thread->devname, "CPU");)
        candidate_accel = cpu_thread;
        insert_cpu_thread(cpu_thread);
        // candidate_load = 0;
    }
    HIGH_DEBUG(printf("[VAM BE] Candidate for hpthread %s = %s!\n", hpthread_get_name(th), physical_accel_get_name(candidate_accel));)
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
    // Update the phy<->virt mapping for the chosen context with the hpthread
    candidate_accel->th[cur_context] = th;
    th->accel = candidate_accel;
    th->accel_context = cur_context;
    // Mark the context as allocated.
    bitset_set(candidate_accel->valid_contexts, cur_context);
    // Configure the device allocated
    if (accel_allocated) {
        vam_configure_accel(th, candidate_accel, cur_context);
    } else {
        vam_configure_cpu(th, candidate_accel);
    }
}

void vam_configure_accel(hpthread_t *th, physical_accel_t *accel, unsigned context) {
    HIGH_DEBUG(printf("[VAM BE] Configuring accel...\n");)
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
        esp_access_desc->context_base_ptr = th->args->base_ptr;
        esp_access_desc->context_nprio = th->nprio;
        esp_access_desc->valid_contexts = accel->valid_contexts;
        esp_access_desc->sched_period = AVU_SCHED_PERIOD;
    }
    // // Retrieve load assigned from device-dependent cfg function (scaled by priority)
    // accel->context_load[context] = th->assigned_load / th->attr->nprio;
    // th->active_load = accel->context_load[context];

    if (accel->init_done) {
        LOW_DEBUG(printf("[VAM BE] Adding to accel %s:%d for hpthread %s\n", physical_accel_get_name(accel), context, hpthread_get_name(th));)
        if (ioctl(accel->fd, accel->add_ctxt_ioctl, esp_access_desc)) {
            perror("ioctl");
            exit(EXIT_FAILURE);
        }
    } else {
        LOW_DEBUG(printf("[VAM BE] Initializing accel %s:0 for hpthread %s\n", physical_accel_get_name(accel), hpthread_get_name(th));)
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

void vam_configure_cpu(hpthread_t *th, physical_accel_t *accel) {
    LOW_DEBUG(printf("[VAM BE] Configuring CPU for hpthread %s\n", hpthread_get_name(th));)
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
    LOW_DEBUG(printf("[VAM BE] Releasing accel %s:%d for hpthread %s\n", physical_accel_get_name(accel), context, hpthread_get_name(th));)
    // Free the allocated context.
    bitset_reset(accel->valid_contexts, context);

    if (accel->prim == PRIM_NONE) {
        *(th->args->kill_pthread) = false;
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
    LOW_DEBUG(printf("[VAM BE] Setting priority of accel %s:%d to %d for hpthread %s\n", physical_accel_get_name(accel), context, th->nprio, hpthread_get_name(th));)
    // Free the allocated context.
    bitset_reset(accel->valid_contexts, context);

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

void insert_cpu_thread(physical_accel_t *accel) {
	accel->next = cpu_thread_list;
	cpu_thread_list = accel;
}
