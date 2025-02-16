#ifndef __GEN_ACCEL_DEFS_H__
#define __GEN_ACCEL_DEFS_H__

#include <vector>

// Capabilities available to the application
typedef enum {
	AUDIO_FFT = 0,
	AUDIO_FIR = 1,
	AUDIO_FFI = 2
} Capability;

// Definition of a virtual instance that will be requested by
// user application. Pointer to this will be added to physical to
// virtual accelerator monitor in VAM.
typedef struct {
    unsigned thread_id;
    Capability capab;

    // Generic memory parameters (for working set of task)
    unsigned in_words;
    unsigned out_words;
    unsigned acc_len;
    unsigned mem_size;
    void *hw_buf;

    // ASI parameters
    unsigned ConsRdyOffset;
    unsigned ConsVldOffset;
    unsigned ProdRdyOffset;
    unsigned ProdVldOffset;
    unsigned InputOffset;
    unsigned OutputOffset;
} VirtualInst;

// class AccelDefs {

//             // That accelerator definition should map accelerator name to capability
//             // and other details like IOCTL code, and access info (like register names).
//             // This accelerator definition must be a generic class that is extended.
//     public:
// 	        char *devname;
// 	        int ioctl_req;
// 	        /* Partially Filled-in by ESPLIB */
// 	        struct esp_access *esp_desc;

// }

// AccelDefinitions accel_defs[3] = {
//     {
//         .devname = "audio_fft_stratus.",
//         .ioctl_req = AUDIO_FFT_STRATUS_IOC_ACCESS
//     },
// };


// struct audio_fft_stratus_access fft_cfg_001[] = {
// 	{
// 		/* <<--descriptor-->> */
// 		.logn_samples = LOG_LEN,
// 		.do_inverse = 1,
// 		.do_shift = 0,
// 		.src_offset = 0,
// 		.dst_offset = 0,
// 		.esp.coherence = ACC_COH_FULL,
// 		.esp.p2p_store = 0,
// 		.esp.p2p_nsrcs = 0,
// 		.esp.p2p_srcs = {"", "", "", ""},
// 	}
// };

// esp_thread_info_t cfg_000[] = {
// 	{
// 		.run = true,
// 		.devname = "audio_fft_stratus.0",
// 		.ioctl_req = AUDIO_FFT_STRATUS_IOC_ACCESS,
// 		.esp_desc = &(fft_cfg_000[0].esp),
// 	}
// };
#endif // __GEN_ACCEL_DEFS_H__