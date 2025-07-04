#include <audio-worker.hpp>
#include <audio-offline-prof.hpp>

void audio_worker::init_buf() {
	unsigned local_len = 2 * (1 << logn_samples);
	token_t *input_base = (token_t *) input_queue->get_payload_base();

	// initialize filters
	for (unsigned i = 0; i < local_len; i++) {
		input_base[i] = i % 100;
	}
}

unsigned audio_worker::validate_buf() {
	unsigned errors = 0;
	unsigned local_len = 2 * (1 << logn_samples);
	token_t *output_base = (token_t *) output_queue->get_payload_base();

	// initialize filters
	for (unsigned i = 0; i < local_len; i++) {
		if (output_base[i] != i % 100) errors++;
	}

	return errors;
}

void audio_worker::flt_twd_init() {
	unsigned local_len = 3 * (1 << logn_samples) + 2;
	token_t *filter_twiddle_base = (token_t *) filter_queue->get_payload_base();

	// initialize filters + twiddles
	for (unsigned i = 0; i < local_len; i++) {
		filter_twiddle_base[i] = i % 100;
	}
}

void audio_worker::run() {
	printf("[%s] Hello from audio_worker!\n", thread_name);

	// set up memory buffers for accel -- single contiguous buffer for an arbitrarily large size
	mem_pool = new mem_pool_t;
	DEBUG(printf("[%s] Allocated mem pool %p.\n", thread_name, mem_pool->hw_buf);)

	// configure the parameters for this audio worker
	{
		// This task assumes it's doing the entirety of Audio FFI, therefore, it sets inverse
		// as 0. But VAM should internally set this as 1 for the IFFT accelerator.
		do_inverse = 0;
		do_shift = 0;

		// for audio, input and output payload is of same size.
		io_payload_size = 2 * (1 << logn_samples) * sizeof(token_t);

		// Allocate input queue
		// -- alloc function increments a counter for amount of memory used from pool
		input_queue = new mem_queue_t(mem_pool, io_payload_size);

		// Allocate output queue
		output_queue = new mem_queue_t(mem_pool, io_payload_size);

		// filter payload size for audio
		flt_payload_size = (3 * (1 << logn_samples) + 2) * sizeof(token_t);

		// Allocate output queue
		filter_queue = new mem_queue_t(mem_pool, flt_payload_size);
	}

	// One-time initialization of FIR filters and twiddle factors
	flt_twd_init();

	// Declare an hpthread for the audio task, and create the DFG
	hpthread_t audio_hpthread(thread_name);
	create_audio_dfg(audio_hpthread.routine_dfg);

	DEBUG(
		printf("\n[%s] printing hpthread DFG:\n", thread_name);
		audio_hpthread.dump();
	);

	// Create an hpthread by requesting the VAM
	while (!hpthread_create(&audio_hpthread, NULL, NULL)) {
		sleep(1);
	}

	DEBUG(printf("[%s] Successfully received hpthread.\n", thread_name);)

	unsigned in_count = 0;
	unsigned flt_count = 0;
	unsigned out_count = 0;
	time_helper t_iter;

	t_iter.start_counter();

	while (1) {
		// Wait for FIR (consumer) to be ready.
		if (filter_queue->is_full()) {
			sched_yield();
		} else {
			// Inform FIR (consumer) to start.
			filter_queue->enqueue();
			flt_count++;
		}

		// Wait for FFT (consumer) to be ready.
		if (input_queue->is_full()) {
			sched_yield();
		} else {
			DEBUG(printf("[%s] Starting iteration %d.\n", thread_name, in_count);)

			// Write input data for FFT.
			// init_buf();
			// Inform FFT (consumer) to start.
			input_queue->enqueue();
			in_count++;
		}

		// Wait for IFFT (producer) to send output
		if (output_queue->is_full()) {
			// Read back output from IFFT
			// errors = validate_buf();
			// Inform IFFT (producer) - ready for next iteration.
			output_queue->dequeue();
			t_iter.lap_counter();

			out_count++;
			if (out_count % 1000 == 0) {
				uint64_t total_cycles = t_iter.get_total();
				uint64_t iter_cycles = total_cycles/1000;
				uint64_t iter_ns = iter_cycles * 12.8;
				double ips = (double) pow(10, 9)/iter_ns;
				printf("[%s] Iter %dK, Avg time = %lu, IPS = %0.2f\n", thread_name, out_count/1000, iter_cycles, ips);
				t_iter.reset_total();
				t_iter.start_counter();
			}
		} else {
			sched_yield();
		}

		if (recv_thread_signal(thread_id) == audio_cmd_t::END) break;
	}

	unsigned max_count = std::max({in_count, flt_count, out_count});
	unsigned in_left = max_count - in_count;
	unsigned flt_left = max_count - flt_count;
	unsigned out_left = max_count - out_count;

	while (in_left != 0 || flt_left != 0 || out_left != 0) {
		if (in_left != 0) {
			if (!input_queue->is_full()) {
				input_queue->enqueue();
				in_left--;
			}
		}
		
		if (flt_left != 0) {
			if (!filter_queue->is_full()) {
				filter_queue->enqueue();
				flt_left--;
			}
		}

		if (out_left != 0) {
			if (output_queue->is_full()) {
				output_queue->dequeue();
				out_left--;
			}
		}
	}

	// Release the hpthread
	while (!hpthread_join(&audio_hpthread)) {
		sleep(1);
	}

	delete input_queue;
	delete output_queue;
	delete filter_queue;
	delete mem_pool;
}

