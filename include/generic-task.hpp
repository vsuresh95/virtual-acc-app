#ifndef __GENERIC_TASK_H__
#define __GENERIC_TASK_H__

#include <vam-req-intf.hpp>
#include <common-helper.hpp>

class generic_task {
    
    public:
			// Thread ID of the worker that will use this object
            unsigned thread_id;

			// Common memory pool buffer
    		hw_buf_pool_t *hw_buf_pool;

			// VAM interface for requesting accel allocations
			vam_req_intf_t *req_intf;

			generic_task(unsigned , vam_req_intf_t *);

			// Allows worker threads to request accelerators from VAM
			vam_code_t get_accel(virtual_inst_t *virt_handle);

			// Timer functions
			uint64_t t_start;
			uint64_t t_end;

			void start_counter();
			uint64_t end_counter();
};

#endif // __GENERIC_TASK_H__ 