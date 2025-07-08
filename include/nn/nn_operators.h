#ifndef __NN_OPERATORS_H__
#define __NN_OPERATORS_H__

namespace nn {
    
    typedef enum : unsigned {
        NONE = 0,
        GEMM = 1,
    } operator_t;

};

// Helper function for printing primitive names
// -- Defined in nn_graph.cpp
const char *get_op_name(nn::operator_t p);

#endif // __NN_OPERATORS_H__
