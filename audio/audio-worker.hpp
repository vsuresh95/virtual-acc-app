#ifndef __AUDIO_WORKER_H__
#define __AUDIO_WORKER_H__

#include <vam-hpthread.hpp>
#include <audio-helper.hpp>

#define MAX_AUDIO_THREADS 100

class audio_worker {

	public:
			// Thread name and pthread associated with this object
			char thread_name[100];
			pthread_t audio_thread;

			unsigned logn_samples;
			unsigned do_inverse;
			unsigned do_shift;

			unsigned io_payload_size;
			unsigned flt_payload_size;

			// Common memory pool buffer
    		mem_pool_t *mem_pool;

			// Two user mode queues for input and output and one for filters/twiddles
    		mem_queue_t *input_queue;
    		mem_queue_t *output_queue;
			mem_queue_t *filter_queue;

			audio_worker(const char *t) { std::strcpy(thread_name, t); }

            // Static version of run method
			static void* run(void* task_obj) {
				static_cast<audio_worker *>(task_obj)->run();
				return 0;
			}

			void init_buf();
			unsigned validate_buf();
			void flt_twd_init();

			void run();

			void create_audio_dfg(hpthread_routine_t *routine_dfg);
};

#endif /* __AUDIO_WORKER_H__ */
