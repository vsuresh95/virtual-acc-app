#include <stdlib.h>
#include <hpthread.h>
#include <common_helper.h>
#include <vam_physical_accel.h>
#include <gemm_params.h>
#include <gemm_def.h>
#include <gemm_stratus.h>

// Device-dependent probe function
void gemm_probe(physical_accel_t *accel) {
    accel->prim = PRIM_GEMM;
    accel->reset_ioctl = GEMM_STRATUS_RESET_IOC_ACCESS;
    accel->init_ioctl = GEMM_STRATUS_INIT_IOC_ACCESS;
    accel->add_ctxt_ioctl = GEMM_STRATUS_ADD_IOC_ACCESS;
    accel->del_ctxt_ioctl = GEMM_STRATUS_DEL_IOC_ACCESS;
    accel->setprio_ioctl = GEMM_STRATUS_PRIO_IOC_ACCESS;

    // Create a new access struct and track within the device struct
    struct gemm_stratus_access *gemm_desc = (struct gemm_stratus_access *) malloc (sizeof(struct gemm_stratus_access));
    accel->esp_access_desc = (struct esp_access *) gemm_desc;
}
