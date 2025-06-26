#ifndef __HPTHREAD_INTF_H__
#define __HPTHREAD_INTF_H__

#include <hpthread.h>

////////////////////////////////////
// State enumeration for hpthread request interface
enum class vam_state_t {
    RESET = 0,
    WAKEUP = 1,
    IDLE = 2,
    BUSY = 3,
    CREATE = 4,
    JOIN = 5,
    SETPRIO = 6,
    DONE = 7,
    REPORT = 8
};

////////////////////////////////////
// hpthread interface definition
typedef struct {
    // Interface synchronization variable
    std::atomic<vam_state_t> state;

    // hpthread for the request
    hpthread_t *th;

    // Helper function for swapping the state of the interface
    bool swap(vam_state_t expected_value, vam_state_t new_value);

    // Helper function for testing the state of the interface
    vam_state_t test();

    // Helper function for setting the state of the interface
    void set(vam_state_t s);
} hpthread_intf_t;

#endif // __HPTHREAD_H__
