#include <iostream>
#include <audio-task.hpp>
#include <vam-worker.hpp>

int main(int argc, char **argv) {
	printf("\n-------------------------\n");
	printf("Starting Audio App\n");
	printf("---------------------------\n");

	// Create a new request interface object (that will be passed to the workers)
	vam_req_intf_t *req_intf = new vam_req_intf_t;

	// Create a VAM object
	vam_worker *vam = (vam_worker *) new vam_worker(req_intf);

	// Launching a VAM thread
	pthread_t vam_thread;
	if (pthread_create(&vam_thread, NULL, vam->run, (void *) vam) != 0) {
        perror("Failed to create VAM thread");
        return 1;
    }

	// Declare a list of audio tasks
	audio_task **audio = new audio_task*[NUM_AUDIO_THREADS];

	// Launching all threads for audio tasks
	pthread_t audio_thread[NUM_AUDIO_THREADS];

	for (unsigned i = 0; i < NUM_AUDIO_THREADS; i++) {
	 	audio[i] = (audio_task *) new audio_task(i, req_intf);
		if (pthread_create(&audio_thread[i], NULL, audio[i]->run, (void *) (audio[i])) != 0) {
			perror("Failed to create audio thread");
			return 1;
		}
	}

	printf("Threads launched for VAM and audio\n");

	// Trigger start of audio thread 1

	// Trigger start of audio thread 2

	// Wait for all threads to return
	pthread_join(vam_thread, NULL);
	for (unsigned i = 0; i < NUM_AUDIO_THREADS; i++) {
		pthread_join(audio_thread[i], NULL);
	}

	return 0;
}
