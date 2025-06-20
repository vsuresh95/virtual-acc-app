#include <vam_backend.h>

// External instance of hpthread interface
extern hpthread_intf_t intf;

// Used in wakeup_vam() for small applications without multi-threading.
std::thread vam_th;

// Function to wake up VAM for the first time
void wakeup_vam() {
	printf("[VAM BE] Launching a new thread for VAM BACKEND!\n");

    // Create an object of VAM and launch a std::thread for VAM
    vam_backend *be = new vam_backend;
    vam_th = std::thread(&vam_backend::run_backend, be);
    vam_th.detach();

    // TODO: this function is only used for small applications
    // where we do not want impose the need for multi-threading
    // and launching VAM from the application. In larger multi-
    // threaded applications, this must be done in the startup.
}

void vam_backend::run_backend() {
	DEBUG(printf("[VAM BE] Hello from VAM BACKEND!\n");)

    // populate the list of physical accelerators in the system
    probe_accel();

    unsigned vam_sleep = 0;

    // Run loop will run forever
    while (1) {
        // Test the interface state
        vam_state_t state = intf.test();

        switch(state) {
            case vam_state_t::IDLE: {
                // Iterate through all accelerators in the system and report the utilization
                for (physical_accel_t &accel : accel_list) {
                    struct avu_mon_desc mon;

                    printf("[VAM BE] Util of %s: ", accel.get_name());
                    if (ioctl(accel.fd, ESP_IOC_MON, &mon)) {
                        perror("ioctl");
                        exit(EXIT_FAILURE);
                    }

                    float total_util = 0.0;
                    uint64_t *mon_extended = (uint64_t *) mon.util;

                    for (int i = 0; i < MAX_CONTEXTS; i++) {
                        if (accel.valid_contexts[i]) {
                            // Get the utilization in the previous monitor period
                            uint64_t elapsed_cycles = get_counter() - accel.context_start_cycles[i];
                            uint64_t util_cycles = mon_extended[i] - accel.context_active_cycles[i];
                            float util = (float) util_cycles/elapsed_cycles;
                            if (util_cycles > elapsed_cycles) util = 0.0;
                            total_util += util;
                            printf("C%d=%0.2f%%, ", i, util * 100);
                            // Set the cycles for the next period
                            accel.context_start_cycles[i] = get_counter();
                            accel.context_active_cycles[i] = mon_extended[i];
                        }
                    }
                    
                    printf("total=%0.2f%%\n", total_util * 100);
                }

                sleep(std::min((const int) vam_sleep++, 10));
                break;
            }
            case vam_state_t::CREATE: {
                DEBUG(printf("\n[VAM BE] Received a request for creating hpthread %s\n", intf.th->get_name());)

                search_accel(intf.th);

                // Set the interface state to DONE
                intf.set(vam_state_t::DONE);

                DEBUG(printf("[VAM BE] Completed the processing of request.\n");)

                // Reset sleep counter after servicing a request
                vam_sleep = 0;
                break;
            }
            case vam_state_t::JOIN: {
                DEBUG(printf("\n[VAM BE] Received a request for joining hpthread %s\n", intf.th->get_name());)

                if (!release_accel(intf.th)) {
                    perror("hpthread unmapped earlier!");
                    exit(1);
                }

                // Set the interface state to DONE
                intf.set(vam_state_t::DONE);

                DEBUG(printf("[VAM BE] Completed the processing of request.\n");)

                // Reset sleep counter after servicing a request
                vam_sleep = 0;
                break;
            }
            default: 
                break;
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

    DEBUG(printf("[VAM BE] Performing device probe.\n");)

    unsigned device_id = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (fnmatch("*_stratus.*", entry->d_name, FNM_NOESCAPE) == 0) {
            physical_accel_t accel_temp;
            accel_temp.accel_id = device_id++;
            accel_temp.valid_contexts.reset();
            for (int i = 0; i < MAX_CONTEXTS; i++) {
                accel_temp.thread_id[i] = -1;
                accel_temp.context_quota[i] = 0;
                accel_temp.context_start_cycles[i] = 0;
                accel_temp.context_active_cycles[i] = 0;
            }
            std::strcpy(accel_temp.devname, entry->d_name);
            accel_temp.init_done = false;

            // Print out debug message
            DEBUG(printf("[VAM BE] Discovered device %d: %s\n", device_id, accel_temp.devname);)

            if (fnmatch("audio_fft*", entry->d_name, FNM_NOESCAPE) == 0) {
                audio_fft_probe(&accel_temp);
            } else if (fnmatch("audio_fir*", entry->d_name, FNM_NOESCAPE) == 0) {
            } else if (fnmatch("audio_ffi*", entry->d_name, FNM_NOESCAPE) == 0) {
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
    DEBUG(printf("[VAM BE] Searching accelerator for hpthread %s\n", th->get_name()));

    // We will find a candidate accelerator that has the lowest allocated weighted by priorities.
    // If no accelerator candidates are found, we will consider the CPU as the only candidate.
    std::pair<physical_accel_t *, unsigned> candidate_accel = {NULL, INT_MAX};

    bool accel_allocated = false;

    for (physical_accel_t &accel : accel_list) {
        DEBUG(
            printf("\n[VAM BE] Checking device %s.\n", accel.get_name());
            accel.dump();
        )

        // Is the accelerator suitable and not fully utilized for this primitive?
        if (accel.prim == th->get_prim() && !accel.valid_contexts.all()) {
            // Check if this accelerator's total quota is less than the previous min
            if (accel.get_total_quota() < candidate_accel.second) {
                candidate_accel = std::make_pair(&accel, accel.get_total_quota());
            }

            // Found one accelerator!
            accel_allocated = true;

            DEBUG(printf("[VAM BE] Device %s is a candidate!\n", accel.get_name());)
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

    DEBUG(printf("[VAM BE] Candidate for hpthread %s = %s!\n", th->get_name(), candidate_accel.first->get_name()));

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
    virt_to_phy_mapping[th] = std::make_pair(candidate_accel.first, cur_context);

    // Mark the device as allocated.
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

    DEBUG(printf("[VAM BE] Configuring accel...\n");)

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
        generic_esp_access->valid_contexts = accel->valid_contexts.to_ulong();
    }

    // Retrieve quota assigned from device-dependent cfg function
    unsigned assigned_quota;

    // Call function for configuring other device-dependent fields
    accel->device_cfg(th, generic_esp_access, accel->valid_contexts.to_ulong(), &assigned_quota);

    accel->context_quota[context] = assigned_quota;

    if (accel->init_done) {
        printf("[VAM BE] Adding to accel %s:%d for hpthread %s\n", accel->get_name(), context, th->get_name());

        if (ioctl(accel->fd, accel->add_ctxt_ioctl, accel->esp_access_desc)) {
            perror("ioctl");
            exit(EXIT_FAILURE);
        }
    } else {
        printf("[VAM BE] Initializing accel %s:0 for hpthread %s\n", accel->get_name(), th->get_name());

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
    printf("[VAM BE] Configuring CPU for hpthread %s\n", th->get_name());

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

    printf("[VAM BE] Releasing accel %s:%d for hpthread %s\n", accel->get_name(), context, th->get_name());

    // Configring all the device-independent fields in the esp desc
    esp_access *generic_esp_access = (esp_access *) accel->esp_access_desc;
    {
        generic_esp_access->context_id = context;
        generic_esp_access->valid_contexts = accel->valid_contexts.reset(context).to_ulong();
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

    return true;
}
