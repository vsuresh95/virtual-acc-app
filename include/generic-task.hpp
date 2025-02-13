#ifndef __GENERIC_TASK_H__
#define __GENERIC_TASK_H__

#include <vam-req-intf.hpp>
#include <common-helper.hpp>

class GenericTask {
    
    public:
			// Thread ID of the worker that will use this object
            unsigned thread_id;

			// Generic accelerator parameters
            unsigned in_words;
            unsigned out_words;
            unsigned acc_len;
            unsigned mem_size;
            void *mem;

			// Offsets for synchronization flags
			unsigned ConsRdyFlag;
			unsigned ConsVldFlag;
			unsigned ProdRdyFlag;
			unsigned ProdVldFlag;

			// VAM interface for requesting accel allocations
			VAMReqIntf *req_intf;

			GenericTask(unsigned , VAMReqIntf *);

			// Allows worker threads to request accelerators from VAM
			VAMcode get_accel();

			// Timer functions
			uint64_t t_start;
			uint64_t t_end;

			void start_counter();
			uint64_t end_counter();
};

#endif // __GENERIC_TASK_H__ 