void audio_worker::create_audio_dfg(hpthread_routine_t *routine_dfg) {
    // Create node for AUDIO_FFI
    df_int_node_t *audio_ffi = new df_int_node_t(AUDIO_FFI, routine_dfg, false, "AUDIO_FFI");
	
    // Create all edges for AUDIO_FFI -- will be added to the outer graph
    df_edge_t *ffi_input = new df_edge_t(routine_dfg->get_entry(), audio_ffi);
    df_edge_t *ffi_filters = new df_edge_t(routine_dfg->get_entry(), audio_ffi);
    df_edge_t *ffi_output = new df_edge_t(audio_ffi, routine_dfg->get_exit());

	// add nodes and edges to the outer graoh
	routine_dfg->add_df_node(audio_ffi);
	routine_dfg->add_df_edge(ffi_input);
	routine_dfg->add_df_edge(ffi_filters);
	routine_dfg->add_df_edge(ffi_output);

	// Setting the parameters for FFI
	ffi_params_t *ffi_params = new ffi_params_t;
	*ffi_params = {mem_pool, logn_samples, 0};
	audio_ffi->set_params(ffi_params);
	
	// Set the mem queues for the edges
	ffi_input->set_mem_queue(input_queue);
	ffi_filters->set_mem_queue(filter_queue);
	ffi_output->set_mem_queue(output_queue);

	// Add SW implementation for Audio FFI
	audio_ffi->set_sw_impl(audio_ffi_sw_impl);

    // Create all nodes for AUDIO_FFI graph
    df_leaf_node_t *audio_fft = new df_leaf_node_t(AUDIO_FFT, audio_ffi, false);
    df_leaf_node_t *audio_fir = new df_leaf_node_t(AUDIO_FIR, audio_ffi, false);
    df_leaf_node_t *audio_ifft = new df_leaf_node_t(AUDIO_FFT, audio_ffi, false);

	// bind AUDIO_FFI edges to internal edges -- will be added to the AUDIO_FFI graph
    df_edge_t *fft_input = new df_edge_t(audio_ffi->get_entry(), audio_fft, ffi_input);
    df_edge_t *fir_filters = new df_edge_t(audio_ffi->get_entry(), audio_fir, ffi_filters);
    df_edge_t *ifft_output = new df_edge_t(audio_ifft, audio_ffi->get_exit(), ffi_output);

    // Create internal edges
    df_edge_t *fft_fir = new df_edge_t(audio_fft, audio_fir, io_payload_size);
    df_edge_t *fir_ifft = new df_edge_t(audio_fir, audio_ifft, io_payload_size);

	// add nodes and edges to the outer graoh
	audio_ffi->add_df_node(audio_fft);
	audio_ffi->add_df_node(audio_fir);
	audio_ffi->add_df_node(audio_ifft);
	audio_ffi->add_df_edge(fft_input);
	audio_ffi->add_df_edge(fft_fir);
	audio_ffi->add_df_edge(fir_filters);
	audio_ffi->add_df_edge(fir_ifft);
	audio_ffi->add_df_edge(ifft_output);

	// Setting the parameters for FFT
	fft_params_t *fft_params = new fft_params_t;
	*fft_params = {mem_pool, logn_samples, 0, 0};
	audio_fft->set_params(fft_params);

	// Setting the parameters for FIR
	fir_params_t *fir_params = new fir_params_t;
	*fir_params = {mem_pool, logn_samples};
	audio_fir->set_params(fir_params);

	// Setting the parameters for IFFT
	fft_params_t *ifft_params = new fft_params_t;
	*ifft_params = {mem_pool, logn_samples, 1, 0};
	audio_ifft->set_params(ifft_params);

	// Add SW implementation for Audio FFT/FIR
	audio_fft->set_sw_impl(audio_fft_sw_impl);
	audio_fir->set_sw_impl(audio_fir_sw_impl);
	audio_ifft->set_sw_impl(audio_fft_sw_impl);
}


