#ifndef __VAM_REQ_INTF_H__
#define __VAM_REQ_INTF_H__

#include <atomic>
#include <pthread.h>

typedef enum { SUCCESS = 0, ERROR = 1 } VAMcode;

typedef enum { IDLE = 0, ONGOING = 1, DONE = 2 } ReqIntfState;

class VAMReqIntf {

    public:
            // Interface synchronization variables
            std::atomic<ReqIntfState> intf_state;

            // Result of previous task
            VAMcode rsp_code;
            
            // App-specific virtual instance parameters
            void *accel_handle;

            VAMReqIntf() {
                intf_state.store(IDLE);
            }
};

#endif // __VAM_REQ_INTF_H__