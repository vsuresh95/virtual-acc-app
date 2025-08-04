#include <vam_backend.h>

// External instance of hpthread interface
extern hpthread_intf_t intf;

// Used in wakeup_vam().
std::thread vam_th;

// Function to wake up VAM for the first time
void wakeup_vam() {
	HIGH_DEBUG(printf("[VAM BE] Launching a new thread for VAM BACKEND!\n");)

    // Create an object of VAM and launch a std::thread for VAM
    vam_backend *be = new vam_backend;
    vam_th = std::thread(&vam_backend::run_backend, be);
    vam_th.detach();
}

void vam_backend::run_backend() {
	HIGH_DEBUG(printf("[VAM BE] Hello from VAM BACKEND!\n");)

    // populate the list of physical accelerators in the system
    probe_accel();

    uint64_t start_time = get_counter();
    uint64_t vam_sleep = VAM_SLEEP_MIN;

    // Start a utilization monitor thread
    std::thread util_mon = std::thread(&vam_backend::run_util_mon, this);
    util_mon.detach();

    // Run loop will run forever
    while (1) {
        // Test the interface state
        vam_state_t state = intf.test();

        switch(state) {
            case vam_state_t::IDLE: {
                // Examine the load across all accelerators in the system
                if (get_counter() - start_time > vam_sleep) {
                    if(check_load_balance()) {
                        LOW_DEBUG(printf("[VAM BE] Trigerring load balancer...\n");)

                        // Start the load balancing algorithm
                        load_balance();
                        start_time = get_counter();
                    }
                    start_time = get_counter();
                    if (vam_sleep < VAM_SLEEP_MAX) vam_sleep += VAM_SLEEP_MIN;
                }
                sched_yield();
                break;
            }
            case vam_state_t::CREATE: {
                HIGH_DEBUG(printf("\n[VAM BE] Received a request for creating hpthread %s\n", intf.th->get_name());)
                search_accel(intf.th);
                break;
            }
            case vam_state_t::JOIN: {
                HIGH_DEBUG(printf("\n[VAM BE] Received a request for joining hpthread %s\n", intf.th->get_name());)
                if (!release_accel(intf.th)) {
                    perror("hpthread unmapped earlier!");
                    exit(1);
                }
                break;
            }
            case vam_state_t::SETPRIO: {
                HIGH_DEBUG(printf("\n[VAM BE] Received a request for changing priority hpthread %s to %d\n", intf.th->get_name(), intf.th->attr->nprio);)
                if (!setprio_accel(intf.th)) {
                    perror("hpthread unmapped earlier!");
                    exit(1);
                }
                break;
            }
            case vam_state_t::REPORT: {
                HIGH_DEBUG(printf("\n[VAM BE] Received a report request\n");)
                print_report();
                break;
            }
            default:
                break;
        }

        // If there was a request, set the state to done.
        if (state > vam_state_t::DONE) {
            // Set the interface state to DONE
            intf.set(vam_state_t::DONE);
            HIGH_DEBUG(printf("[VAM BE] Completed the processing of request.\n");)
            // Reset trigger delay after servicing a request
            start_time = get_counter();
            vam_sleep = VAM_SLEEP_MIN;
        }
    }
}

