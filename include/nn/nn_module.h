#ifndef __NN_MODULE_H__
#define __NN_MODULE_H__

#include <nn_helper.h>
#include <sw_kernels.h>
#include <gemm_node_args.h>

////////////////////////////////////
// NN handle provides an API endpoint
// for registering and interacting with a model

class nn_module {
public:
    ////////////////////////////////////
    // Member variables

    // Data structure for the computational graph
    nn_graph_t *graph;

    // Memory pool (allocated by NN frontend)
    mem_pool_t *mem;

    // Input output sync flags
    atomic_flag_t input_flag, output_flag;
    // Input output data pointers
    nn_token_t *input_data, *output_data;

    // Module identifier
    unsigned id;

    ////////////////////////////////////
    // Member functions

    // Load a model using a description in a txt file
    void load(const char *n);

    // Register the model with NN frontend
    void _register();

    // Helper: Load and register a model in one call
    void load_and_register(const char *n);

    // Release the resources for this model
    void release();

    const char *get_name() { return graph->name; }

    // Constructors and destructor
    nn_module(unsigned i) { id = i; }
    nn_module() {}
    ~nn_module() { delete mem; }
};

#endif // __NN_MODULE_H__
