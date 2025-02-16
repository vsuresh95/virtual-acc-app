#include "vam-worker.hpp"

VamWorker::VamWorker(VAMReqIntf *req_intf_param) 
    : req_intf {req_intf_param}
{
    // Allocate space for accel_list?
}

void VamWorker::run() {
    // populate the list of physical accelerators in the system
    // PhysicalAccel *accel_list = (PhysicalAccel *) malloc (20 * sizeof(PhysicalAccel));
    probe_accel();

    // probe the available accels using *stratus*
    // create a physical-virtual accelerator mapping (use some sort of map)
    // create some mechanism for ffi workers to enqueue accel requests to the vam worker (circular request queue?)
    // VAM will keep monitoring this circular request queue to see if there are new requests (for now, just monitor this. Later, we must also monitor hardware to see if we need to live migrate.)
    // worker must send the memory base address and parameters for the workloads (sizes, etc. that will be configured in the registers)
    // VAM will need to start the accelerator, use the memory base address to initialize the sync. flags and start the accels (what should we configure the accelerators with here?)
    // VAM should return a data structure with all the required values (mainly, SM offsets in this case)
    // create a mechanism for ffi workers to issue de-allocation of virtual accel. Here, vam must also manipulate the map.
}

void VamWorker::put_accel(VAMReqIntf *vam_intf) {
	// Check if the req is non-empty
	while(std::atomic_flag_test_and_set(&(vam_intf->req_empty)) != false);

	// register request
	vam_intf->rsp_code = SUCCESS;

	// clear rsp_empty to convey allocation is done
	std::atomic_flag_clear(&(vam_intf->rsp_empty));
}

void VamWorker::probe_accel() {
    // Open the devices directory to search for accels
    DIR *dir = opendir("/dev");
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

            auto device = std::make_unique<PhysicalAccel>();
            device->accel_id = device_id++;
            device->is_allocated = false;

            // This must check a registry of accelerator definitions stored elsewhere
            // That accelerator definition should map accelerator name to capability
            // and other details like IOCTL code, and access info (like register names).
            // This accelerator definition must be a generic class that is extended.

            PhysicalAccel* rawPtr = device.get();
            accel_list[device_id] = std::move(device);
        }
    }

    closedir(dir);
}