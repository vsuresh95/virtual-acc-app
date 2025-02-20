#include "vam-worker.hpp"
#include <vam-accel-defs.hpp>

VamWorker::VamWorker(VAMReqIntf *req_intf_param) 
    : req_intf {req_intf_param}
{
    // Allocate space for accel_list?
}

void VamWorker::run() {
	printf("[VAM] Hello from VamWorker!\n");

    // populate the list of physical accelerators in the system
    // PhysicalAccel *accel_list = (PhysicalAccel *) malloc (20 * sizeof(PhysicalAccel));
    probe_accel();

    while (1) {
        // VAM will keep monitoring the request interface to see if there are new requests 
        // TODO: for now, just monitor this. Later, we must also monitor hardware to see if we need to live migrate.)

        // Wait until a task is submitted by atomically checking if the state is ONGOING
        while (req_intf->intf_state.load() != ONGOING) {
            sched_yield();
        }

        // search the available accelerators if any can satisfy the request
        // TODO: currently, we assume only allocation requests. Support deallocation later.
        if (search_accel(req_intf->accel_handle) == SUCCESS) {
            // If yes, configure the accelerator with the parameters in the handle
            req_intf->rsp_code = SUCCESS;
            
            configure_accel(req_intf->accel_handle);
        } else {
            // If no, return ERROR
            req_intf->rsp_code = ERROR;
        }

        // Set the task as done by acquiring the lock
        req_intf->intf_state.store(DONE);

        DEBUG(printf("[VAM] Completed the processing for request.\n");)
    }
}

void VamWorker::probe_accel() {
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
            // Print out debug message
            DEBUG(printf("[VAM] Discovered device %d: %s.\n", device_id, entry->d_name);)

            PhysicalAccel accel_temp;
            accel_temp.accel_id = device_id++;
            accel_temp.is_allocated = false;
            accel_temp.thread_id = 1024;
            std::strcpy(accel_temp.devname, entry->d_name);
            accel_temp.hw_buf = NULL;

            if (fnmatch("audio_fft*", entry->d_name, FNM_NOESCAPE) == 0) {
                accel_temp.capab = AUDIO_FFT;
                accel_temp.ioctl_req = AUDIO_FFT_STRATUS_IOC_ACCESS;
            } else if (fnmatch("audio_fir*", entry->d_name, FNM_NOESCAPE) == 0) {
                accel_temp.capab = AUDIO_FIR;
                accel_temp.ioctl_req = AUDIO_FIR_STRATUS_IOC_ACCESS;
            } else if (fnmatch("audio_ffi*", entry->d_name, FNM_NOESCAPE) == 0) {
                accel_temp.capab = AUDIO_FFI;
                accel_temp.ioctl_req = AUDIO_FFI_STRATUS_IOC_ACCESS;
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

VAMcode VamWorker::search_accel(void* generic_handle) {
    VirtualInst *accel_handle = (VirtualInst *) generic_handle;

    Capability capab = accel_handle->capab;

	printf("[VAM] Received allocation request from thread %d.\n", accel_handle->thread_id);

    // Check if there is any hardware device that supports the capability for the requested instance,
    // and are not yet allocated.
    // For composable capabilities, this must be a monolithic accelerator.
    for (unsigned id = 0; id < accel_list.size(); id++) {
        DEBUG(
	        printf("\n[VAM] Checking device %d.\n", id);
            accel_list[id].print();
        )

        if (accel_list[id].capab == capab && accel_list[id].is_allocated == false) {
            // Update the phy->virt and virt->phy mapping between the two instances.
            phy_to_virt_mapping[&(accel_list[id])] = accel_handle;
            virt_to_phy_mapping[accel_handle] = &(accel_list[id]);

            // Mark the device as allocated.
            accel_list[id].is_allocated = true;
	        printf("[VAM] SUCCESS: Allocated device %d!\n", id);
            // Return successful.
            return SUCCESS;
        }
    }

    // If you do not find a device with the capability, you do two things:
    // TODO a) for composable capabilities, check for devices that can be composed together
    //         using the Capability registry. When configuring this, the application would not have specified the 
    //         intermediate the offsets or how the register configurations needs to split/duplicated.
    // TODO b) for other capabilities, check whether there are allocated devices that can 
    //         meet your capability. If yes, you need to pre-empt them (or add extra context to them).
    //         Shouldn't this also apply to composable capabilities - pre-empt or add context to monolithic accelerators?

    // Check monolithic devices
    if (CapabilityRegistry[capab].composable == true) {

    }
    
    // Check for pre-emption/adding context to already allocated accelerators

    // If you do not find any candidate, respond with error.
    printf("[VAM] ERROR: Did not find any capable device.\n");
    return ERROR;
}