void vam_backend::probe_accel() {
    // Open the devices directory to search for accels
    DIR *dir = opendir("/dev/");
    if (!dir) {
        perror("Failed to open directory");
        exit(1);
    }

    // Search for all stratus accelerators and fill into accel_list
    struct dirent *entry;

    HIGH_DEBUG(printf("[VAM BE] Performing device probe.\n");)

    unsigned device_id = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (fnmatch("*_stratus.*", entry->d_name, FNM_NOESCAPE) == 0) {
            physical_accel_t accel_temp;
            accel_temp.accel_id = device_id++;
            accel_temp.valid_contexts.reset();
            for (int i = 0; i < MAX_CONTEXTS; i++) {
                accel_temp.thread_id[i] = -1;
                accel_temp.context_load[i] = 0;
                accel_temp.effective_load[i] = 0;
                accel_temp.context_start_cycles[i] = 0;
                accel_temp.context_active_cycles[i] = 0;
                accel_temp.context_util[i] = 0.0;
            }
            std::strcpy(accel_temp.devname, entry->d_name);
            accel_temp.init_done = false;

            // Print out debug message
            HIGH_DEBUG(printf("[VAM BE] Discovered device %d: %s\n", device_id, accel_temp.devname);)

            if (fnmatch("audio_fft*", entry->d_name, FNM_NOESCAPE) == 0) {
                audio_fft_probe(&accel_temp);
            } else if (fnmatch("audio_fir*", entry->d_name, FNM_NOESCAPE) == 0) {
                audio_fir_probe(&accel_temp);
            } else if (fnmatch("audio_ffi*", entry->d_name, FNM_NOESCAPE) == 0) {
            } else if (fnmatch("gemm*", entry->d_name, FNM_NOESCAPE) == 0) {
                gemm_probe(&accel_temp);
            } else {
                printf("[ERROR] Device does not match any supported accelerators.\n");
            }

            char full_path[384];
            snprintf(full_path, 384, "/dev/%s", entry->d_name);
            accel_temp.fd = open(full_path, O_RDWR, 0);
            if (accel_temp.fd < 0) {
                fprintf(stderr, "Error: cannot open %s", full_path);
                exit(EXIT_FAILURE);
            }

            accel_list.push_back(accel_temp);
        }
    }

    closedir(dir);
}

void vam_backend::search_accel(hpthread_t *th) {
    HIGH_DEBUG(printf("[VAM BE] Searching accelerator for hpthread %s\n", th->get_name());)

    // We will find a candidate accelerator that has the lowest assigned load.
    // If no accelerator candidates are found, we will consider the CPU as the only candidate.
    std::pair<physical_accel_t *, unsigned> candidate_accel = {NULL, INT_MAX};

    bool accel_allocated = false;

    // First, update the active utilization of each accelerator
    check_utilization();

    for (physical_accel_t &accel : accel_list) {
        HIGH_DEBUG(
            printf("\n[VAM BE] Checking device %s.\n", accel.get_name());
            accel.dump();
        )

        // Is the accelerator suitable and not fully utilized for this primitive?
        if (accel.prim == th->get_prim() && !accel.valid_contexts.all()) {
            // Check if this accelerator's total load is less than the previous min
            if (accel.get_effective_load() < candidate_accel.second) {
                candidate_accel = {&accel, accel.get_effective_load()};
                HIGH_DEBUG(printf("[VAM BE] Device %s is a candidate!\n", accel.get_name());)
            }

            // Found one accelerator!
            accel_allocated = true;
        }
    }

    // If no candidate accelerator was found, we will create a new CPU thread for this node.
    if (accel_allocated == false) {
        // Create a new physical accelerator for this CPU thread
        physical_accel_t *cpu_thread = new physical_accel_t;
        cpu_thread->prim = hpthread_prim_t::NONE;
        std::strcpy(cpu_thread->devname, "CPU");

        candidate_accel = {cpu_thread, 0};
    }

    HIGH_DEBUG(printf("[VAM BE] Candidate for hpthread %s = %s!\n", th->get_name(), candidate_accel.first->get_name());)

    // Identify the valid context to allocate
    unsigned cur_context = 0;
    if (accel_allocated) {
        for (unsigned i = 0; i < MAX_CONTEXTS; i++) {
            if (!candidate_accel.first->valid_contexts[i]) {
                cur_context = i;
                break;
            }
        }
    }

    // Update the phy->virt for the chosen context with the hpthread
    phy_to_virt_mapping[candidate_accel.first][cur_context] = th;

    // Update the virt->phy for the hpthread
    virt_to_phy_mapping[th] = {candidate_accel.first, cur_context};

    // Mark the context as allocated.
    candidate_accel.first->valid_contexts.set(cur_context);

    // Assign the thread ID to this accelerator's context
    candidate_accel.first->thread_id[cur_context] = th->id;

    // Configure the device allocated
    if (accel_allocated) {
        configure_accel(th, candidate_accel.first, cur_context);
    } else {
        configure_cpu(th, candidate_accel.first);
    }
}

