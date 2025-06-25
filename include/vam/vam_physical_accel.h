#ifndef __VAM_PHYSICAL_ACCEL_H__
#define __VAM_PHYSICAL_ACCEL_H__

////////////////////////////////////
// Definition of the physical accelerator struct
// -- this includes all information and current status
// of the physical accelerators in the system.
// The same struct is reused to track CPU threads as well.
typedef struct {
    ////////////////////////////////////
    // VAM-relevant variables

    unsigned accel_id; // ID for tracking
    hpthread_prim_t prim; // operation of the accelerator
    std::bitset<MAX_CONTEXTS> valid_contexts; // Is the context currently allocated?
    uint64_t context_start_cycles[MAX_CONTEXTS]; // Start counter for the context to use for utilization
    uint64_t context_active_cycles[MAX_CONTEXTS]; // Active cycles for the context to use for utilization
    int thread_id[MAX_CONTEXTS]; // If allocated, what is the hpthread ID of the context?
    unsigned context_load[MAX_CONTEXTS]; // Assigned (predicted) load for each context
    float context_util[MAX_CONTEXTS]; // Actual utilization of the accele

    ////////////////////////////////////
    // ESP-relevant variables

    char devname[384]; // Name of device in file system
	int init_ioctl; // IOCTL access code for initialization
	int add_ctxt_ioctl; // IOCTL access code for adding context
	int del_ctxt_ioctl; // IOCTL access code for deleting context
    int fd; // File descriptor of the device, when open

    ////////////////////////////////////
    // Variables used for configuration and context

    void *esp_access_desc; // Generic pointer to the access struct.
	void (*device_cfg)(hpthread_t *, esp_access *, unsigned); // configure device-dependent fields
    bool init_done; // Flag to identify whether the device was initialized in the past

    ////////////////////////////////////
    // Helper functions for internal use

    // Print function
    void dump() {
        printf("\t- accel_id = %d\n", accel_id);
        printf("\t- primitive = %s\n", get_prim_name(prim));
        printf("\t- is_allocated = 0x%lu\n", valid_contexts.to_ulong());
        printf("\t- thread_id = ");
        for (int &id : thread_id)
            printf("%d ", id);
        printf("\n");
        printf("\t- devname = %s\n", devname);
        printf("\t- context_load = ");
        for (unsigned &l : context_load)
            printf("%d ", l);
        printf("\n");        
        printf("\t- total_load = %d\n", get_total_load());
    }

    // Get name of the device
    char *get_name() { return devname; }

    // Get the total current assigned load for this device
    unsigned get_total_load() const {
        unsigned total_load = 0;
        for (int i = 0; i < MAX_CONTEXTS; i++) {
            if (valid_contexts[i]) {
                total_load += (unsigned) (context_load[i] * context_util[i]);
            }
        }
        return total_load;
    }
} physical_accel_t;

#endif // __VAM_PHYSICAL_ACCEL_H__
