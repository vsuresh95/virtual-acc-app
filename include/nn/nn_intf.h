#ifndef __NN_INTF_H__
#define __NN_INTF_H__

#include <nn_module.h>

////////////////////////////////////
// State enumeration for nn request interface
namespace nn {    
    typedef enum : unsigned {
        RESET = 0,
        WAKEUP = 1,
        IDLE = 2,
        BUSY = 3,
        REGISTER = 4,
        RELEASE = 5,
        DONE = 7,
    } fe_state_t;
}

////////////////////////////////////
// nn interface definition
typedef struct {
    // Interface synchronization variable
    std::atomic<nn::fe_state_t> state;

    // nn for the request
    nn_module *m;

    // Helper function for swapping the state of the interface
    bool swap(nn::fe_state_t expected_value, nn::fe_state_t new_value);

    // Helper function for testing the state of the interface
    nn::fe_state_t test();

    // Helper function for setting the state of the interface
    void set(nn::fe_state_t s);
} nn_intf_t;

#endif // __NN_INTF_H__