void vam_backend::configure_accel(hpthread_t *th, physical_accel_t *accel, unsigned context) {
    // Get the arguments for the hpthread
    hpthread_args *args = (hpthread_args *) th->args;

    HIGH_DEBUG(printf("[VAM BE] Configuring accel...\n");)

    // ESP defined data type for the pointer to the memory pool for accelerators.
    enum contig_alloc_policy policy;
    contig_handle_t *handle = lookup_handle(args->mem, &policy);

    // Configring all device-independent fields in the esp desc
    esp_access *generic_esp_access = (esp_access *) accel->esp_access_desc;
    {
        generic_esp_access->contig = contig_to_khandle(*handle);
        generic_esp_access->ddr_node = contig_to_most_allocated(*handle);
        generic_esp_access->alloc_policy = policy;
        generic_esp_access->run = true;
        generic_esp_access->coherence = ACC_COH_RECALL;
        generic_esp_access->spandex_conf = 0;
        generic_esp_access->start_stop = 1;
        generic_esp_access->p2p_store = 0;
        generic_esp_access->p2p_nsrcs = 0;
        generic_esp_access->src_offset = 0;
        generic_esp_access->dst_offset = 0;
        generic_esp_access->context_id = context;
        generic_esp_access->context_nprio = th->attr->nprio;
        generic_esp_access->valid_contexts = accel->valid_contexts.to_ulong();
    }

    // Call function for configuring other device-dependent fields
    accel->device_cfg(th, generic_esp_access);

    // Retrieve load assigned from device-dependent cfg function (scaled by priority)
    accel->context_load[context] = th->assigned_load / th->attr->nprio;
    th->active_load = accel->context_load[context];

    if (accel->init_done) {
        LOW_DEBUG(printf("[VAM BE] Adding to accel %s:%d for hpthread %s\n", accel->get_name(), context, th->get_name());)

        // The scheduling period for AVU is a multiple of the total load on the accelerator
        generic_esp_access->sched_period = NUM_SCHED_INTERVALS * accel->context_load[context];

        if (ioctl(accel->fd, accel->add_ctxt_ioctl, accel->esp_access_desc)) {
            perror("ioctl");
            exit(EXIT_FAILURE);
        }
    } else {
        LOW_DEBUG(printf("[VAM BE] Initializing accel %s:0 for hpthread %s\n", accel->get_name(), th->get_name());)

        generic_esp_access->sched_period += NUM_SCHED_INTERVALS * accel->context_load[context];

        if (ioctl(accel->fd, accel->init_ioctl, accel->esp_access_desc)) {
            perror("ioctl");
            exit(EXIT_FAILURE);
        }

        accel->init_done = true;
    }

    // Read the current time for when the accelerator is started.
    accel->context_start_cycles[context] = get_counter();
    accel->context_active_cycles[context] = 0;
}

void vam_backend::configure_cpu(hpthread_t *th, physical_accel_t *accel) {
    LOW_DEBUG(printf("[VAM BE] Configuring CPU for hpthread %s\n", th->get_name());)

    // Create a new CPU thread for the SW implementation of this node.
    pthread_t cpu_thread;
    if (pthread_create(&cpu_thread, NULL, th->start_routine, th->args) != 0) {
        perror("Failed to create CPU thread\n");
    }

    // Add this thread to the cpu thread list of VAM
    cpu_thread_list.push_back(cpu_thread);

    // Map the physical accel struct to the new CPU thread.
    accel->accel_id = cpu_thread_list.size() - 1;
}

