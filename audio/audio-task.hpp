#ifndef __AudioTask_H__
#define __AudioTask_H__

#include <audio-helper.hpp>
#include <generic-task.hpp>

class AudioTask : public GenericTask {

	public:
			unsigned log_len;
			unsigned FltRdyFlag;
			unsigned FltVldFlag;

			AudioTask(unsigned thread_id_param, VAMReqIntf *req_intf_param) : GenericTask(thread_id_param, req_intf_param) {}

            // Static version of run method
			static void* run(void* task_obj) {
				static_cast<AudioTask *>(task_obj)->run();
			}

	private:
			void init_buf();
			unsigned validate_buf();
			void flt_twd_init();

			void run();

			// Generic APIs for interfacing with ASI sync flags
			void UpdateSync(unsigned FlagOFfset, token_t value);
			void SpinSync(unsigned FlagOFfset, token_t value);
			bool TestSync(unsigned FlagOFfset, token_t value);
};

#endif /* __AudioTask_H__ */
