#ifndef __AUDIO_FFT_DEVICE_CFG_H__
#define __AUDIO_FFT_DEVICE_CFG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <audio_fft_stratus.h>

// Device-dependent configuration function
void audio_fft_access_cfg(df_node_t *node, esp_access *generic_esp_access, unsigned valid_contexts);

#ifdef __cplusplus
}
#endif

#endif // __AUDIO_FFT_DEVICE_CFG_H__