bool vam_backend::release_accel(hpthread_t *th) {
    if (!virt_to_phy_mapping.count(th)) return false;

    std::pair<physical_accel_t *, unsigned> &accel_mapping = virt_to_phy_mapping[th];

    physical_accel_t *accel = accel_mapping.first;
    unsigned context = accel_mapping.second;

    LOW_DEBUG(printf("[VAM BE] Releasing accel %s:%d for hpthread %s\n", accel->get_name(), context, th->get_name());)

    // Free the allocated context.
    accel->valid_contexts.reset(context);

    // Configring all the device-independent fields in the esp desc
    esp_access *generic_esp_access = (esp_access *) accel->esp_access_desc;
    {
        generic_esp_access->context_id = context;
        generic_esp_access->valid_contexts = accel->valid_contexts.to_ulong();
    }

    if (ioctl(accel->fd, accel->del_ctxt_ioctl, accel->esp_access_desc)) {
        perror("ioctl");
        exit(EXIT_FAILURE);
    }

    // Delete the entry for this context on the physical accel map
    phy_to_virt_mapping[accel][context] = NULL;

    // TODO: only considering accelerator mappings for now. Need to fix CPU.

    // Delete the map entry for the routine
    virt_to_phy_mapping.erase(th);

    // If there is no context active, we should re-init the accelerator the next time
    if (accel->valid_contexts.none()) {
        accel->init_done = false;
    } 

    return true;
}

bool vam_backend::setprio_accel(hpthread_t *th) {
    if (!virt_to_phy_mapping.count(th)) return false;

    std::pair<physical_accel_t *, unsigned> &accel_mapping = virt_to_phy_mapping[th];

    physical_accel_t *accel = accel_mapping.first;
    unsigned context = accel_mapping.second;

    LOW_DEBUG(printf("[VAM BE] Setting priority of accel %s:%d to %d for hpthread %s\n", accel->get_name(), context, th->attr->nprio, th->get_name());)

    // Configring all the device-independent fields in the esp desc
    esp_access *generic_esp_access = (esp_access *) accel->esp_access_desc;
    {
        generic_esp_access->context_id = context;
        generic_esp_access->context_nprio = th->attr->nprio;
    }

    if (ioctl(accel->fd, accel->setprio_ioctl, accel->esp_access_desc)) {
        perror("ioctl");
        exit(EXIT_FAILURE);
    }

    return true;
}

void vam_backend::check_utilization() {
    for (physical_accel_t &accel : accel_list) {
        struct avu_mon_desc mon;

        HIGH_DEBUG(
            float total_util = 0.0;
            printf("[VAM BE] Util of %s: ", accel.get_name());
        )

        if (ioctl(accel.fd, ESP_IOC_MON, &mon)) {
            perror("ioctl");
            exit(EXIT_FAILURE);
        }
        uint64_t *mon_extended = (uint64_t *) mon.util;

        for (int i = 0; i < MAX_CONTEXTS; i++) {
            if (accel.valid_contexts[i]) {
                // Get the utilization in the previous monitor period
                uint64_t elapsed_cycles = get_counter() - accel.context_start_cycles[i];
                uint64_t util_cycles = mon_extended[i] - accel.context_active_cycles[i];
                float util = (float) util_cycles/elapsed_cycles;
                if (util_cycles > elapsed_cycles) util = 0.0;
                // Set the cycles for the next period
                accel.context_start_cycles[i] = get_counter();
                accel.context_active_cycles[i] = mon_extended[i];
                accel.context_util[i] = util;
                hpthread_t *th = phy_to_virt_mapping[&accel][i];
                th->active_load = accel.context_util[i] * accel.context_load[i];
                float expected_util = (float) accel.context_load[i] / accel.get_total_load();
                float effective_util = accel.context_util[i] / expected_util;
                accel.effective_load[i] = (unsigned) (accel.context_load[i] * effective_util);

                HIGH_DEBUG(
                    total_util += accel.context_util[i];
                    printf("C%d(%d)=%0.2f%%, ", i, th->attr->nprio, accel.context_util[i] * 100);
                )
            }
        }
        HIGH_DEBUG(printf("total=%0.2f%%, load=%d\n", total_util * 100, accel.get_effective_load());)
    }
}

