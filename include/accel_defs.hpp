#ifndef __MULTIPROC_ACCEL_DEFS_H__
#define __MULTIPROC_ACCEL_DEFS_H__

#include "multiproc_ffi_helper.h"

struct audio_fft_stratus_access fft_desc = {
	{
		/* <<--descriptor-->> */
		.logn_samples = LOG_LEN,
		.do_inverse = 0,
		.do_shift = 0,
		.src_offset = 0,
		.dst_offset = 0,
		.esp.coherence = ACC_COH_FULL,
		.esp.p2p_store = 0,
		.esp.p2p_nsrcs = 0,
		.esp.p2p_srcs = {"", "", "", ""},
	}
};

typedef struct {
    char *devname;
	int ioctl_req;
    struct esp_access *esp_desc;
} AccelDefinitions;

AccelDefinitions accel_defs[3] = {
    {
        .devname = "audio_fft_stratus.",
        .ioctl_req = AUDIO_FFT_STRATUS_IOC_ACCESS
    },
};



struct audio_fft_stratus_access fft_cfg_000[] = {
	{
		/* <<--descriptor-->> */
		.logn_samples = LOG_LEN,
		.do_inverse = 0,
		.do_shift = 0,
		.src_offset = 0,
		.dst_offset = 0,
		.esp.coherence = ACC_COH_FULL,
		.esp.p2p_store = 0,
		.esp.p2p_nsrcs = 0,
		.esp.p2p_srcs = {"", "", "", ""},
	}
};

struct audio_fft_stratus_access fft_cfg_001[] = {
	{
		/* <<--descriptor-->> */
		.logn_samples = LOG_LEN,
		.do_inverse = 1,
		.do_shift = 0,
		.src_offset = 0,
		.dst_offset = 0,
		.esp.coherence = ACC_COH_FULL,
		.esp.p2p_store = 0,
		.esp.p2p_nsrcs = 0,
		.esp.p2p_srcs = {"", "", "", ""},
	}
};

struct audio_fir_stratus_access fir_cfg_000[] = {
	{
		/* <<--descriptor-->> */
		.logn_samples = LOG_LEN,
		.do_inverse = 0,
		.do_shift = 0,
		.src_offset = 0,
		.dst_offset = 0,
		.esp.coherence = ACC_COH_FULL,
		.esp.p2p_store = 0,
		.esp.p2p_nsrcs = 0,
		.esp.p2p_srcs = {"", "", "", ""},
	}
};

esp_thread_info_t cfg_000[] = {
	{
		.run = true,
		.devname = "audio_fft_stratus.0",
		.ioctl_req = AUDIO_FFT_STRATUS_IOC_ACCESS,
		.esp_desc = &(fft_cfg_000[0].esp),
	}
};

esp_thread_info_t cfg_001[] = {
	{
		.run = true,
		.devname = "audio_fft_stratus.1",
		.ioctl_req = AUDIO_FFT_STRATUS_IOC_ACCESS,
		.esp_desc = &(fft_cfg_001[0].esp),
	}
};

esp_thread_info_t cfg_002[] = {
	{
		.run = true,
		.devname = "audio_fir_stratus.0",
		.ioctl_req = AUDIO_FIR_STRATUS_IOC_ACCESS,
		.esp_desc = &(fir_cfg_000[0].esp),
	}
};

#endif // __MULTIPROC_ACCEL_DEFS_H__