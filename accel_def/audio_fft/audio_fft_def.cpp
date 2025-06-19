#include <hpthread.h>
#include <common_helper.h>
#include <vam_physical_accel.h>
#include <audio_fft_hpthread_args.h>
#include <audio_fft_def.h>
#include <audio_fft_offline_prof.h>

// Device-dependent ESP driver
#ifdef __cplusplus
extern "C" {
#endif

#include <audio_fft_stratus.h>

#ifdef __cplusplus
}
#endif

// Device-dependent probe function
void audio_fft_probe(physical_accel_t *accel) {
    accel->prim = hpthread_prim_t::AUDIO_FFT;
    accel->init_ioctl = AUDIO_FFT_STRATUS_INIT_IOC_ACCESS;
    accel->add_ctxt_ioctl = AUDIO_FFT_STRATUS_ADD_IOC_ACCESS;
    accel->del_ctxt_ioctl = AUDIO_FFT_STRATUS_DEL_IOC_ACCESS;

    // Create a new access struct and track within the device struct
    struct audio_fft_stratus_access *audio_fft_desc = new struct audio_fft_stratus_access;
    accel->esp_access_desc = audio_fft_desc;
    accel->device_cfg = audio_fft_cfg;
}

// Device-dependent configuration function
void audio_fft_cfg(hpthread_t *th, esp_access *generic_esp_access, unsigned valid_contexts) {
    audio_fft_hpthread_args *args = (audio_fft_hpthread_args *) th->args;
    struct audio_fft_stratus_access *audio_fft_desc = (struct audio_fft_stratus_access *) generic_esp_access;

    audio_fft_desc->logn_samples = args->logn_samples;
    audio_fft_desc->do_shift = args->do_shift;
    audio_fft_desc->do_inverse = args->do_inverse;

    // Get the queue base from the in/out edges of the FFT node
    audio_fft_desc->input_queue_base = args->input_queue_base;
    audio_fft_desc->output_queue_base = args->output_queue_base;

    generic_esp_access->context_quota = audio_fft_offline_prof[args->logn_samples];
}
