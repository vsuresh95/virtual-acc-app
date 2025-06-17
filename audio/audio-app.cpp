#include <audio-worker.hpp>
#include <vam-module.hpp>

typedef enum {
	ADD_THREAD = 0,
	DEL_THREAD = 1,
	WAIT = 2,
	TERMINATE = 3,
	NOOP = 4,
} trace_cmd_t;

typedef struct {
	unsigned thread_id;
	unsigned logn_samples;
	unsigned delay;
} cmd_opts_t;

trace_cmd_t read_next_trace(cmd_opts_t *cmd_opts);

FILE *input_trace = NULL;

// Interface for communicating between main thread and worker threads
audio_thread_intf_t audio_intf;

int main(int argc, char **argv) {
	printf("\n-------------------------\n");
	printf("Starting Audio App\n");
	printf("---------------------------\n");

	// Create a new request interface object (that will be passed to the workers)
	hpthread_intf_t *req_intf = new hpthread_intf_t;

	// Create a VAM object
	vam_worker *vam = (vam_worker *) new vam_worker(req_intf);

	// Launching a VAM thread
	pthread_t vam_thread;
	if (pthread_create(&vam_thread, NULL, vam->run, (void *) vam) != 0) {
        perror("Failed to create VAM thread");
        return 1;
    }

	// Declare a list of audio tasks
	audio_worker **audio = new audio_worker*[MAX_AUDIO_THREADS];

	// Initialize the audio thread command interface
	audio_intf.valid_cmd.store(false, std::memory_order_seq_cst);

	char *trace_file;
    if (argc > 1) {
		trace_file = argv[1];
	} else {
		trace_file = (char *) "trace.txt";
	}

	input_trace = fopen(trace_file, "r");
	if (!input_trace)
	{
		perror("Failed to read trace file");
		return 1;
	}

	unsigned cur_thread_id = 0;
	cmd_opts_t cmd_opts;

	while (true) {
		switch (read_next_trace(&cmd_opts)) {
			case ADD_THREAD:
				char name[100];
				sprintf(name, "AUDIO %d", cur_thread_id);
				audio[cur_thread_id] = (audio_worker *) new audio_worker(name);
				audio[cur_thread_id]->logn_samples = cmd_opts.logn_samples;
				audio[cur_thread_id]->thread_id = cmd_opts.thread_id;

				if (pthread_create(&(audio[cur_thread_id]->audio_thread), NULL, audio[cur_thread_id]->run, (void *) (audio[cur_thread_id])) != 0) {
					perror("Failed to create audio thread");
					return 1;
				}

				cur_thread_id++;
				break;
			case DEL_THREAD:
				send_thread_signal(audio_cmd_t::END, cmd_opts.thread_id);
				pthread_join(audio[cmd_opts.thread_id]->audio_thread, NULL);
				break;
			case WAIT:
				sleep(cmd_opts.delay);
				break;
			case TERMINATE:
				break;
			default:
				break;
		}
	}

	return 0;
}

trace_cmd_t read_next_trace(cmd_opts_t *cmd_opts) {
	char in_line_buf[256]; // Mxx 256 characters in one line
	void* fres = fgets(in_line_buf, 256, input_trace);

	if (fres == NULL) {
		printf("FGETS returned EOF = %u\n", feof(input_trace));
		exit(1);
	}

	if ((strlen(in_line_buf) > 0) && (in_line_buf[strlen (in_line_buf) - 1] == '\n')) {
		in_line_buf[strlen(in_line_buf) - 1] = '\0';
	}

	DEBUG(printf("[APP] IN LINE = %s\n", in_line_buf));

	// Check first letter of trace line
	char cmd = in_line_buf[0];
	switch(cmd) {
		case 'A': // ADD
			sscanf(in_line_buf+2, "%d %d", &(cmd_opts->thread_id), &(cmd_opts->logn_samples));
			return trace_cmd_t::ADD_THREAD;
		case 'D': // DELETE
			sscanf(in_line_buf+2, "%d", &(cmd_opts->thread_id));
			return trace_cmd_t::DEL_THREAD;
		case 'W': // WAIT
			sscanf(in_line_buf+2, "%d", &(cmd_opts->delay));
			return trace_cmd_t::WAIT;
		case 'T': // TERMINATE
			return trace_cmd_t::TERMINATE;
		case '#': // NOOP 
			return trace_cmd_t::NOOP;
		default:
			printf("[APP] Bad character read at 0!");
			return trace_cmd_t::TERMINATE;
	}
}

void send_thread_signal(audio_cmd_t c, unsigned t) {
	audio_intf.cmd = c;
	audio_intf.thread_id = t;
    std::atomic_thread_fence(std::memory_order_seq_cst);
	audio_intf.valid_cmd.store(true, std::memory_order_seq_cst);
}

audio_cmd_t recv_thread_signal(unsigned t) {
	if (!audio_intf.valid_cmd.load(std::memory_order_seq_cst)) {
		return audio_cmd_t::EMPTY;
	}

	std::atomic_thread_fence(std::memory_order_seq_cst);
	
	if (audio_intf.thread_id == t) {
		audio_intf.valid_cmd.store(false, std::memory_order_seq_cst);
		return audio_intf.cmd;
	} else {
		return audio_cmd_t::EMPTY;
	}
}
