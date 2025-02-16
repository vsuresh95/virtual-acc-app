#include <audio-task.hpp>

void AudioTask::init_buf() {
	unsigned local_len = in_words;
	token_t *input_base = (token_t *) hw_buf + SYNC_VAR_SIZE;

	// initialize filters
	for (unsigned i = 0; i < local_len; i++) {
		input_base[i] = i % 100;
	}
}

unsigned AudioTask::validate_buf() {
	unsigned errors = 0;
	unsigned local_len = out_words;
	token_t *output_base = (token_t *) hw_buf + NUM_DEVICES*acc_len + SYNC_VAR_SIZE;

	// initialize filters
	for (unsigned i = 0; i < local_len; i++) {
		if (output_base[i] != i % 100) errors++;
	}

	return errors;
}

void AudioTask::flt_twd_init() {
	unsigned local_len = in_words;
	token_t *filter_base = (token_t *) hw_buf + (5 * acc_len);
	token_t *twiddle_base = (token_t *) hw_buf + (7 * acc_len);

	// initialize filters
	for (unsigned i = 0; i < local_len + 2; i++) {
		filter_base[i] = i % 100;
	}

	// initialize twiddles
	for (unsigned i = 0; i < local_len; i++) {
		twiddle_base[i] = i % 100;
	}
}

void AudioTask::run() {
	thread_id = thread_id;

	// configure the parameters for this audio worker
	{
		logn_samples = (rand() % 6) + 6;
		in_words = 2 * (1 << logn_samples);
		out_words = 2 * (1 << logn_samples);
		acc_len = SYNC_VAR_SIZE + in_words + out_words;
		mem_size = (NUM_DEVICES + 5) * acc_len;
	}

	// set up memory buffers for accel -- single contiguous buffer for mem_size
	hw_buf = esp_alloc (mem_size);
	
	// Set ASI sync flags offsets
	ConsRdyOffset = 0*acc_len + READY_FLAG_OFFSET;
	ConsVldOffset = 0*acc_len + VALID_FLAG_OFFSET;
	FltRdyOffset = 1*acc_len + FLT_READY_FLAG_OFFSET;
	FltVldOffset = 1*acc_len + FLT_VALID_FLAG_OFFSET;
	ProdRdyOffset = 3*acc_len + READY_FLAG_OFFSET;
	ProdVldOffset = 3*acc_len + VALID_FLAG_OFFSET;
	InputOffset = acc_len + SYNC_VAR_SIZE;
	OutputOffset = 2*acc_len + SYNC_VAR_SIZE;
	FltInputOffset = 5*acc_len;
	TwdInputOffset = 7*acc_len;

	// This task assumes it's doing the entirety of Audio FFI, therefore, it sets inverse
	// as 0. But VAM should internally set this as 1 for the IFFT accelerator.
	do_inverse = 0;

	// One-time initialization of FIR filters and twiddle factors 
	flt_twd_init();

	accel_handle = (AudioInst *) new AudioInst;	

	// Submit the request to VAM interface and proceed if the allocation was successful
    VAMcode success = ERROR;
	do {
		success = get_accel(accel_handle);
	} while (success == ERROR);

	unsigned iter_count = 0;
	uint64_t t_accel;
	unsigned errors;

	while (1) {
		printf("Thread %d starting iteration %d\n", thread_id, iter_count);

		// Wait for FFT (consumer) to be ready.
		SpinSync(ConsRdyOffset, 1);
		// Reset flag for next iteration.
		UpdateSync(ConsRdyOffset, 0);
		// Write input data for FFT.
		init_buf();
		// Inform FFT (consumer) to start.
		UpdateSync(ConsVldOffset, 1);

		start_counter();
		// Wait for IFFT (producer) to send output.
		SpinSync(ProdVldOffset, 1);
		// Reset flag for next iteration.
		UpdateSync(ProdVldOffset, 0);
		t_accel += end_counter();
		printf("Thread %d accel time = %lu\n", thread_id, t_accel);

		// Read back output from IFFT
		errors = validate_buf();
		// Inform IFFT (producer) - ready for next iteration.
		UpdateSync(ProdRdyOffset, 1);
	}
}   

VAMcode AudioTask::get_accel(AudioInst *accel_handle) {
	req_intf->accel_handle = (void *) accel_handle;

	return GenericTask::get_accel();
}

void AudioTask::UpdateSync(unsigned FlagOFfset, token_t UpdateValue) {
	volatile token_t* sync = (token_t *) hw_buf + FlagOFfset;
	
    *sync = UpdateValue;
}

void AudioTask::SpinSync(unsigned FlagOFfset, token_t SpinValue) {
	volatile token_t* sync = (token_t *) hw_buf + FlagOFfset;

	while (*sync != SpinValue);
}

bool AudioTask::TestSync(unsigned FlagOFfset, token_t TestValue) {
	volatile token_t* sync = (token_t *) hw_buf + FlagOFfset;

	if (*sync != TestValue) return false;
	else return true;
}

void AudioTask::handle_sync() {
    *accel_handle = {
        // From VirtualInst* -- generic
        thread_id,
		AUDIO_FFI,
        // From VirtualInst* -- working memory parameters
        in_words, out_words, acc_len, mem_size, hw_buf,
        // From VirtualInst* -- ASI parameters
        ConsRdyOffset, ConsVldOffset, ProdRdyOffset, ProdVldOffset,
        InputOffset, OutputOffset,

        // From AudioInst* -- workload parameters
        logn_samples, do_inverse,
        // From AudioInst* -- ASI parameters
        FltVldOffset, FltRdyOffset, FltInputOffset, TwdInputOffset
    };
}