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
    initialize_data("input.txt", m.input_data, 4096);
    m.input_flag.store(1); // set

    // Wait for the last node to finish.
    while(m.output_flag.load() != 1);

    // Load reference output
    nn_token_t *gold = new nn_token_t[64];
    initialize_data("output.txt", gold, 64);

    // Compare with actual output
    unsigned errors = 0;
    for (unsigned j = 0; j < 64; j++) {
        if ((fabs(gold[j] - m.output_data[j]) / fabs(gold[j])) > ERR_TH) {
            if (errors < 2) { HIGH_DEBUG(printf("\tGOLD[%u] = %f vs %f = out[%u]\n", j, (float) gold[j], (float) m.output_data[j], j);) }
            errors++;
        }
    }

    m.output_flag.store(0); // reset

    printf("\tRelative error > %.02f for %d values out of 64\n", ERR_TH, errors);
}

#endif // __FCNN_H__
