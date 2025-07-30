////////////////////////////////////
// Example application for multithreaded
// pipelined FCNN using GEMM accelerator
// with the ML frontend interface and
// hpthread interface
////////////////////////////////////

#include <helper.h>
#include <fcnn.h>

int main(int argc, char **argv) {
    if (argc > 2) {
		NUM_THREADS = atoi(argv[1]);
		NUM_ITERATIONS = atoi(argv[2]);
    } else if (argc > 1) {
		NUM_THREADS = atoi(argv[1]);
	}

    printf("[MAIN] Starting app: FCNN multithreaded T:%d, I:%d!\n", NUM_THREADS, NUM_ITERATIONS);

    // Start NUM_THREADS threads for the FCNN function
    std::thread th[MAX_THREADS];
    for (unsigned i = 0; i < NUM_THREADS; i++) {
        th[i] = std::thread(&fcnn_worker, i);
    }

    std::array<float, MAX_THREADS> old_iters = {};
    unsigned total_iterations = 0;
    const unsigned sleep_seconds = 4;
    while (total_iterations != NUM_ITERATIONS * NUM_THREADS) {
        sleep(sleep_seconds); // 2 seconds

        std::array<float, MAX_THREADS> iters;
        ips_report.push_back(iters);
        total_iterations = 0;
        printf("[MAIN] IPS = ");
        for (unsigned i = 0; i < NUM_THREADS; i++) {
            unsigned new_iters = iterations_done[i].load();
            float ips = (new_iters - old_iters[i]) / sleep_seconds;
            ips_report.back()[i] = ips;
            old_iters[i] = new_iters;
            total_iterations += new_iters;
            printf("T%d:%0.2f(%d), ", i, ips, new_iters);
        }
        printf("\n");
    }

    // Join all threads when done
    for (unsigned i = 0; i < NUM_THREADS; i++) {
        th[i].join();
    }

    hpthread_t::report();
    
    // Print out the iterations per second report
    for (unsigned i = 0; i < NUM_THREADS + 1; i++) {
        printf("--------");
    }
    printf("\n");
    printf("  #\t");
    for (unsigned i = 0; i < NUM_THREADS; i++) {
        printf("%d\t", i);
    }
    printf("\n");
    for (unsigned i = 0; i < NUM_THREADS + 1; i++) {
        printf("--------");
    }
    printf("\n");
    for (size_t i = 0; i < ips_report.size(); i++) {
        printf("  %lu\t", i);
        for (unsigned j = 0; j < NUM_THREADS; j++) {
            printf("%0.2f\t", ips_report[i][j]);
        }
        printf("\n");
    }
}
