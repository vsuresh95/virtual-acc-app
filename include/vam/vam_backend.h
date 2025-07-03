#ifndef __VAM_BACKEND_H__
#define __VAM_BACKEND_H__

#include <common_helper.h>
#include <hpthread_intf.h>
#include <vam_physical_accel.h>
#include <vam_accel_def.h>

#define VAM_SLEEP_MIN   78125000 // ~1 seconds
#define VAM_SLEEP_MAX   781250000 // ~10 seconds

// Approximate scaling factor for total scheduling period of AVU
#define NUM_SCHED_INTERVALS 20

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

    // Mapping from physical accelerators to hpthreads
    std::unordered_map<physical_accel_t *, std::array<hpthread_t *, MAX_CONTEXTS>> phy_to_virt_mapping;
    std::unordered_map<hpthread_t *, std::pair<physical_accel_t *, unsigned>> virt_to_phy_mapping;

    // List of CPU pthreads that we can launch a SW kernel from
    std::vector<pthread_t> cpu_thread_list;

    // Tracking utilization across epochs
    std::vector<std::unordered_map<physical_accel_t *, std::array<float, MAX_CONTEXTS>>> epoch_utilization;

    ////////////////////////////////////
    // Member functions

    // Probe the ESP system for available physical accelerators
    void probe_accel();

    // Search for accelerator candidates for the hpthread
    void search_accel(hpthread_t *th);

    // Once accelerator candidate is identified, configure the accelerator
    void configure_accel(hpthread_t *th, physical_accel_t *accel, unsigned context);

    // Similar as accelerator counterpart; this function launches a pthread.
    void configure_cpu(hpthread_t *th, physical_accel_t *accel);

    // Release the accelerator allocated to the hpthread
    bool release_accel(hpthread_t *th);

    // Set a new priority for the accelerator allocated to the hpthread
    bool setprio_accel(hpthread_t *th);

    // Update utilization metrics for all accelerators
    void check_utilization();

    // Runs the load balancing algorithm across all accelerators
    void load_balance();

    // Checks whether the load is balanced across all acclerators
    bool check_load_balance();

    // Print out the utilization metrics for the previous epochs in a pretty format
    void print_report();

    // Utilization monitor
    void run_util_mon();

    // Main run method
    void run_backend();
};

// Function to wake up (create an object of) VAM for the first time
void wakeup_vam();

#endif // __VAM_BACKEND_H__
