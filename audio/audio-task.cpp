#include <audio-task.hpp>

void AudioTask::init_buf() {
	unsigned local_len = in_words;
	token_t *input_base = (token_t *) mem + SYNC_VAR_SIZE;

	// initialize filters
	for (unsigned i = 0; i < local_len; i++) {
		input_base[i] = i % 100;
	}
}

unsigned AudioTask::validate_buf() {
	unsigned errors = 0;
	unsigned local_len = out_words;
	token_t *output_base = (token_t *) mem + NUM_DEVICES*acc_len + SYNC_VAR_SIZE;

	// initialize filters
	for (unsigned i = 0; i < local_len; i++) {
		if (output_base[i] != i % 100) errors++;
	}

	return errors;
}

void AudioTask::flt_twd_init() {
	unsigned local_len = in_words;
	token_t *filter_base = (token_t *) mem + (5 * acc_len);
	token_t *twiddle_base = (token_t *) mem + (7 * acc_len);

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
	// configure the parameters for this audio worker
	{
		log_len = (rand() % 6) + 6;
		in_words = 2 * (1 << log_len);
		out_words = 2 * (1 << log_len);
		acc_len = SYNC_VAR_SIZE + in_words + out_words;
		mem_size = (NUM_DEVICES + 5) * acc_len;
	}

	// set up memory buffers for accel -- single contiguous buffer for mem_size
	mem = (token_t *) esp_alloc (mem_size);

	// One-time initialization of FIR filters and twiddle factors 
	flt_twd_init();

	// Submit the request to VAM interface and proceed if the allocation was successful
    VAMcode success = ERROR;
	do {
		success = get_accel();
	} while (success == ERROR);
	
	ConsRdyFlag = 0*acc_len + READY_FLAG_OFFSET;
	ConsVldFlag = 0*acc_len + VALID_FLAG_OFFSET;
	FltRdyFlag = 1*acc_len + FLT_READY_FLAG_OFFSET;
	FltVldFlag = 1*acc_len + FLT_VALID_FLAG_OFFSET;
	ProdRdyFlag = 3*acc_len + READY_FLAG_OFFSET;
	ProdVldFlag = 3*acc_len + VALID_FLAG_OFFSET;

	unsigned iter_count = 0;
	uint64_t t_accel;
	unsigned errors;

	while (1) {
		printf("Thread %d starting iteration %d\n", thread_id, iter_count);

		// Wait for FFT (consumer) to be ready.
		SpinSync(ConsRdyFlag, 1);
		// Reset flag for next iteration.
		UpdateSync(ConsRdyFlag, 0);
		// Write input data for FFT.
		init_buf();
		// Inform FFT (consumer) to start.
		UpdateSync(ConsVldFlag, 1);

		start_counter();
		// Wait for IFFT (producer) to send output.
		SpinSync(ProdVldFlag, 1);
		// Reset flag for next iteration.
		UpdateSync(ProdVldFlag, 0);
		t_accel += end_counter();
		printf("Thread %d accel time = %lu\n", thread_id, t_accel);

		// Read back output from IFFT
		errors = validate_buf();
		// Inform IFFT (producer) - ready for next iteration.
		UpdateSync(ProdRdyFlag, 1);
	}
}   

void AudioTask::UpdateSync(unsigned FlagOFfset, token_t UpdateValue) {
	volatile token_t* sync = (token_t *) mem + FlagOFfset;
	
    *sync = UpdateValue;
}

void AudioTask::SpinSync(unsigned FlagOFfset, token_t SpinValue) {
	volatile token_t* sync = (token_t *) mem + FlagOFfset;

	while (*sync != SpinValue);
}

bool AudioTask::TestSync(unsigned FlagOFfset, token_t TestValue) {
	volatile token_t* sync = (token_t *) mem + FlagOFfset;

	if (*sync != TestValue) return false;
	else return true;
}