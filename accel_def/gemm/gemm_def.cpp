#include <hpthread.h>
#include <common_helper.h>
#include <vam_physical_accel.h>
#include <gemm_hpthread_args.h>
#include <gemm_def.h>
#include <gemm_offline_prof.h>

// Device-dependent ESP driver
#ifdef __cplusplus
extern "C" {
#endif

#include <gemm_stratus.h>

#ifdef __cplusplus
}
#endif

// Device-dependent probe function
void gemm_probe(physical_accel_t *accel) {
    accel->prim = hpthread_prim_t::GEMM;
    accel->init_ioctl = GEMM_STRATUS_INIT_IOC_ACCESS;
    accel->add_ctxt_ioctl = GEMM_STRATUS_ADD_IOC_ACCESS;
    accel->del_ctxt_ioctl = GEMM_STRATUS_DEL_IOC_ACCESS;
    accel->setprio_ioctl = GEMM_STRATUS_PRIO_IOC_ACCESS;

    // Create a new access struct and track within the device struct
    struct gemm_stratus_access *gemm_desc = new struct gemm_stratus_access;
    accel->esp_access_desc = gemm_desc;
    accel->device_cfg = gemm_cfg;
}

// Device-dependent configuration function
void gemm_cfg(hpthread_t *th, esp_access *generic_esp_access) {
    gemm_hpthread_args *args = (gemm_hpthread_args *) th->args;
    struct gemm_stratus_access *gemm_desc = (struct gemm_stratus_access *) generic_esp_access;

    // Get the parameters and memory offsets from the args of hpthread
    gemm_desc->dim_m = args->dim_m;
    gemm_desc->dim_n = args->dim_n;
    gemm_desc->dim_k = args->dim_k;
    gemm_desc->weight_base = args->weight_base;
    gemm_desc->input_base = args->input_base;
    gemm_desc->output_base = args->output_base;

    // Send the predicted load back to VAM, from the offline profiled runtime
    unsigned offline_profile_key = (args->dim_m * args->dim_n * args->dim_k)/4096;
    auto it = gemm_offline_prof.upper_bound(offline_profile_key);
    // x is larger than the largest key: return time for largest size
    // else, return value for the smallest size greater than the dimensions
    if (it == gemm_offline_prof.end()) th->assigned_load = std::prev(it)->second;
    else th->assigned_load = it->second;
}
