#include <helper.h>
#include <fcnn.h>

////////////////////////////////////
// Example application for fully connected
// neural network (FCNN) using GEMM
// accelerator with the ML frontend interface
// and hpthread interface
////////////////////////////////////

int main(int argc, char **argv) {
    printf("[MAIN] Starting app: FCNN!\n");

    // Start a thread for the FCNN function
	HIGH_DEBUG(printf("[MAIN] Launching a new thread for FCNN worker!\n");)
    std::thread worker_th = std::thread(&fcnn_worker, 0);

    // Join the worker thread
    worker_th.join();
}
