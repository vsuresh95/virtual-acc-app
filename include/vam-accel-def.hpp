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
    std::bitset<MAX_CONTEXTS> valid_contexts; // Is the context currently allocated?
    char thread_id[MAX_CONTEXTS][100]; // If allocated, what is the thread ID of the context?
    unsigned tickets;
    // ESP-relevant variables
    char devname[384]; // Name of device in file system
	int init_ioctl; // IOCTL access code for initialization
	int add_ctxt_ioctl; // IOCTL access code for adding context
	int del_ctxt_ioctl; // IOCTL access code for deleting context
    int fd; // File descriptor of the device, when open
    // Variables used for configuration and context
    void *esp_access_desc; // Generic pointer to the access struct.
	void (*device_access_cfg)(df_node_t *, esp_access *, unsigned); // configure device-dependent fields
    bool init_done; // Flag to identify whether the device was initialized in the past

    void dump() {
        printf("\taccel_id = %d\n", accel_id);
        printf("\tprimitive = %s\n", df_node_t::dump_prim(prim));
        printf("\tis_allocated = 0x%lu\n", valid_contexts.to_ulong());
        printf("\tthread_id = ");
        for (char *id : thread_id)
            printf("%s ", id);
        printf("\n");
        printf("\tdevname = %s\n", devname);
        printf("\tavail_tickets = %d\n", get_avail_tickets());
    }

    char *get_name() { return devname; }

    unsigned get_avail_tickets() {
        return (valid_contexts.all() ? 0 : (tickets/(valid_contexts.count()+1)));
    }

    unsigned get_avg_tickets() {
        return (valid_contexts.none() ? tickets : (tickets/valid_contexts.count()));
    }
} physical_accel_t;

#endif // __VAM_ACCEL_DEF_H__