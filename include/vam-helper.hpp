#ifndef __VAM_HELPER_H__
#define __VAM_HELPER_H__

#include <common-helper.hpp>

typedef struct {
    unsigned x;
    unsigned y;
} AccelTileID;

typedef struct {
    unsigned accel_id;
    TaskID capability;
    AccelTileID tile_id;
    bool is_allocated;
    unsigned thread_id;
    esp_thread_info_t cfg_info;
} PhysicalAccel;

#endif // __VAM_HELPER_H__