#ifndef __NN_MODULE_H__
#define __NN_MODULE_H__

#include <hpthread.h>
#include <nn_graph.h>
#include <gemm_node_args.h>
#include <nn_token.h>

// NN hpthread list 
typedef struct nn_hpthread_list {
    hpthread_t *th;
    struct nn_hpthread_list *next;
} nn_hpthread_list;

// NN handle provides an API endpoint for registering and interacting with a model
typedef struct {
    nn_graph_t *graph; // computational graph
    void *mem; // Memory handle for the module
    unsigned mem_allocated; // how much memory already allocated in this module
    unsigned input_flag_offset, output_flag_offset; // Offsets for sync flags
    unsigned id; // Module ID
    unsigned nprio; // Priority: 1 (highest) - 10 (lowest)
    nn_hpthread_list *th_list;
} nn_module;

// User APIs for nn_module
void nn_module_load(nn_module *m, const char *n);
void nn_module_register(nn_module *m);
void nn_module_load_and_register(nn_module *m, const char *n);
void nn_module_release(nn_module *m);
static inline const char *nn_module_get_name(nn_module *m) { return m->graph->name; }

void nn_module_add_hpthread(nn_module *m, hpthread_t *th);

// Data structures for BFS traversal of NN graph
typedef struct {
    nn_node_list *head, *tail;
} nn_queue_t;
void nn_queue_push(nn_queue_t *q, nn_node_t *n);
nn_node_t *nn_queue_pop(nn_queue_t *q);
bool nn_set_add_node(nn_node_list **s, nn_node_t *n);

void initialize_data(const char *input_file, nn_token_t *mem, unsigned len);

#endif // __NN_MODULE_H__
