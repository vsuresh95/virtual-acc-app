#include <audio-task.hpp>

void audio_task::init_buf() {
	unsigned local_len = io_payload_size;
	token64_t *input_base = (token64_t *) ((token_t *) hw_buf_pool->hw_buf + input_queue.base + PAYLOAD_OFFSET);

	// initialize filters
	for (unsigned i = 0; i < local_len; i++) {
		input_base[i] = i % 100;
	}
}

unsigned audio_task::validate_buf() {
	unsigned errors = 0;
	unsigned local_len = io_payload_size;
	token64_t *output_base = (token64_t *) ((token_t *) hw_buf_pool->hw_buf + output_queue.base + PAYLOAD_OFFSET);

	// initialize filters
	for (unsigned i = 0; i < local_len; i++) {
		if (output_base[i] != i % 100) errors++;
	}

	return errors;
}

void audio_task::flt_twd_init() {
	unsigned local_len = flt_payload_size + (1 << logn_samples);
	token64_t *filter_twiddle_base = (token64_t *) ((token_t *) hw_buf_pool->hw_buf + filter_queue.base + PAYLOAD_OFFSET);

	// initialize filters + twiddles
	for (unsigned i = 0; i < local_len; i++) {
		filter_twiddle_base[i] = i % 100;
	}
}

void audio_task::run() {
	printf("[AUDIO thread %d] Hello from audio_task!\n", thread_id);

	// set up memory buffers for accel -- single contiguous buffer for an arbitrarily large size
	hw_buf_pool = new hw_buf_pool_t;
	DEBUG(printf("[AUDIO thread %d] Allocated hw_buf pool.\n", thread_id);)

	// configure the parameters for this audio worker
	{
		logn_samples = 8; // (rand() % 6) + 6;
		// This task assumes it's doing the entirety of Audio FFI, therefore, it sets inverse
		// as 0. But VAM should internally set this as 1 for the IFFT accelerator.
		do_inverse = 0;
		do_shift = 0;

		// for audio, input and output payload is of same size.
		io_payload_size = 2 * (1 << logn_samples);

		// Allocate input queue 
		// -- alloc function increments a counter for amount of memory used from pool
		input_queue.init(hw_buf_pool->hw_buf, hw_buf_pool->alloc(PAYLOAD_OFFSET + io_payload_size));

		// Allocate output queue
		output_queue.init(hw_buf_pool->hw_buf, hw_buf_pool->alloc(PAYLOAD_OFFSET + io_payload_size));

		// filter payload size for audio
		flt_payload_size = io_payload_size + 2;

		// Allocate output queue
		filter_queue.init(hw_buf_pool->hw_buf, hw_buf_pool->alloc(PAYLOAD_OFFSET + flt_payload_size));
	}

	// One-time initialization of FIR filters and twiddle factors 
	flt_twd_init();

	accel_handle = (audio_inst_t *) new audio_inst_t;	
	handle_sync();
	DEBUG(
		printf("\n[AUDIO thread %d] printing accel_handle:\n", thread_id);
		accel_handle->print();
	);

	// Submit the request to VAM interface and proceed if the allocation was successful
    vam_code_t success = ALLOC_ERROR;
	do {
		success = get_accel();
	} while (success == ALLOC_ERROR);

	DEBUG(printf("[AUDIO thread %d] Successfully received accel handle.\n", thread_id);)

	unsigned iter_count = 0;
	uint64_t t_accel;
	unsigned errors;

	while (1) {
		DEBUG(printf("[AUDIO thread %d] Starting iteration %d.\n", thread_id, iter_count);)

		// Wait for FFT (consumer) to be ready.
		while(input_queue.is_full());
		// Write input data for FFT.
		init_buf();
		// Inform FFT (consumer) to start.
		filter_queue.enqueue();
		input_queue.enqueue();

		start_counter();
		// Wait for IFFT (producer) to send output.
		while(!output_queue.is_full());
		t_accel = end_counter();
		DEBUG(printf("[AUDIO thread %d] Accel time = %lu.\n", thread_id, t_accel);)

		// Read back output from IFFT
		errors = validate_buf();
		// Inform IFFT (producer) - ready for next iteration.
		output_queue.dequeue();

		iter_count++;
		if (iter_count % 100 == 0) {
			printf("[AUDIO thread %d] Finished iteration %d.\n", thread_id, iter_count);
		}
	}

	printf("[AUDIO thread %d] Errors = %d.\n", thread_id, errors);
}   

vam_code_t audio_task::get_accel() {
	virtual_inst_t *virt_handle = (virtual_inst_t *) accel_handle;

	return generic_task::get_accel(virt_handle);
}

void audio_task::handle_sync() {
    *accel_handle = {
        // From virtual_inst_t* -- generic
        thread_id,
		AUDIO_FFI,
        // From virtual_inst_t* -- memory buffer
        hw_buf_pool,

        // From audio_inst_t* -- workload parameters
		logn_samples, do_inverse, do_shift,
        input_queue, output_queue, filter_queue
    };
}