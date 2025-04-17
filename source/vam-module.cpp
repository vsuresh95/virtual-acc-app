#include <vam-module.hpp>

void vam_worker::run() {
	printf("[VAM] Hello from vam_worker!\n");

    // populate the list of physical accelerators in the system
    probe_accel();

    while (1) {
        // Test if a task is submitted by atomically checking if the state is ONGOING
        hpthread_routine_t *routine = test_hpthread_req();

        if (routine != NULL) {
            DEBUG(printf("[VAM] Received a request for hpthread %s\n", routine->get_name());)

            // search the available accelerators if any can satisfy the request
            bool success = search_accel(routine);

            // Acknowledge the request with the success code
            ack_hpthread_req(success);

            DEBUG(printf("[VAM] Completed the processing for request.\n");)
        } else {
            // Print out the tickets for each hpthread
            for (const auto& thread_ticket_pair : virt_ticket_table) {
                printf("[VAM] Tickets allocated to %s = %d\n", thread_ticket_pair.first->get_name(), thread_ticket_pair.second);
            }

            sleep(1);
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
            accel_temp.is_allocated = false;
            std::strcpy(accel_temp.thread_id, "Unallocated");
            std::strcpy(accel_temp.devname, entry->d_name);
            accel_temp.hw_buf = NULL;

            // Print out debug message
            DEBUG(printf("[VAM] Discovered device %d: %s\n", device_id, accel_temp.devname);)

            if (fnmatch("audio_fft*", entry->d_name, FNM_NOESCAPE) == 0) {
                accel_temp.prim = AUDIO_FFT;
                accel_temp.ioctl_req = AUDIO_FFT_STRATUS_IOC_ACCESS;
                accel_temp.tickets = AUDIO_FFT_TICKETS;

                // Create a new access struct and track within the device struct
                struct audio_fft_stratus_access *audio_fft_desc = new struct audio_fft_stratus_access;
                accel_temp.esp_access_desc = audio_fft_desc;
                accel_temp.device_access_cfg = audio_fft_access_cfg;
            } else if (fnmatch("audio_fir*", entry->d_name, FNM_NOESCAPE) == 0) {
                accel_temp.prim = AUDIO_FIR;
                accel_temp.ioctl_req = AUDIO_FIR_STRATUS_IOC_ACCESS;
                accel_temp.tickets = AUDIO_FIR_TICKETS;

                // Create a new access struct and track within the device struct
                struct audio_fir_stratus_access *audio_fir_desc = new struct audio_fir_stratus_access;
                accel_temp.esp_access_desc = audio_fir_desc;
                accel_temp.device_access_cfg = audio_fir_access_cfg;
            } else if (fnmatch("audio_ffi*", entry->d_name, FNM_NOESCAPE) == 0) {
                accel_temp.prim = AUDIO_FFI;
                accel_temp.ioctl_req = AUDIO_FFI_STRATUS_IOC_ACCESS;
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

    // This variable tracks if any accelerator was allocated in this int_node
    // If not, we will allocated a coarse-grained SW function.
    bool accel_allocated = false;

    while (!q.empty()) {
        df_node_t *current = q.front();
        q.pop();

        DEBUG(printf("\tFound node %s\n", current->dump_prim()));

        // Check if there exists an accelerator for this node (if not NONE) and if yes, assign that accelerator. 
        bool success = false;

        // Ensure that the current node is an entry or exit
        if (current->get_prim() != NONE) {
            // Check if there is any hardware device that supports the task for the requested instance,
            // and are not yet allocated.
            for (unsigned id = 0; id < accel_list.size(); id++) {
                DEBUG(
                    printf("\n[VAM] Checking device %d.\n", id);
                    accel_list[id].dump();
                )

                if (accel_list[id].prim == current->get_prim() && accel_list[id].is_allocated == false) {
                    // Update the phy->virt (here virt is the root of the original task request)
                    phy_to_virt_mapping[&(accel_list[id])] = current->get_root();
                    // Update the virt->node->phy
                    virt_to_phy_mapping[current->get_root()][current] = &(accel_list[id]);

                    // Mark the device as allocated.
                    accel_list[id].is_allocated = true;
                    std::strcpy(accel_list[id].thread_id, current->get_root()->get_name());

                    printf("[VAM] Temporarily allocated device %s to node %s of routine %s!\n", virt_to_phy_mapping[current->get_root()][current]->get_name(), current->dump_prim(), current->get_root()->get_name());
                    success = true;
                    accel_allocated = true;
                    break;
                }
            }

            // If we could not find an accelerator, we check if the node is internal. If yes, 
            // there exists a sub-graph we can process. If not, we return an ERROR.
            if (!success) {
                // If the node is internal, we must traverse its child graph before going to its consumers
                if (current->is_internal()) {
                    DEBUG(printf("\tSearching the child graph of int_node %s\n", ((df_int_node_t *) current)->get_name()));

                    // If the child graph was unable to allocate any hardware accelerators, run this internal node in SW.
                    if (!search_accel((df_int_node_t *) current)) {
                        // None of the child nodes mapped to an accelerator, therefore, we will delete the CPU physical accel's
                        // created for this node, and then clear all entries in virt_to_phy_mapping for all the child nodes
                        // of current and then assign current to a single CPU physical accel alone.
                        for (auto child : ((df_int_node_t *) current)->child_graph->df_node_list) {
                            delete virt_to_phy_mapping[current->get_root()][current];
                            virt_to_phy_mapping[current->get_root()].erase(child);
                        }

                        DEBUG(printf("[VAM] Cleared all child nodes of %s of routine %s!\n", current->dump_prim(), current->get_root()->get_name());)

                        // Create a new physical accelerator for this CPU thread
                        physical_accel_t *cpu_thread = new physical_accel_t;
                        cpu_thread->prim = NONE;
                        cpu_thread->tickets = CPU_TICKETS;
                        std::strcpy(cpu_thread->devname, "CPU");

                        // Only virt to phy is updated, since multiple nodes might map to CPU
                        virt_to_phy_mapping[current->get_root()][current] = cpu_thread;

                        printf("[VAM] Temporarily allocated CPU to node %s of routine %s!\n", current->dump_prim(), current->get_root()->get_name());
                    }
                } else {
                    // Create a new physical accelerator for this CPU thread
                    physical_accel_t *cpu_thread = new physical_accel_t;
                    cpu_thread->prim = NONE;
                    cpu_thread->tickets = CPU_TICKETS;
                    std::strcpy(cpu_thread->devname, "CPU");

                    // Only virt to phy is updated, since multiple nodes might map to CPU
                    virt_to_phy_mapping[current->get_root()][current] = cpu_thread;

                    printf("[VAM] Temporarily allocated CPU to node %s of routine %s!\n", current->dump_prim(), current->get_root()->get_name());
                }
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
    if (!routine->is_root()) return accel_allocated;

    virt_ticket_table[routine] = 0;

    // Iterate through all the nodes in the virt to phy mapping to configure the accelerators
    std::unordered_map<df_node_t *, physical_accel_t *> accel_candidates = virt_to_phy_mapping[routine];
    for (const auto& node_accel_pair : accel_candidates) {
        if ((node_accel_pair.second)->prim == NONE) {
            configure_cpu(node_accel_pair.first, node_accel_pair.second);
        } else {
            configure_accel(node_accel_pair.first, node_accel_pair.second);
        }

        // Increment the tickets for this hpthread
        virt_ticket_table[routine] += node_accel_pair.second->tickets;
    }

    // If we reached this point, we successfully allocated accelerators to all tasks in the DFG.
    return true;
}

void vam_worker::configure_accel(df_node_t *node, physical_accel_t *accel) {
    DEBUG(printf("[VAM] Configuring accel %s for node %s in routine %s\n", accel->get_name(), node->dump_prim(), node->get_root()->get_name());)

    // ESP defined data type for the pointer to the memory pool for accelerators.
    contig_handle_t *handle = lookup_handle(node->get_params()->mem_pool->hw_buf, NULL);

    // Configring all the device-independent fields in the esp desc
    esp_access *generic_esp_access = (esp_access *) accel->esp_access_desc;
    {
        generic_esp_access->contig = contig_to_khandle(*handle);
        generic_esp_access->run = true;
        generic_esp_access->coherence = ACC_COH_RECALL;
        generic_esp_access->spandex_conf = 0;
        generic_esp_access->start_stop = 1;
        generic_esp_access->p2p_store = 0;
        generic_esp_access->p2p_nsrcs = 0;
        generic_esp_access->src_offset = 0;
        generic_esp_access->dst_offset = 0;
    }

    // Call function for configuring other accel-dependent fields
    accel->device_access_cfg(node, generic_esp_access);

    if (ioctl(accel->fd, accel->ioctl_req, accel->esp_access_desc)) {
        perror("ioctl");
        exit(EXIT_FAILURE);
    }
}

void vam_worker::configure_cpu(df_node_t *node, physical_accel_t *accel) {
    DEBUG(printf("[VAM] Configuring CPU for node %s in routine %s\n", node->dump_prim(), node->get_root()->get_name());)

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
