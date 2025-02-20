#ifndef __AudioTask_H__
#define __AudioTask_H__

#include <generic-task.hpp>
#include <audio-helper.hpp>

#define NUM_AUDIO_THREADS 2
#define NUM_DEVICES 3

typedef unsigned token_t;
typedef uint64_t token64_t;

class AudioTask : public GenericTask {

	public:
			unsigned logn_samples;
			unsigned do_inverse;
			unsigned do_shift;
			unsigned FltRdyOffset;
			unsigned FltVldOffset;
			unsigned FltInputOffset;
			unsigned TwdInputOffset;

			// Handle to store all the parameters and configuration
			AudioInst *accel_handle;

			AudioTask(unsigned thread_id_param, VAMReqIntf *req_intf_param) : GenericTask(thread_id_param, req_intf_param) {}

            // Static version of run method
			static void* run(void* task_obj) {
				static_cast<AudioTask *>(task_obj)->run();
				return 0;
			}

	private:
			void init_buf();
			unsigned validate_buf();
			void flt_twd_init();

			void run();

			// Allows worker threads to request accelerators from VAM
			VAMcode get_accel();

			// Generic APIs for interfacing with ASI sync flags
			void UpdateSync(unsigned FlagOFfset, token_t value);
			void SpinSync(unsigned FlagOFfset, token_t value);
			bool TestSync(unsigned FlagOFfset, token_t value);

			// Synchronize all parameters from task instance to virtual instance.
			// Here, all flags are the same name so we can directly assign to the struct.
			void handle_sync();

			void print_asi_flags() {
				printf("ConsVldOffset = %d\n", *((token_t *) hw_buf + ConsVldOffset));
				printf("ConsRdyOffset = %d\n", *((token_t *) hw_buf + ConsRdyOffset));
				printf("ProdVldOffset = %d\n", *((token_t *) hw_buf + ProdVldOffset));
				printf("ProdRdyOffset = %d\n", *((token_t *) hw_buf + ProdRdyOffset));
				printf("FltVldOffset = %d\n", *((token_t *) hw_buf + FltVldOffset));
				printf("FltRdyOffset = %d\n", *((token_t *) hw_buf + FltRdyOffset));
			}
};

#endif /* __AudioTask_H__ */
