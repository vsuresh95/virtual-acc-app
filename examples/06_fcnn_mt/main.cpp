////////////////////////////////////
// Example application for multithreaded
// pipelined FCNN using GEMM accelerator
// with the ML frontend interface and
// hpthread interface
////////////////////////////////////

#include <helper.h>
#include <fcnn.h>

int main(int argc, char **argv) {
    printf("[MAIN] Starting app: FCNN multithreaded!\n");

    // Start NUM_THREADS threads for the FCNN function
    std::thread th[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        th[i] = std::thread(&fcnn_worker, i);
    }

    std::array<float, NUM_THREADS> old_iters = {};
    unsigned total_iterations = 0;
    const unsigned sleep_seconds = 4;
    while (total_iterations != NUM_ITERATIONS * NUM_THREADS) {
        sleep(sleep_seconds); // 2 seconds

        std::array<float, NUM_THREADS> iters;
        ips_report.push_back(iters);
        total_iterations = 0;
        printf("[MAIN] Iters = ");
        for (int i = 0; i < NUM_THREADS; i++) {
            unsigned new_iters = iterations_done[i].load();
            float ips = (new_iters - old_iters[i]) / sleep_seconds;
            ips_report.back()[i] = ips;
            old_iters[i] = new_iters;
            total_iterations += new_iters;
            printf("T%d:%d, ", i, new_iters);
        }
        printf("\n");
    }

    // Join all threads when done
    for (int i = 0; i < NUM_THREADS; i++) {
        th[i].join();
    }

    hpthread_t::report();
    
    // Print out the iterations per second report
    printf("-------------------------------------------------------------------\n");
    printf("  #\t");
    for (int i = 0; i < NUM_THREADS; i++) {
        printf("%d\t", i);
    }
    printf("\n-------------------------------------------------------------------\n");
    for (size_t i = 0; i < ips_report.size(); i++) {
        printf("  %lu\t", i);
        for (int j = 0; j < NUM_THREADS; j++) {
            printf("%0.2f\t", ips_report[i][j]);
        }
        printf("\n");
    }
}
