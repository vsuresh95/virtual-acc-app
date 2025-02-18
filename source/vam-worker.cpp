#include "vam-worker.hpp"
#include <vam-accel-defs.hpp>

VamWorker::VamWorker(VAMReqIntf *req_intf_param) 
    : req_intf {req_intf_param}
{
    // Allocate space for accel_list?
}

void VamWorker::run() {
    // populate the list of physical accelerators in the system
    // PhysicalAccel *accel_list = (PhysicalAccel *) malloc (20 * sizeof(PhysicalAccel));
    probe_accel();

    while (1) {
        // VAM will keep monitoring the request interface to see if there are new requests 
        // TODO: for now, just monitor this. Later, we must also monitor hardware to see if we need to live migrate.)

        // Check if the req is non-empty
        while(std::atomic_flag_test_and_set(&(req_intf->req_empty)) != false);

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

        // clear rsp_empty to convey allocation is done
        std::atomic_flag_clear(&(req_intf->rsp_empty));
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

    unsigned device_id = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (fnmatch("*_stratus.*", entry->d_name, FNM_NOESCAPE) == 0) {
            // Print out debug message
            printf("Discovered %s\n", entry->d_name);

            PhysicalAccel accel_temp;
            accel_temp.accel_id = device_id++;
            accel_temp.is_allocated = false;
            accel_temp.thread_id = 1024;
            accel_temp.devname = entry->d_name;
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

            accel_list.push_back (accel_temp);
        }
    }

    closedir(dir);
}

VAMcode VamWorker::search_accel(void* generic_handle) {
    VirtualInst *accel_handle = (VirtualInst *) generic_handle;

    Capability capab = accel_handle->capab;

    if (CapabilityRegistry[capab].composable == true) {
        return ERROR;
    } else {
        unsigned id = 0;
        while(&(accel_list[id]) != NULL) {
            // Iterate through all hardware devices and check if the capability of the hardware device and
            // requested instance match, and that the device is not allocated.
            if (accel_list[id].capab == capab && accel_list[id].is_allocated == false) {
                // Update the phy->virt and virt->phy mapping between the two instances.
                phy_to_virt_mapping[&(accel_list[id])] = accel_handle;
                virt_to_phy_mapping[accel_handle] = &(accel_list[id]);

                // Mark the device as allocated.
                accel_list[id].is_allocated = true;
                // Return successful.
                return SUCCESS;
            }
            id++;
        }

        return ERROR;
    }
}
