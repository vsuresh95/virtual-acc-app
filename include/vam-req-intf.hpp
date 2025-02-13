#ifndef __VAM_REQ_INTF_H__
#define __VAM_REQ_INTF_H__

#include <atomic>
#include <pthread.h>

typedef enum { SUCCESS = 0, ERROR = 1 } VAMcode;

class VAMReqIntf {

    public:
            pthread_mutex_t intf_mutex;
            volatile std::atomic_flag req_empty;
            volatile std::atomic_flag rsp_empty;
            VAMcode rsp_code;

            VAMReqIntf() {};
};

#endif // __VAM_REQ_INTF_H__