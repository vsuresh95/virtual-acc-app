#ifndef __AUDIO_FFT_DEF_H__
#define __AUDIO_FFT_DEF_H__

// Device-dependent probe function
void audio_fft_probe(physical_accel_t *accel);

// Device-dependent configuration function
void audio_fft_cfg(hpthread_t *th, esp_access *generic_esp_access);

#endif // __AUDIO_FFT_DEF_H__
