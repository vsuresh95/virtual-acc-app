#ifndef __HPTHREAD_INTF_H__
#define __HPTHREAD_INTF_H__

#include <hpthread.h>

// State enumeration for hpthread request interface
#define VAM_RESET 0
#define VAM_WAKEUP 1
#define VAM_IDLE 2
#define VAM_BUSY 3
#define VAM_DONE 4
#define VAM_CREATE 5
#define VAM_JOIN 6
#define VAM_SETPRIO 7
#define VAM_REPORT 8

// hpthread interface definition
typedef struct {
    volatile uint8_t state; // Interface synchronization variable
    hpthread_t *th; // hpthread for the request
} hpthread_intf_t;

// Helper function for swapping the state of the interface
bool hpthread_intf_swap(uint8_t expected_value, uint8_t new_value);
// Helper function for testing the state of the interface
uint8_t hpthread_intf_test();
// Helper function for setting the state of the interface
void hpthread_intf_set(uint8_t set_value);

#endif // __HPTHREAD_INTF_H__
