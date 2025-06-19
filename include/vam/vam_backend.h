#ifndef __VAM_BACKEND_H__
#define __VAM_BACKEND_H__

#include <common_helper.h>
#include <hpthread_intf.h>
#include <vam_physical_accel.h>
#include <vam_accel_def.h>

////////////////////////////////////
// VAM backend is responsible for map virutal hpthreads
// to physical accelerators (or CPU threads) and tracking
// utilization of these mappings
class vam_backend {
public:
    ////////////////////////////////////
    // Member variables for tracking the mapping

    // List of physical devices in the system
    std::vector<physical_accel_t> accel_list;

    // // Mapping from physical accelerators to hpthreads
    // std::unordered_map<physical_accel_t *, std::array<hpthread_t *, MAX_CONTEXTS>> phy_to_virt_mapping;
    // std::unordered_map<hpthread_t *, std::pair<physical_accel_t *, unsigned>> virt_to_phy_mapping;

    // List of CPU pthreads that we can launch a SW kernel from
    std::vector<pthread_t> cpu_thread_list;

    ////////////////////////////////////
    // Member functions

    // Probe the ESP system for available physical accelerators
    void probe_accel();

    // // Search for accelerator candidates for each node in a DFG
    // bool search_accel(hpthread_t *th);

    // // Once accelerator candidates are identified, configure each accelerator
    // void configure_accel(hpthread_t *th, physical_accel_t *accel, unsigned context);

    // // Similar as accelerator counterpart; this function launches a pthread.
    // void configure_cpu(hpthread_t *th, physical_accel_t *accel);

    // Main run method
    void run_backend();
};

// Function to wake up (create an object of) VAM for the first time
void vam_wakeup();

#endif // __VAM_BACKEND_H__