bool vam_backend::check_load_balance() {
    // First, update the active utilization of each accelerator
    check_utilization();

    // Print out the total utilization
    LOW_DEBUG(
        for (physical_accel_t &accel : accel_list) {
            float total_util = 0.0;
            printf("[VAM BE] Util of %s: ", accel.get_name());
            for (int i = 0; i < MAX_CONTEXTS; i++) {
                if (accel.valid_contexts[i]) {
                    hpthread_t *th = phy_to_virt_mapping[&accel][i];
                    printf("C%d(%d)=%0.2f%%, ", i, th->attr->nprio, accel.context_util[i] * 100);
                    total_util += accel.context_util[i];
                }
            }
            LOW_DEBUG(printf("total=%0.2f%%, load=%d\n", total_util * 100, accel.get_effective_load());)
        }
    )

    // Check whether there is load imbalance across accelerators
    // - calculate average and max/min load across all accelerators with non-zero load assigned
    float average_load = 0;
    unsigned count = 0;
    unsigned max_load = 0;
    unsigned min_load = INT_MAX;
    const float imbalance_limit = 0.25;
    bool skip_load_balance = true;

    for (physical_accel_t &accel : accel_list) {
        unsigned cur_load = accel.get_effective_load();
        if (cur_load < min_load) min_load = cur_load;
        if (cur_load > max_load) max_load = cur_load;
        average_load += cur_load;
        count++;
        if (accel.valid_contexts.count() > 1) skip_load_balance = false;
        HIGH_DEBUG(printf("[VAM BE] Current load of %s = %d\n", accel.get_name(), cur_load);)
    }

    // If none of the accelerators have more than one valid context, there's no need for load balancing
    if (skip_load_balance) return false;

    average_load = average_load/count;
    HIGH_DEBUG(printf("[VAM BE] Max load = %d, min load = %d, avg load = %0.2f\n", max_load, min_load, average_load);)

    float new_load_imbalance = ((float) (max_load - min_load)) / (average_load);
    // Check if the new load imbalance is significantly worse than the previous checkpointed value
    // If not, do not perform any load balancing.
    if (new_load_imbalance - load_imbalance > imbalance_limit) {
        load_imbalance = new_load_imbalance;
        return true;
    } else {
        load_imbalance = new_load_imbalance;
        return false;
    }
}

// Custom comparator for min-heap based on total_load assigned
struct AccelCompare {
    bool operator()(const physical_accel_t a, const physical_accel_t b) const {
        return a.get_effective_load() > b.get_effective_load(); // min-heap: less total_load comes first
    }
};

