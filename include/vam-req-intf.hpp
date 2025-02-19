#ifndef __VAM_REQ_INTF_H__
#define __VAM_REQ_INTF_H__

#include <atomic>
#include <pthread.h>

typedef enum { SUCCESS = 0, ERROR = 1 } VAMcode;

class VAMReqIntf {

    public:
            pthread_mutex_t intf_mutex;
            pthread_cond_t req_ready;
            pthread_cond_t req_done;
            bool task_available;
            bool task_completed;
            VAMcode rsp_code;
            // App-specific virtual instance parameters
            void *accel_handle;

            VAMReqIntf() {
                intf_mutex = PTHREAD_MUTEX_INITIALIZER;
                req_ready = PTHREAD_COND_INITIALIZER;
                req_done = PTHREAD_COND_INITIALIZER; 
                task_available = false;
                task_completed = false;
            }
};

#endif // __VAM_REQ_INTF_H__