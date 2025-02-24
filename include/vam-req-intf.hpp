#ifndef __VAM_REQ_INTF_H__
#define __VAM_REQ_INTF_H__

#include <atomic>
#include <pthread.h>

typedef enum { ALLOC_SUCCESS = 0, ALLOC_ERROR = 1 } vam_code_t;

typedef enum { IDLE = 0, ONGOING = 1, DONE = 2 } req_intf_state_t;

class vam_req_intf_t {

    public:
            // Interface synchronization variables
            std::atomic<req_intf_state_t> intf_state;

            // Result of previous task
            vam_code_t rsp_code;
            
            // App-specific virtual instance parameters
            void *accel_handle;

            vam_req_intf_t() {
                intf_state.store(IDLE);
            }
};

#endif // __VAM_REQ_INTF_H__