// Device-dependent configuration functions
void audio_fft_access_cfg(df_node_t *node, esp_access *generic_esp_access, unsigned valid_contexts) {
    fft_params_t *params = (fft_params_t *) node->get_params();
    struct audio_fft_stratus_access *audio_fft_desc = (struct audio_fft_stratus_access *) generic_esp_access;

    audio_fft_desc->logn_samples = params->logn_samples;
    audio_fft_desc->do_shift = params->do_shift;
    audio_fft_desc->do_inverse = params->do_inverse;

    // Get the queue base from the in/out edges of the FFT node
    audio_fft_desc->input_queue_base = node->in_edges[0]->data->base / sizeof(token_t);
    audio_fft_desc->output_queue_base = node->out_edges[0]->data->base / sizeof(token_t);

    generic_esp_access->context_quota = audio_offline_prof[AUDIO_FFT][params->logn_samples];
}

void audio_fir_access_cfg(df_node_t *node, esp_access *generic_esp_access, unsigned valid_contexts) {
    fir_params_t *params = (fir_params_t *) node->get_params();
    struct audio_fir_stratus_access *audio_fir_desc = (struct audio_fir_stratus_access *) generic_esp_access;

    audio_fir_desc->logn_samples = params->logn_samples;

    // Get the queue base from the in/out edges of the FIR node
    audio_fir_desc->input_queue_base = node->in_edges[0]->data->base / sizeof(token_t);
    audio_fir_desc->filter_queue_base = node->in_edges[1]->data->base / sizeof(token_t);
    audio_fir_desc->output_queue_base = node->out_edges[0]->data->base / sizeof(token_t);

    generic_esp_access->context_quota = audio_offline_prof[AUDIO_FIR][params->logn_samples];
}

void audio_ffi_access_cfg(df_node_t *node, esp_access *generic_esp_access, unsigned valid_contexts) {
    ffi_params_t *params = (ffi_params_t *) node->get_params();
    struct audio_ffi_stratus_access *audio_ffi_desc = (struct audio_ffi_stratus_access *) generic_esp_access;

    audio_ffi_desc->logn_samples = params->logn_samples;
    audio_ffi_desc->do_shift = params->do_shift;

    // Get the queue base from the in/out edges of the FFI node
    audio_ffi_desc->input_queue_base = node->in_edges[0]->data->base / sizeof(token_t);
    audio_ffi_desc->filter_queue_base = node->in_edges[1]->data->base / sizeof(token_t);
    audio_ffi_desc->output_queue_base = node->out_edges[0]->data->base / sizeof(token_t);

    generic_esp_access->context_quota = audio_offline_prof[AUDIO_FFI][params->logn_samples];
}
