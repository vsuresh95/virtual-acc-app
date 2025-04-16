#ifndef __VAM_DF_GRAPH_H__
#define __VAM_DF_GRAPH_H__

struct df_graph_t;
struct df_node_t;
struct df_int_node_t;
struct df_leaf_node_t;
struct df_edge_t;
struct node_params_t;
struct edge_params_t;

// Available node primitives in this system
typedef enum {
    NONE = 0,
    AUDIO_FFT = 1,
    AUDIO_FIR = 2,
    AUDIO_FFI = 3
} primitive_t;

struct df_graph_t {
    // dummy node for entry to and exit from graph
    df_node_t *entry;
    df_node_t *exit;

     // for hierarchical graphs, where a node is a graph
    df_int_node_t *parent;

    // List of all nodes and edges in this graph
    std::vector<df_node_t *> df_node_list;
    std::vector<df_edge_t *> df_edge_list;

    // Helper functions
    df_graph_t(df_int_node_t *p);

    void add_df_node(df_node_t *node) {
        if (std::find(df_node_list.begin(), df_node_list.end(), node) == df_node_list.end())
            df_node_list.push_back(node);
    }

    void add_df_edge(df_edge_t *edge) {
        if (std::find(df_edge_list.begin(), df_edge_list.end(), edge) == df_edge_list.end())
            df_edge_list.push_back(edge);
    }

    df_node_t *get_entry() { return entry; }
    df_node_t *get_exit() { return exit; }
    df_int_node_t *get_parent() { return parent; }

    void dump();
};

struct df_node_t {
    // what primitive operation does this node map to?
    primitive_t prim;

    // parent node, if this node is part of a hierarchical graph
    df_int_node_t *parent;

    // List of all consumers (successors) of this node, and inbound/outbound edges
    std::vector<df_node_t *> consumers;
    std::vector<df_edge_t *> in_edges;
    std::vector<df_edge_t *> out_edges;

    node_params_t *params;

    // Type of node -- is it root? is it internal?
    bool root, internal;

    // If an accelerator is not found, we will map the node to this SW function.
    void *(*sw_impl)(void *args);

    // Helper functions
    df_node_t(primitive_t p1, df_int_node_t *p2, bool is_root) {
        prim = p1;
        parent = p2;
        root = is_root;
        internal = false;
    }

    void add_consumer(df_node_t *c) {
        if (std::find(consumers.begin(), consumers.end(), c) == consumers.end())
            consumers.push_back(c);
    }

    void add_in_edge(df_edge_t *e) {
        if (std::find(in_edges.begin(), in_edges.end(), e) == in_edges.end())
            in_edges.push_back(e);
    }

    void add_out_edge(df_edge_t *e) {
        if (std::find(out_edges.begin(), out_edges.end(), e) == out_edges.end())
            out_edges.push_back(e);
    }

    // Useful helpers in graph traversal
    primitive_t get_prim() { return prim; }
    df_int_node_t *get_parent() { return parent; }
    bool is_root() { return root; }
    bool is_internal() { return internal; }

    // set/get the node parameters
    void set_params(node_params_t *p) { params = p; }
    node_params_t *get_params() { return params; }

    // Recursvively traverse parents to find the root of the graph this node belongs to
    df_int_node_t *get_root();

    void set_sw_impl(void *s(void *)) { sw_impl = s; }

    static const char *dump_prim(primitive_t p) {
        switch(p) {
            case NONE : return (const char *) "NONE";
            case AUDIO_FFT: return (const char *) "AUDIO_FFT";
            case AUDIO_FIR: return (const char *) "AUDIO_FIR";
            case AUDIO_FFI: return (const char *) "AUDIO_FFI";
            default: return (const char *) "Unknown primitive";
        }
    }

    const char *dump_prim() { return dump_prim(prim); }

    void dump();
};

struct node_params_t {
    // Common memory pool buffer -- what is the shared memory address space this node is exposed to
    mem_pool_t *mem_pool;

    // This will be primitive specific parameters

    // Print for debug
    void dump() {
        printf("\tmem_pool = %p\n", mem_pool);
    } 
};

struct df_int_node_t : public df_node_t {
    // The hierarchical graph for this node
    df_graph_t *child_graph;

    // Name for this node (and graph) for debugging
    char name[100];

    // Helper functions
    df_int_node_t(primitive_t p1, df_int_node_t *p2, bool is_root, const char *n) : df_node_t(p1, p2, is_root) {
        child_graph = new df_graph_t(this);
        internal = true;
        std::strcpy(name, n);
    }

    df_int_node_t(primitive_t p1, df_int_node_t *p2, bool is_root) : df_node_t(p1, p2, is_root) {
        child_graph = new df_graph_t(this);
        internal = true;
        char dummy_name[100];
        sprintf(dummy_name, "No name, primitive: %s", dump_prim(p1));
        std::strcpy(name, dummy_name);
    }

    void add_df_node(df_node_t *node) { child_graph->add_df_node(node); }

    void add_df_edge(df_edge_t *edge);

    const char *get_name() { return name; }

    df_node_t *get_entry() { return child_graph->get_entry(); }
    df_node_t *get_exit() { return child_graph->get_exit(); }

    void dump() {
        printf("\n\tPrinting out %s:\n", get_name());
        df_node_t::dump();
        printf("\n\tPrinting out child graph for %s:\n", get_name());
        child_graph->dump();
    }
};

struct df_leaf_node_t : public df_node_t {
    // Helper functions
    df_leaf_node_t(primitive_t p1, df_int_node_t *p2, bool is_root) : df_node_t(p1, p2, is_root) {
        internal = false;
    }
};

struct df_edge_t {
    // Source and destination nodes of this edge
    df_node_t *src_node;
    df_node_t *dst_node;

    // We will generally assume that each edge correponds to one mem_queue
    // Optionally, an edge can be created with a payload size specified, if the app
    // does not wish to be exposed to the queue. In such cases, a queue is created dynamically
    // by the runtime whenever needed.
    mem_queue_t *data;
    unsigned payload_size;

    df_edge_t *bind_edge;
    bool binding;

    // Helper functions
    df_edge_t(df_node_t *src, df_node_t *dst) : src_node(src), dst_node(dst) {}
    df_edge_t(df_node_t *src, df_node_t *dst, unsigned s) : src_node(src), dst_node(dst), payload_size(s) {}
    df_edge_t(df_node_t *src, df_node_t *dst, df_edge_t *b) : src_node(src), dst_node(dst), bind_edge(b) { binding = true; }

    df_node_t *get_source() { return src_node; }
    df_node_t *get_destination() { return dst_node; }
    void set_mem_queue(mem_queue_t *d) { data = d; }
    mem_queue_t *get_mem_queue() { return data; }
    bool is_binding() { return binding; }
    df_edge_t * get_bind() { return bind_edge; }

    void dump() {
        printf("\tEdge in: %s, out: %s\n", src_node->dump_prim(), dst_node->dump_prim());
    }
};

#endif // __VAM_DF_GRAPH_H__
