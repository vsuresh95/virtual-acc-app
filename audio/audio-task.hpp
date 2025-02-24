#ifndef __audio_task_H__
#define __audio_task_H__

#include <generic-task.hpp>
#include <audio-helper.hpp>

#define NUM_AUDIO_THREADS 2
#define NUM_DEVICES 3

class audio_task : public generic_task {

	public:
			unsigned logn_samples;
			unsigned do_inverse;
			unsigned do_shift;

			unsigned io_payload_size;
			unsigned flt_payload_size;

			// Two user mode queues for input and output and one for filters/twiddles
    		user_queue_t<token_t> input_queue;
    		user_queue_t<token_t> output_queue;
			user_queue_t<token_t> filter_queue;

			// Handle to store all the parameters and configuration
			audio_inst_t *accel_handle;

			audio_task(unsigned thread_id_param, vam_req_intf_t *req_intf_param) : generic_task(thread_id_param, req_intf_param) {}

            // Static version of run method
			static void* run(void* task_obj) {
				static_cast<audio_task *>(task_obj)->run();
				return 0;
			}

	private:
			void init_buf();
			unsigned validate_buf();
			void flt_twd_init();

			void run();

			// Allows worker threads to request accelerators from VAM
			vam_code_t get_accel();

			// Synchronize all parameters from task instance to virtual instance.
			// Here, all flags are the same name so we can directly assign to the struct.
			void handle_sync();
};

#endif /* __audio_task_H__ */
