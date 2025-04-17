#ifndef __VAM_ACCEL_DEF_H__
#define __VAM_ACCEL_DEF_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

// Physical accelerator attributes below
#define CPU_TICKETS          1
#define AUDIO_FFT_TICKETS    3
#define AUDIO_FIR_TICKETS    5
#define AUDIO_FFI_TICKETS    15

// Physical location of accelerator in SoC (used for optimizing
// data movement and accelerator locality)
typedef struct {
    unsigned x;
    unsigned y;
} AccelTileID;

typedef struct {
    // VAM-relevant variables
    unsigned accel_id; // ID for tracking
    primitive_t prim; // operation of the accelerator
    // AccelTileID tile_id; // Physical tile ID (for optimizing data movement)
    bool is_allocated; // Is the accelerator currently allocated?
    char thread_id[100]; // If allocated, what is the thread ID it is allocated to?
    unsigned tickets;
    // ESP-relevant variables
    char devname[384]; // Name of device in file system
	int ioctl_req; // IOCTL access code
    void* hw_buf; // Buffer address -- converted to configuous
    int fd; // File descriptor of the device, when open
    // Variables used for configuraption and context
    void *esp_access_desc; // Generic pointer to the access struct.
	void (*device_access_cfg)(df_node_t *, esp_access *); // configure device-dependent fields

    void dump() {
        printf("\taccel_id = %d\n", accel_id);
        printf("\tprimitive = %s\n", df_node_t::dump_prim(prim));
        printf("\tis_allocated = %s\n", (is_allocated ? "TRUE" : "FALSE"));
        printf("\tthread_id = %s\n", thread_id);
        printf("\tdevname = %s\n", devname);
        printf("\thw_buf = %p\n", hw_buf);
    }

    char *get_name() { return devname; }
} physical_accel_t;

#endif // __VAM_ACCEL_DEF_H__