#include <audio-worker.hpp>
#include <vam-module.hpp>

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

    unsigned num_audio_threads = 1;
    if (argc > 1){
        num_audio_threads = atoi(argv[1]);
    }

	// Initialize app->worker sync variables
	init_sync_threads(num_audio_threads);

	for (unsigned i = 0; i < num_audio_threads; i++) {
		char name[100];
		sprintf(name, "AUDIO %d", i);
	 	audio[i] = (audio_worker *) new audio_worker(name);

		if (pthread_create(&(audio[i]->audio_thread), NULL, audio[i]->run, (void *) (audio[i])) != 0) {
			perror("Failed to create audio thread");
			return 1;
		}
	}

	printf("[APP] Threads launched for VAM and audio\n");

	// Wait for all threads to return
	pthread_join(vam_thread, NULL);
	for (unsigned i = 0; i < num_audio_threads; i++) {
		pthread_join(audio[i]->audio_thread, NULL);
	}

	return 0;
}