void vam_backend::load_balance() {
    // Per-primitive vectors sorted by the load
    std::unordered_map<hpthread_prim_t, std::vector<hpthread_t *>> active_thread_list;

    // Add all the active hpthreads to active_thread_list
    for (const auto &accel_mapping : virt_to_phy_mapping) {
        hpthread_t *th = accel_mapping.first;
        active_thread_list[th->attr->prim].push_back(th);
    }

    // Iterate through the thread list for each primitive
    for (auto &prim_list : active_thread_list) {
        std::vector<hpthread_t *> &list = prim_list.second;
        hpthread_prim_t prim = prim_list.first;
        HIGH_DEBUG(printf("[VAM BE] Starting load balancer for %s...\n", get_prim_name(prim));)

        // Sort the list in active_thread_list by active load
        std::sort(list.begin(), list.end(),
                [](const hpthread_t *a, const hpthread_t *b) {
                    return a->active_load > b->active_load;
                });

        HIGH_DEBUG(
            printf("[VAM BE] Sorted hpthreads for %s:\n", get_prim_name(prim));
            for (hpthread_t *th: list) {
                printf("\t- %s (old map: %s)\n", th->get_name(), virt_to_phy_mapping[th].first->get_name());
            }
        )

        // Create a temporary list of the physical accelerators with the same accel IDs
        std::priority_queue<physical_accel_t, std::vector<physical_accel_t>, AccelCompare> accel_queue;

        // Create new duplicate accelerators for accel_list and add to the load sorted queue
        for (physical_accel_t &accel : accel_list) {
            if (accel.prim == prim) {
                physical_accel_t a;
                a.accel_id = accel.accel_id;
                accel_queue.push(a);
            }
        }

        // We will recompute the load imbalance of the new mapping
        float average_load = 0;
        unsigned count = 0;
        unsigned max_load = 0;
        unsigned min_load = INT_MAX;
        const float imbalance_limit = 0.25;

        // Assign each thread using a greedy longest processing time algorithm.
        // Also, check which hpthreads need to be released for new accel
        // and create smaller new_mapping.
        std::vector<hpthread_t *> del_threads;
        std::unordered_map<hpthread_t *, physical_accel_t *> new_mapping;
        HIGH_DEBUG(printf("[VAM BE] Running LPT for %s...\n", get_prim_name(prim));)
        while (!list.empty()) {
            // Get the least loaded device
            physical_accel_t accel = accel_queue.top(); accel_queue.pop();
            // Get the highest load hpthread
            hpthread_t *th = list.front();
            list.erase(list.begin());

            HIGH_DEBUG(printf("[VAM BE] Map %s to %s\n", th->get_name(), accel_list[accel.accel_id].get_name());)

            // Get the original mapping for this hpthread
            std::pair<physical_accel_t *, unsigned> old_mapping = virt_to_phy_mapping[th];
            physical_accel_t *old_accel = old_mapping.first;
            unsigned old_context = old_mapping.second;
            if (old_accel->accel_id == accel.accel_id) {
                // Mapped to the same accelerator currently.
                // Do nothing
            } else {
                // Need to release this hpthread and remap
                del_threads.push_back(th);
                new_mapping[th] = &accel_list[accel.accel_id];
            }

            // Add to the lowest empty context of accel
            // -- this is only for LPT and not used after this loop
            for (unsigned i = 0; i < MAX_CONTEXTS; i++) {
                if (!accel.valid_contexts[i]) {
                    accel.context_load[i] = old_accel->context_load[old_context];
                    accel.effective_load[i] = old_accel->effective_load[old_context];
                    accel.valid_contexts.set(i);
                    break;
                }
            }

            // Don't push it back into the queue if it is full
            if (!accel.valid_contexts.all()) {
                accel_queue.push(accel);
            } else {
                // Since this accel is not being pushed back, we will calculate load here
                unsigned cur_load = accel.get_effective_load();
                // unsigned cur_load = accel.get_effective_load();
                if (cur_load < min_load) min_load = cur_load;
                if (cur_load > max_load) max_load = cur_load;
                average_load += cur_load;
                count++;
                HIGH_DEBUG(printf("[VAM BE] Current load of accel ID %d = %d\n", accel.accel_id, cur_load);)

                // Print out the total utilization
                HIGH_DEBUG(
                    printf("[VAM BE] New util of %s: ", accel_list[accel.accel_id].get_name());
                    for (int i = 0; i < MAX_CONTEXTS; i++) {
                        if (accel.valid_contexts[i]) {
                            printf("C%d=%d, ", i, accel.effective_load[i]);
                        }
                    }
                    printf("total=%d\n", accel.get_effective_load());
                )
            }
        }

        while (!accel_queue.empty()) {
            physical_accel_t accel = accel_queue.top(); accel_queue.pop();
            unsigned cur_load = accel.get_effective_load();
            if (cur_load < min_load) min_load = cur_load;
            if (cur_load > max_load) max_load = cur_load;
            average_load += cur_load;
            count++;
            HIGH_DEBUG(printf("[VAM BE] Current load of accel ID %d = %d\n", accel.accel_id, cur_load);)

            // Print out the total utilization
            HIGH_DEBUG(
                printf("[VAM BE] New util of %s: ", accel_list[accel.accel_id].get_name());
                for (int i = 0; i < MAX_CONTEXTS; i++) {
                    if (accel.valid_contexts[i]) {
                        printf("C%d=%d, ", i, accel.effective_load[i]);
                    }
                }
                printf("total=%d\n", accel.get_effective_load());
            )
        }

        average_load = average_load/count;
        HIGH_DEBUG(printf("[VAM BE] Max load = %d, min load = %d, avg load = %0.2f\n", max_load, min_load, average_load);)

        // Check whether there is load imbalance across accelerators for the new mapping
        float new_load_imbalance = ((float) (max_load - min_load)) / (average_load);
        LOW_DEBUG(printf("[VAM BE] Old load imbalance = %f, new load imbalance = %f\n", load_imbalance, new_load_imbalance);)
        if (load_imbalance - new_load_imbalance < imbalance_limit) continue;

        // First, release threads in the del_threads vector
        for (hpthread_t *th : del_threads) {
            // Need to release th from the accelerator
            if (!release_accel(th)) {
                perror("hpthread unmapped earlier!");
                exit(1);
            }
        }

        // Next, reconfigure threads in the new_mapping map
        for (auto &n : new_mapping) {
            hpthread_t *th = n.first;
            physical_accel_t *accel = n.second;

            // Identify the valid context to allocate
            unsigned cur_context = 0;
            for (unsigned i = 0; i < MAX_CONTEXTS; i++) {
                if (!accel->valid_contexts[i]) {
                    cur_context = i;
                    break;
                }
            }

            // Update the phy->virt for the chosen context with the hpthread
            phy_to_virt_mapping[accel][cur_context] = th;

            // Update the virt->phy for the hpthread
            virt_to_phy_mapping[th] = {accel, cur_context};

            // Mark the device as allocated.
            accel->valid_contexts.set(cur_context);

            // Assign the thread ID to this accelerator's context
            accel->thread_id[cur_context] = th->id;

            // Configure the device allocated
            configure_accel(th, accel, cur_context);
        }
    }
}

