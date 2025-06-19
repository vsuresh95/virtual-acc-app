#include <vam-module.hpp>

extern void audio_fft_access_cfg(df_node_t *node, esp_access *generic_esp_access, unsigned valid_contexts);
extern void audio_fir_access_cfg(df_node_t *node, esp_access *generic_esp_access, unsigned valid_contexts);
extern void audio_ffi_access_cfg(df_node_t *node, esp_access *generic_esp_access, unsigned valid_contexts);

void vam_worker::run() {
	printf("[VAM] Hello from vam_worker!\n");

    // populate the list of physical accelerators in the system
    probe_accel();

    unsigned vam_sleep = 0;

    while (1) {
        // Test if a task is submitted by atomically checking if the state is ONGOING
        hpthread_req_t req;
        hpthread_routine_t *routine = test_hpthread_req(&req);

        if (routine != NULL) {
            switch (req) {
                case hpthread_req_t::CREATE: {
                    DEBUG(printf("[VAM] Received a request for creating hpthread %s\n", routine->get_name());)

                    // search the available accelerators if any can satisfy the request
                    bool success = search_accel(routine);

                    // Acknowledge the request with the success code
                    ack_hpthread_req(success);

                    DEBUG(printf("[VAM] Completed the processing for request.\n");)

                    // Re-evaluate all the virtual instance ticket allocations
                    if (success) eval_ticket_alloc();
                    break;
                }
                case hpthread_req_t::JOIN: {
                    DEBUG(printf("[VAM] Received a request for joining hpthread %s\n", routine->get_name());)

                    // search the available accelerators if any can satisfy the request
                    bool success = release_accel(routine);

                    // Acknowledge the request with the success code
                    ack_hpthread_req(success);

                    DEBUG(printf("[VAM] Completed the processing for request.\n");)

                    // Re-evaluate all the virtual instance ticket allocations
                    if (success) eval_ticket_alloc();
                    break;
                }
                default: {
                    break;
                }
            }

            // Reset sleep counter after servicing a request
            vam_sleep = 0;
        } else {
            for (physical_accel_t &accel : accel_list) {
                struct avu_mon_desc mon;

                printf("[VAM] Util of %s: ", accel.get_name());
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
        }
    }
}

void vam_worker::probe_accel() {
    // Open the devices directory to search for accels
    DIR *dir = opendir("/dev/");
    if (!dir) {
        perror("Failed to open directory");
        exit(1);
    }

    // Search for all stratus accelerators and fill into accel_list
    struct dirent *entry;

    printf("\n[VAM] Performing device probe.\n");

    unsigned device_id = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (fnmatch("*_stratus.*", entry->d_name, FNM_NOESCAPE) == 0) {
            physical_accel_t accel_temp;
            accel_temp.accel_id = device_id++;
            accel_temp.valid_contexts.reset();
            for (char *id : accel_temp.thread_id)
                std::strcpy(id, "UNALLOC");
            std::strcpy(accel_temp.devname, entry->d_name);
            accel_temp.init_done = false;

            // Print out debug message
            DEBUG(printf("[VAM] Discovered device %d: %s\n", device_id, accel_temp.devname);)

            if (fnmatch("audio_fft*", entry->d_name, FNM_NOESCAPE) == 0) {
                accel_temp.prim = AUDIO_FFT;
                accel_temp.init_ioctl = AUDIO_FFT_STRATUS_INIT_IOC_ACCESS;
                accel_temp.add_ctxt_ioctl = AUDIO_FFT_STRATUS_ADD_IOC_ACCESS;
                accel_temp.del_ctxt_ioctl = AUDIO_FFT_STRATUS_DEL_IOC_ACCESS;
                accel_temp.tickets = AUDIO_FFT_TICKETS;

                // Create a new access struct and track within the device struct
                struct audio_fft_stratus_access *audio_fft_desc = new struct audio_fft_stratus_access;
                accel_temp.esp_access_desc = audio_fft_desc;
                accel_temp.device_access_cfg = audio_fft_access_cfg;
            } else if (fnmatch("audio_fir*", entry->d_name, FNM_NOESCAPE) == 0) {
                accel_temp.prim = AUDIO_FIR;
                accel_temp.init_ioctl = AUDIO_FIR_STRATUS_INIT_IOC_ACCESS;
                accel_temp.add_ctxt_ioctl = AUDIO_FIR_STRATUS_ADD_IOC_ACCESS;
                accel_temp.del_ctxt_ioctl = AUDIO_FIR_STRATUS_DEL_IOC_ACCESS;
                accel_temp.tickets = AUDIO_FIR_TICKETS;

                // Create a new access struct and track within the device struct
                struct audio_fir_stratus_access *audio_fir_desc = new struct audio_fir_stratus_access;
                accel_temp.esp_access_desc = audio_fir_desc;
                accel_temp.device_access_cfg = audio_fir_access_cfg;
            } else if (fnmatch("audio_ffi*", entry->d_name, FNM_NOESCAPE) == 0) {
                accel_temp.prim = AUDIO_FFI;
                accel_temp.init_ioctl = AUDIO_FFI_STRATUS_INIT_IOC_ACCESS;
                accel_temp.add_ctxt_ioctl = AUDIO_FFI_STRATUS_ADD_IOC_ACCESS;
                accel_temp.del_ctxt_ioctl = AUDIO_FFI_STRATUS_DEL_IOC_ACCESS;
                accel_temp.tickets = AUDIO_FFI_TICKETS;

                // Create a new access struct and track within the device struct
                struct audio_ffi_stratus_access *audio_ffi_desc = new struct audio_ffi_stratus_access;
                accel_temp.esp_access_desc = audio_ffi_desc;
                accel_temp.device_access_cfg = audio_ffi_access_cfg;
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

bool vam_worker::search_accel(hpthread_routine_t *routine) {
    // We will maintain a queue of nodes to-be-visited in BFS order and a list of visited nodes
    std::queue<df_node_t *> q;
    std::unordered_set<df_node_t *> visited;

    // Do BFS traversal of the routine DFG by starting at the entry node
    df_node_t *entry = routine->get_entry();
    q.push(entry);
    visited.insert(entry);

    // This variable tracks the total number of tickets allocated to this graph.
    unsigned tickets_allocated = 0;

    while (!q.empty()) {
        df_node_t *current = q.front();
        q.pop();

        DEBUG(printf("\tFound node %s\n", current->dump_prim()));

        // Ensure that the current node is an entry or exit
        if (current->get_prim() != NONE) {
            // For each non-NONE node, we will find a candidate accelerator that gives the highest number of
            // tickets. If no accelerator candidates are found, we will consider the CPU as the only candidate.
            std::pair<physical_accel_t *, unsigned> candidate_accel = {NULL, 0};

            for (physical_accel_t &accel : accel_list) {
                DEBUG(
                    printf("\n[VAM] Checking device %s.\n", accel.get_name());
                    accel.dump();
                )

                if (accel.prim == current->get_prim() && !accel.valid_contexts.all()) {
                    // Check if this accelerator's available tickets is more than the previous max
                    if (accel.get_avail_tickets() > candidate_accel.second) {
                        candidate_accel = std::make_pair(&accel, accel.get_avail_tickets());
                    }
                }
            }

            // If no candidate accelerator was found, we will create a new CPU thread for this node.
            if (candidate_accel.first == NULL) {
                // Create a new physical accelerator for this CPU thread
                physical_accel_t *cpu_thread = new physical_accel_t;
                cpu_thread->prim = NONE;
                cpu_thread->tickets = CPU_TICKETS;
                std::strcpy(cpu_thread->devname, "CPU");

                candidate_accel = {cpu_thread, 0};
            }

            DEBUG(printf("[VAM] Best candidate for node %s of routine %s = %s!\n", current->dump_prim(),
                    current->get_root()->get_name(), candidate_accel.first->get_name()));

            // We have the best candidate for this node, but what if there are better candidates
            // when decomposed. Therefore, we will check the child graph if the node is internal.
            bool alloc_internal = false;

            if (current->is_internal()) {
                DEBUG(printf("\n[VAM] Searching the child graph of int_node %s\n", ((df_int_node_t *) current)->get_name()));

                // If the child graph score is less than (or equal to) the candidate found in parent node,
                // then we need to choose the parent candidate instead.
                if (search_accel((df_int_node_t *) current) <= candidate_accel.second) {
                    DEBUG(printf("[VAM] Traversing children of %s for deletion\n", ((df_int_node_t *) current)->get_name());)
                    // Iterate through all the child nodes
                    for (auto child : ((df_int_node_t *) current)->child_graph->df_node_list) {
                        // Do nothing for NONE children
                        if (child->get_prim() == NONE) continue;

                        DEBUG(printf("\t%s\n", child->dump_prim());)
                        // Set the appropriate context of the device as unallocated and delete if CPU device
                        physical_accel_t *child_device = virt_to_phy_mapping[current->get_root()][child].first;
                        unsigned child_context = virt_to_phy_mapping[current->get_root()][child].second;
                        if (child_device->prim == NONE) {
                            delete child_device;
                        } else {
                            phy_to_virt_mapping[child_device][child_context] = NULL;
                            child_device->valid_contexts.reset(child_context);
                            std::strcpy(child_device->thread_id[child_context], "UNALLOC");
                        }

                        // Delete the map entry for the child node
                        virt_to_phy_mapping[current->get_root()].erase(child);
                    }
                } else {
                    alloc_internal = true;
                }
            }

            // If the node was not allocated internally (or if the node is not internal),
            // assign PHY<->VIRT mapping and set the context of the device as allocated
            if (!alloc_internal)
            {
                // If the device is not fully allocated, identify the valid context to allocate
                unsigned cur_context;
                for (unsigned i = 0; i < MAX_CONTEXTS; i++) {
                    if (!candidate_accel.first->valid_contexts[i]) {
                        cur_context = i;
                        break;
                    }
                }

                // Update the phy->virt for the chosen context with the root node
                phy_to_virt_mapping[candidate_accel.first][cur_context] = current->get_root();

                // Update the virt->node->phy
                virt_to_phy_mapping[current->get_root()][current] = std::make_pair(candidate_accel.first, cur_context);

                // Mark the device as allocated.
                candidate_accel.first->valid_contexts.set(cur_context);
                std::strcpy(candidate_accel.first->thread_id[cur_context], current->get_root()->get_name());

                DEBUG(printf("[VAM] Temporarily allocated device %s:%d to node %s of routine %s!\n", candidate_accel.first->get_name(), cur_context, current->dump_prim(), current->get_root()->get_name());)

                // Increment the total tickets allocated with this candidate accel
                tickets_allocated += candidate_accel.second;
            }

        }

        // Iterate through all out edges of this node and add the destinations to the visitor set and the BFS queue.
        for (df_edge_t *out : current->out_edges) {
            // Check that the consumer was not visited earlier and push into the queue.
            if (visited.insert(out->get_destination()).second) {
                q.push(out->get_destination());
            }

            // Check if the queue associated with this edge is not yet allocated (might be the
            // case for child graphs). Further, check if the edge is a binding to a parent node edge.
            // This also covers the fact that bindings will usually be from NONE nodes, which
            // will not have mem_pool or payload size. If yes, you need to set the mem queue
            // of this edge to the mem queue of the parent bound edge. If no to both,
            // allocate it here for the size specified.
            if (out->data == NULL) {
                if (out->is_binding()) {
                    out->set_mem_queue(out->get_bind()->get_mem_queue());
                } else {
                    mem_queue_t *temp_queue = new mem_queue_t(current->get_params()->mem_pool, out->payload_size);
                    out->set_mem_queue(temp_queue);
                }
            }
        }
    }

    // Once all nodes in the graph have been processed, we will return immediately if it's a sub-graph
    // or configure the accelerators if it is the root graph of the routine.
    // We will return whether were able to allocate any accelerator within this child graph; if not, we
    // we will allocate the SW function for it.
    DEBUG(printf("[VAM] Returning total tickets for graph %s = %d\n", routine->get_name(), tickets_allocated));
    if (!routine->is_root()) return tickets_allocated;

    // Iterate through all the nodes in the virt to phy mapping to configure the accelerators
    std::unordered_map<df_node_t *, std::pair<physical_accel_t *, unsigned>> accel_candidates = virt_to_phy_mapping[routine];
    for (const auto &node_accel_pair : accel_candidates) {
        std::pair<physical_accel_t *, unsigned> accel = node_accel_pair.second;

        if (accel.first->prim == NONE) {
            configure_cpu(node_accel_pair.first, accel.first);
        } else {
            configure_accel(node_accel_pair.first, accel.first, accel.second);
        }
    }

    // If we reached this point, we successfully allocated accelerators to all tasks in the DFG.
    return true;
}

void vam_worker::configure_accel(df_node_t *node, physical_accel_t *accel, unsigned context) {
    if (accel->init_done) {
        printf("[VAM] Adding to accel %s:%d for node %s in routine %s\n", accel->get_name(), context, node->dump_prim(), node->get_root()->get_name());

        // ESP defined data type for the pointer to the memory pool for accelerators.
        enum contig_alloc_policy policy;
        contig_handle_t *handle = lookup_handle(node->get_params()->mem_pool->hw_buf, &policy);

        // Configring all the device-independent fields in the esp desc
        esp_access *generic_esp_access = (esp_access *) accel->esp_access_desc;
        {
            generic_esp_access->contig = contig_to_khandle(*handle);
            generic_esp_access->ddr_node = contig_to_most_allocated(*handle);
            generic_esp_access->alloc_policy = policy;
            generic_esp_access->context_id = context;
            generic_esp_access->valid_contexts = accel->valid_contexts.to_ulong();
        }

        // Call function for configuring other accel-dependent fields
        accel->device_access_cfg(node, generic_esp_access, accel->valid_contexts.to_ulong());

        if (ioctl(accel->fd, accel->add_ctxt_ioctl, accel->esp_access_desc)) {
            perror("ioctl");
            exit(EXIT_FAILURE);
        }

        // Read the current time for when the accelerator is started.
        accel->context_start_cycles[context] = get_counter();
        accel->context_active_cycles[context] = 0;
    } else {
        printf("[VAM] Initializing accel %s:0 for node %s in routine %s\n", accel->get_name(), node->dump_prim(), node->get_root()->get_name());

        // ESP defined data type for the pointer to the memory pool for accelerators.
        enum contig_alloc_policy policy;
        contig_handle_t *handle = lookup_handle(node->get_params()->mem_pool->hw_buf, &policy);

        // Configring all the device-independent fields in the esp desc
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
            generic_esp_access->context_id = 0;
            generic_esp_access->valid_contexts = 0x1;
        }

        // Call function for configuring other accel-dependent fields
        accel->device_access_cfg(node, generic_esp_access, 0x1);

        if (ioctl(accel->fd, accel->init_ioctl, accel->esp_access_desc)) {
            perror("ioctl");
            exit(EXIT_FAILURE);
        }

        accel->init_done = true;

        // Read the current time for when the accelerator is started.
        accel->context_start_cycles[0] = get_counter();
        accel->context_active_cycles[0] = 0;
    }
}

void vam_worker::configure_cpu(df_node_t *node, physical_accel_t *accel) {
    printf("[VAM] Configuring CPU for node %s in routine %s\n", node->dump_prim(), node->get_root()->get_name());

    // Create a new CPU thread for the SW implementation of this node.
    pthread_t cpu_thread;
    if (pthread_create(&cpu_thread, NULL, node->sw_impl, (void *) node) != 0) {
        perror("Failed to create CPU thread\n");
    }

    // Add this thread to the cpu thread list of VAM
    cpu_thread_list.push_back(cpu_thread);

    // Map the physical accel struct to the new CPU thread.
    accel->accel_id = cpu_thread_list.size() - 1;
}

void vam_worker::eval_ticket_alloc() {
    for (const auto &routine_map_pair : virt_to_phy_mapping) {
        df_int_node_t *routine = routine_map_pair.first;
        std::unordered_map<df_node_t *, std::pair<physical_accel_t *, unsigned>> mapping = routine_map_pair.second;

        virt_ticket_table[routine] = 0;

        for (const auto& mapping_pair : mapping) {
            physical_accel_t *device = mapping_pair.second.first;
            virt_ticket_table[routine] += device->get_avg_tickets();
        }

        DEBUG(printf("[VAM] Re-evaluated tickets for %s = %d\n", routine->get_name(), virt_ticket_table[routine]);)
    }
}

bool vam_worker::release_accel(hpthread_routine_t *routine) {
    if (!virt_to_phy_mapping.count(routine)) return false;

    std::unordered_map<df_node_t *, std::pair<physical_accel_t *, unsigned>> &accel_mapping = virt_to_phy_mapping[routine];

    for (const auto &node_accel_pair : accel_mapping) {
        std::pair<physical_accel_t *, unsigned> accel_context_pair = node_accel_pair.second;
        physical_accel_t *accel = accel_context_pair.first;
        unsigned context = accel_context_pair.second;

        printf("[VAM] Releasing accel %s:%d for node %s in routine %s\n", accel->get_name(), context, node_accel_pair.first->dump_prim(), routine->get_name());

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
    }

    // Delete the map entry for the routine
    virt_to_phy_mapping.erase(routine);

    return true;
}
