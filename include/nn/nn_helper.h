#ifndef __NN_HELPER_H__
#define __NN_HELPER_H__

#include <common_helper.h>
#include <nn_operators.h>
#include <nn_graph.h>
#include <nn_token.h>

// Data structure to manage memory allocation within an NN graph
struct mem_pool_t {
    void *hw_buf;
    std::atomic<size_t> allocated; // number of bytes already allocated in this module

    // Allocate memory of some size
    size_t alloc(size_t mem_size);

    mem_pool_t();
    ~mem_pool_t();
};

struct nn_edge_args;

void initialize_data(const char *input_file, nn_token_t *mem, unsigned len);

#endif // __NN_HELPER_H__
