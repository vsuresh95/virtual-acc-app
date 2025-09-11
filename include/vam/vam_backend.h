#ifndef __VAM_BACKEND_H__
#define __VAM_BACKEND_H__

// VAM backend is responsible for map virutal hpthreads to physical accelerators
// (or CPU threads) and tracking utilization of these mappings

#define VAM_SLEEP_MIN       2 * 78125000 // ~2 seconds
#define VAM_SLEEP_MAX       10 * 78125000 // ~10 seconds
// Default scheduling period of AVU
#define AVU_SCHED_PERIOD    0x800000
// Cooldown timer for migration
#define TH_MOVE_COOLDOWN    5 * 78125000 // ~5 seconds

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
float vam_check_load_balance();
// Runs the load balancing algorithm across all accelerators
void vam_load_balance();

#endif // __VAM_BACKEND_H__
