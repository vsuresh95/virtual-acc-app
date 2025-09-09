#ifndef __VAM_BACKEND_H__
#define __VAM_BACKEND_H__

// VAM backend is responsible for map virutal hpthreads to physical accelerators
// (or CPU threads) and tracking utilization of these mappings

#define VAM_SLEEP_MIN   2 * 78125000 // ~1 seconds
#define VAM_SLEEP_MAX   10 * 78125000 // ~10 seconds

// Default scheduling period of AVU
#define AVU_SCHED_PERIOD 0x800000

// Function to wake up and create a thread of VAM
void vam_wakeup();
// Main run method
void *vam_run_backend(void *arg);
// Search for accelerator candidates for the hpthread
void vam_search_accel(hpthread_t *th);
// Once accelerator candidate is identified, configure the accelerator
void vam_configure_accel(hpthread_t *th, physical_accel_t *accel, unsigned context);
// Similar as accelerator counterpart; this function launches a pthread.
void vam_configure_cpu(hpthread_t *th, physical_accel_t *accel);
// Release the accelerator allocated to the hpthread
void vam_release_accel(hpthread_t *th);
// Set a new priority for the accelerator allocated to the hpthread
void vam_setprio_accel(hpthread_t *th);
// Insert a new physical accelerator struct or CPU thread
void insert_physical_accel(physical_accel_t *accel);
void insert_cpu_thread(physical_accel_t *accel);
// Update utilization metrics for all accelerators
void vam_check_utilization();
// Checks whether the load is balanced across all acclerators
bool vam_check_load_balance();
// Runs the load balancing algorithm across all accelerators
void vam_load_balance();

// class vam_backend {
// public:
//     ////////////////////////////////////
//     // Member variables for tracking the mapping

//     // List of physical devices in the system
//     std::vector<physical_accel_t> accel_list;

//     // Mapping from physical accelerators to hpthreads
//     std::unordered_map<physical_accel_t *, std::array<hpthread_t *, MAX_CONTEXTS>> phy_to_virt_mapping;
//     std::unordered_map<hpthread_t *, std::pair<physical_accel_t *, unsigned>> virt_to_phy_mapping;

//     // List of CPU pthreads that we can launch a SW kernel from
//     std::vector<pthread_t> cpu_thread_list;

//     // Tracking utilization across epochs
//     std::vector<std::unordered_map<physical_accel_t *, std::array<std::pair<float, unsigned>, MAX_CONTEXTS>>> epoch_utilization;

//     // Current load imbalance
//     float load_imbalance;

//     ////////////////////////////////////
//     // Member functions

//     // Probe the ESP system for available physical accelerators
//     void probe_accel();

//     // Search for accelerator candidates for the hpthread
//     void search_accel(hpthread_t *th);

//     // Once accelerator candidate is identified, configure the accelerator
//     void configure_accel(hpthread_t *th, physical_accel_t *accel, unsigned context);

//     // Similar as accelerator counterpart; this function launches a pthread.
//     void configure_cpu(hpthread_t *th, physical_accel_t *accel);

//     // Release the accelerator allocated to the hpthread
//     bool release_accel(hpthread_t *th);

//     // Set a new priority for the accelerator allocated to the hpthread
//     bool setprio_accel(hpthread_t *th);

//     // Update utilization metrics for all accelerators
//     void check_utilization();

//     // Runs the load balancing algorithm across all accelerators
//     void load_balance();

//     // Checks whether the load is balanced across all acclerators
//     bool check_load_balance();

//     // Print out the utilization metrics for the previous epochs in a pretty format
//     void print_report();

//     // Utilization monitor
//     void run_util_mon();
// };

#endif // __VAM_BACKEND_H__
