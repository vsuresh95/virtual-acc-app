#include <vam_backend.h>

// External instance of hpthread interface
extern hpthread_intf_t intf;

// Function to wake up VAM for the first time
void wakeup_vam() {
	printf("[VAM] Launching a new thread for VAM BACKEND!\n");

    // Create an object of VAM and launch a std::thread for VAM
    vam_backend be;
	std::thread t(&vam_backend::run_backend, &be);

    // TODO: this function is only used for small applications
    // where we do not want impose the need for multi-threading
    // and launching VAM from the application. In larger multi-
    // threaded applications, this must be done in the startup.
}

void vam_backend::run_backend() {
	printf("[VAM] Hello from VAM BACKEND!\n");

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
                break;
            }
            case vam_state_t::CREATE: {
                DEBUG(printf("[VAM] Received a request for creating hpthread %s\n", intf.th->get_name());)

                // Set the interface state to DONE
                intf.set(vam_state_t::DONE);

                DEBUG(printf("[VAM] Completed the processing for request.\n");)

                // Reset sleep counter after servicing a request
                vam_sleep = 0;
                break;
            }
            case vam_state_t::JOIN: {
                DEBUG(printf("[VAM] Received a request for joining hpthread %s\n", intf.th->get_name());)

                // Set the interface state to DONE
                intf.set(vam_state_t::DONE);

                DEBUG(printf("[VAM] Completed the processing for request.\n");)

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

    printf("\n[VAM] Performing device probe.\n");

    unsigned device_id = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (fnmatch("*_stratus.*", entry->d_name, FNM_NOESCAPE) == 0) {
            physical_accel_t accel_temp;
            accel_temp.accel_id = device_id++;
            accel_temp.valid_contexts.reset();
            for (int &id : accel_temp.thread_id)
                id = -1; // Negative thread ID is invalid
            std::strcpy(accel_temp.devname, entry->d_name);
            accel_temp.init_done = false;

            // Print out debug message
            DEBUG(printf("[VAM] Discovered device %d: %s\n", device_id, accel_temp.devname);)

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
