#ifndef __NN_GRAPH_H__
#define __NN_GRAPH_H__

#include <hpthread.h>

// Forward definitions
struct nn_node_t;
typedef struct nn_node_t nn_node_t;
struct nn_node_list;
typedef struct nn_node_list nn_node_list;
struct nn_edge_list;
typedef struct nn_edge_list nn_edge_list;
struct nn_edge_t;
typedef struct nn_edge_t nn_edge_t;

// NN graph
typedef struct {
    // dummy node for entry to and exit from graph
    nn_node_t *entry;
    nn_node_t *exit;
    nn_node_list *nodes; // List of nodes
    char name[100]; // For debug purposes
} nn_graph_t;

// User APIs for nn_graph
void nn_graph_create(nn_graph_t *g);
void nn_graph_add_nn_node(nn_graph_t *g, nn_node_t *n);
nn_node_t *nn_graph_get_nn_node(nn_graph_t *g, int id);
static inline nn_node_t *nn_graph_get_entry(nn_graph_t *g) { return g->entry; }
static inline nn_node_t *nn_graph_get_exit(nn_graph_t *g) { return g->exit; }
static inline const char *nn_graph_get_name(nn_graph_t *g) { return g->name; }
void nn_graph_dump(nn_graph_t *g);

// NN node
struct nn_node_t {
    int id; // Node ID
    unsigned nn_op; // what NN operand does this node map to?
    nn_edge_list *in_edges; // inbound edge list
    nn_edge_list *out_edges; // outbound edge list
    hpthread_t *th; // hpthread allocated for this node
    void *args; // arguments for this node
    char name[100]; // For debug purposes
};

// User APIs for nn_node
void nn_node_create(nn_node_t *n, int i, unsigned op);
void nn_node_add_in_edge(nn_node_t *n, nn_edge_t *e);
void nn_node_add_out_edge(nn_node_t *n, nn_edge_t *e);
static inline unsigned nn_node_get_op(nn_node_t *n) { return n->nn_op; }
const char *nn_node_dump_op(nn_node_t *n);
static inline const char *nn_node_get_name(nn_node_t *n) { return n->name; }
void nn_node_dump(nn_node_t *n);

// NN node list 
struct nn_node_list {
    nn_node_t *n;
    struct nn_node_list *next;
};

// NN edge args
typedef struct {
    unsigned len; // Number of elements transmitted over this edge
    size_t offset; // offset in memory
} nn_edge_args;

// NN edge
struct nn_edge_t {
    nn_node_t *src; // Source node
    nn_node_t *dst; // Destination node
    nn_edge_args *args; // Arguments
};

// User APIs for nn_edge
static inline void nn_edge_create(nn_edge_t *e, nn_node_t *s, nn_node_t *d) { e->src = s; e->dst = d; }
static inline nn_node_t *nn_edge_get_source(nn_edge_t *e) { return e->src; }
static inline nn_node_t *nn_edge_get_destination(nn_edge_t *e) { return e->dst; }
void nn_edge_dump(nn_edge_t *e);

// NN edge list 
struct nn_edge_list {
    nn_edge_t *e;
    struct nn_edge_list *next;
};

// NN operators
#define NN_OP_NONE 0
#define NN_OP_GEMM 1

#endif // __NN_GRAPH_H__