void vam_backend::run_util_mon() {
    while(1) {
        sleep(4); // 2 seconds

        // Create a utilization entry
        std::unordered_map<physical_accel_t *, std::array<std::pair<float, unsigned>, MAX_CONTEXTS>> util_map;
        epoch_utilization.push_back(util_map);

        // Iterate through all accelerators and copy their utilizations
        for (physical_accel_t &accel : accel_list)
            for (int i = 0; i < MAX_CONTEXTS; i++)
                if (accel.valid_contexts[i]) {
                    unsigned th_id = phy_to_virt_mapping[&accel][i]->user_id;
                    epoch_utilization.back()[&accel][i] = {accel.context_util[i], th_id};
                }
    }
}

void vam_backend::print_report() {
    // Print out the total utilization
    printf("------------------------------------------------------------------------------------------------------\n");
    printf("  #\tAccel\t\t\tC0\t\tC1\t\tC2\t\tC3\t\tTotal\n");
    printf("------------------------------------------------------------------------------------------------------\n");
    for (size_t i = 0; i < epoch_utilization.size(); i++) {
        printf("  %lu", i);
        for (physical_accel_t &accel : accel_list) {
            printf("\t%s\t\t", accel.get_name());
            float total_util = 0.0;
            for (int j = 0; j < MAX_CONTEXTS; j++) {
                total_util += epoch_utilization[i][&accel][j].first;
                printf("%0.2f%%(%d)\t", epoch_utilization[i][&accel][j].first*100, epoch_utilization[i][&accel][j].second);
            }
            printf("%0.2f%%\n", total_util*100);
        }
    }
    epoch_utilization.clear();
}
   