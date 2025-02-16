#ifndef __VAM_HELPER_H__
#define __VAM_HELPER_H__

#include <common-helper.hpp>
#include <audio-helper.hpp>

#include <unordered_map>
#include <map>
#include <memory>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

#include <audio_fft_stratus.h>
#include <audio_fir_stratus.h>
#include <audio_ffi_stratus.h>

#ifdef __cplusplus
}
#endif

// This captures the defintion for composable capabilities
// However, this does not cover the case where composable
// primitives may have similar capability but of different
// size. Need to add another attribute later to account for
// this, e.g., GEMM of different sizes for composable GeMM.
typedef struct {
    bool composable;
    std::vector<Capability> comp_list;
} CapabilityDef;

// Physical accelerator attributes below

// Physical location of accelerator in SoC (used for optimizing
// data movement and accelerator locality)
typedef struct {
    unsigned x;
    unsigned y;
} AccelTileID;

typedef struct {
    // VAM-relevant variables
    unsigned accel_id; // ID for tracking
    Capability capab; // Capability of the accelerator
    // AccelTileID tile_id; // Physical tile ID (for optimizing data movement)
    bool is_allocated; // Is the accelerator currently allocated?
    unsigned thread_id; // If allocated, what is the thread ID it is allocated to?
    // ESP-relevant variables
    char *devname; // Name of device in file system
	int ioctl_req; // IOCTL access code
    void* hw_buf; // Buffer address -- converted to configuous
    int fd; // File descriptor of the device, when open
} PhysicalAccel;

#endif // __VAM_HELPER_H__