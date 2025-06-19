#include <vam_backend.h>

// External instance of hpthread interface
extern hpthread_intf_t intf;

void vam_backend::run_backend() {
	printf("[VAM] Hello from VAM BACKEND!\n");

    // populate the list of physical accelerators in the system
    // probe_accel();

    unsigned vam_sleep = 0;

    // Run loop will run forever
    while (1) {
        // Test the interface state
        vam_state_t state = intf.test();

        switch(state) {
            case VAM_IDLE: {
                // Iterate through all accelerators in the system and report the utilization
                // for (physical_accel_t &accel : accel_list) {
                //     struct avu_mon_desc mon;

                //     printf("[VAM] Util of %s: ", accel.get_name());
                //     if (ioctl(accel.fd, ESP_IOC_MON, &mon)) {
                //         perror("ioctl");
                //         exit(EXIT_FAILURE);
                //     }

                //     float total_util = 0.0;
                //     uint64_t *mon_extended = (uint64_t *) mon.util;

                //     for (int i = 0; i < MAX_CONTEXTS; i++) {
                //         if (accel.valid_contexts[i]) {
                //             // Get the utilization in the previous monitor period
                //             uint64_t elapsed_cycles = get_counter() - accel.context_start_cycles[i];
                //             uint64_t util_cycles = mon_extended[i] - accel.context_active_cycles[i];
                //             float util = (float) util_cycles/elapsed_cycles;
                //             if (util_cycles > elapsed_cycles) util = 0.0;
                //             total_util += util;
                //             printf("C%d=%0.2f%%, ", i, util * 100);
                //             // Set the cycles for the next period
                //             accel.context_start_cycles[i] = get_counter();
                //             accel.context_active_cycles[i] = mon_extended[i];
                //         }
                //     }
                    
                //     printf("total=%0.2f%%\n", total_util * 100);
                // }

                sleep(std::min((const int) vam_sleep++, 10));
                break;
            }
            case VAM_CREATE: {
                DEBUG(printf("[VAM] Received a request for creating hpthread %s\n", intf.th->get_name());)

                // Set the interface state to DONE
                intf.set(VAM_DONE);

                DEBUG(printf("[VAM] Completed the processing for request.\n");)

                // Reset sleep counter after servicing a request
                vam_sleep = 0;
                break;
            }
            case VAM_JOIN: {
                DEBUG(printf("[VAM] Received a request for joining hpthread %s\n", intf.th->get_name());)

                // Set the interface state to DONE
                intf.set(VAM_DONE);

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

