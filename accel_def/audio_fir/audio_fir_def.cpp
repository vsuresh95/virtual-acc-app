#include <hpthread.h>
#include <common_helper.h>
#include <vam_physical_accel.h>
#include <audio_fir_hpthread_args.h>
#include <audio_fir_def.h>
#include <audio_fir_offline_prof.h>

// Device-dependent ESP driver
#ifdef __cplusplus
extern "C" {
#endif

#include <audio_fir_stratus.h>

#ifdef __cplusplus
}
#endif

// Device-dependent probe function
void audio_fir_probe(physical_accel_t *accel) {
    accel->prim = hpthread_prim_t::AUDIO_FIR;
    accel->init_ioctl = AUDIO_FIR_STRATUS_INIT_IOC_ACCESS;
    accel->add_ctxt_ioctl = AUDIO_FIR_STRATUS_ADD_IOC_ACCESS;
    accel->del_ctxt_ioctl = AUDIO_FIR_STRATUS_DEL_IOC_ACCESS;
    accel->setprio_ioctl = AUDIO_FIR_STRATUS_PRIO_IOC_ACCESS;

    // Create a new access struct and track within the device struct
    struct audio_fir_stratus_access *audio_fir_desc = new struct audio_fir_stratus_access;
    accel->esp_access_desc = audio_fir_desc;
    accel->device_cfg = audio_fir_cfg;
}

// Device-dependent configuration function
void audio_fir_cfg(hpthread_t *th, esp_access *generic_esp_access) {
    audio_fir_hpthread_args *args = (audio_fir_hpthread_args *) th->args;
    struct audio_fir_stratus_access *audio_fir_desc = (struct audio_fir_stratus_access *) generic_esp_access;

    // Get the parameters and memory offsets from the args of hpthread
    audio_fir_desc->logn_samples = args->logn_samples;
    audio_fir_desc->input_queue_base = args->input_queue_base;
    audio_fir_desc->output_queue_base = args->output_queue_base;
    audio_fir_desc->filter_queue_base = args->filter_queue_base;

    // Send the predicted load back to VAM, from the offline profiled runtime
    th->assigned_load = audio_fir_offline_prof[args->logn_samples];
}
