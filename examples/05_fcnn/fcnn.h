#ifndef __FCNN_H__
#define __FCNN_H__

#include <nn_frontend.h>

void fcnn_worker(unsigned worker_id) {
    HIGH_DEBUG(printf("[WORKER%d] Starting worker %d!\n", worker_id, worker_id);)

    // Load a model from a text file (and) register with NN frontend
    nn_module m;
    m.load_and_register("model.txt");

    // Check if the input flag is set; else write inputs
    while(m.input_flag.load() != 0);
    initialize_data("input.txt", m.input_data, 64);
    m.input_flag.store(1); // set

    // Wait for the last node to finish.
    while(m.output_flag.load() != 1);
    m.output_flag.store(0); // reset

    unsigned errors = 0;
    printf("Errors = %d\n", errors);
}

#endif // __FCNN_H__
