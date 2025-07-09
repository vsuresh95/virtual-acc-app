#ifndef __NN_FRONTEND_H__
#define __NN_FRONTEND_H__

#include <common_helper.h>
#include <nn_helper.h>
#include <nn_module.h>
#include <nn_intf.h>

#define NN_SLEEP_MIN   78125000 // ~1 seconds
#define NN_SLEEP_MAX   781250000 // ~10 seconds

////////////////////////////////////
// NN frontend is responsible for interfacing between
// app and hpthread interface 
class nn_frontend {
public:
    ////////////////////////////////////
    // Member variables

    // Map between model operators and hpthreads
    std::unordered_map<nn_module *, std::unordered_map<nn_node_t *, hpthread_t *>> model_hpthread_mapping;

    ////////////////////////////////////
    // Member functions

    // Parse the NN graph and request for hpthread
    void parse_nn_graph(nn_module *m);

    // Main run method
    void run_frontend();
};

#endif // __NN_FRONTEND_H__
