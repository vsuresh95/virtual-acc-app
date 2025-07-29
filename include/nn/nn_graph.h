#ifndef __NN_GRAPH_H__
#define __NN_GRAPH_H__

#include <hpthread.h>
#include <nn_helper.h>

/////////////////////////////////////////
// Struct definitions for creating an NN computational graph 

// Forward definitions
struct nn_graph_t;
struct nn_node_t;
struct nn_edge_t;
struct nn_node_args;
struct nn_edge_args;

/////////////////////////////////////////
// NN graph
struct nn_graph_t {
    // dummy node for entry to and exit from graph
    nn_node_t *entry;
    nn_node_t *exit;

    // List of all nodes and edges in this graph
    std::vector<nn_node_t *> nn_node_list;
    std::vector<nn_edge_t *> nn_edge_list;

    // For debug purposes
    char name[100];

    // Helper functions
    nn_graph_t();
    void add_nn_node(nn_node_t *node);
    nn_node_t *get_nn_node(int id);
    void add_nn_edge(nn_edge_t *edge);

    nn_node_t *get_entry() { return entry; }
    nn_node_t *get_exit() { return exit; }
    const char *get_name() { return name; }

    void dump();
};

/////////////////////////////////////////
// NN node
struct nn_node_t {
    // Node ID
    int id;

    // what operand does this node map to?
    nn::operator_t op;

    // List of all consumers (successors) of this node, and inbound/outbound edges
    std::vector<nn_node_t *> consumers;
    std::vector<nn_edge_t *> in_edges;
    std::vector<nn_edge_t *> out_edges;

    // hpthread allocated for this node and its arguments
    hpthread_t *th;
    hpthread_args *args;

    // If an accelerator is not found, we will map the node to this SW function.
    void *(*sw_routine)(void *args);

    // For debug purposes
    char name[100];

    // Helper functions
    nn_node_t(int i, nn::operator_t o) { id = i; op = o; }

    void add_consumer(nn_node_t *c) {
        if (std::find(consumers.begin(), consumers.end(), c) == consumers.end())
            consumers.push_back(c);
    }

    void add_in_edge(nn_edge_t *e) {
        if (std::find(in_edges.begin(), in_edges.end(), e) == in_edges.end())
            in_edges.push_back(e);
    }

    void add_out_edge(nn_edge_t *e) {
        if (std::find(out_edges.begin(), out_edges.end(), e) == out_edges.end())
            out_edges.push_back(e);
    }

    // Useful helpers in graph traversal
    nn::operator_t get_op() { return op; }

    // Set the pointer to the software implementation of this node
    void set_sw_routine(void *s(void *)) { sw_routine = s; }

    // Helper functions for printing
    const char *dump_op() { return get_op_name(op); }
    const char *get_name() { return name; }

    void dump();
};

/////////////////////////////////////////
// Data flow edge
struct nn_edge_t {
    // Source and destination nodes of this edge
    nn_node_t *src_node;
    nn_node_t *dst_node;

    nn_edge_args *args;
 
    // Helper functions
    nn_edge_t(nn_node_t *src, nn_node_t *dst) : src_node(src), dst_node(dst) {}

    nn_node_t *get_source() { return src_node; }
    nn_node_t *get_destination() { return dst_node; }

    void dump() {
        printf("\tEdge in: %s(%d), out: %s(%d)\n", src_node->dump_op(), src_node->id, dst_node->dump_op(), dst_node->id);
    }
};

struct nn_edge_args {
    // Number of elements transmitted over this edge and its offset in memory
    unsigned len;
    size_t offset;
};

#endif // __NN_GRAPH_H__
