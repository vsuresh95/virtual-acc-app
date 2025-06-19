#ifndef __HPTHREAD_INTF_H__
#define __HPTHREAD_INTF_H__

#include <hpthread.h>

typedef enum { 
    VAM_RESET = 0,
    VAM_WAKEUP = 1,
    VAM_IDLE = 2,
    VAM_BUSY = 3,
    VAM_CREATE = 4,
    VAM_JOIN = 5,
    VAM_DONE = 6
} vam_state_t;

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
