#ifndef __VAM_HELPER_H__
#define __VAM_HELPER_H__

#include <common-helper.hpp>

#include <unordered_map>
#include <map>
#include <memory>
#include <vector>

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
    unsigned accel_id;
    Capability capab;
    AccelTileID tile_id;
    bool is_allocated;
    esp_thread_info_t cfg_info;
} PhysicalAccel;

typedef struct {
    char *devname;
	int ioctl_req;
    struct esp_access *esp_desc;
} AccelDefinitions;

#endif // __VAM_HELPER_H__