#ifndef __NN_MODULE_H__
#define __NN_MODULE_H__

#include <hpthread.h>
#include <nn_graph.h>
#include <gemm_queue.h>
#include <nn_token.h>

// NN hpthread list 
typedef struct nn_hpthread_list {
    hpthread_t *th;
    struct nn_hpthread_list *next;
} nn_hpthread_list;

// Task descriptors
typedef struct nn_task_descr {
    hpthread_prim_t prim;
    struct nn_task_descr *next;
} nn_task_descr;

typedef struct gemm_task_descr {
    nn_task_descr common;
    gemm_params_t params;
} gemm_task_descr;

// NN handle provides an API endpoint for registering and interacting with a model
typedef struct {
    nn_graph_t *graph; // computational graph
    void *mem; // Memory handle for the module
    unsigned mem_allocated; // how much memory already allocated in this module
    unsigned input_flag_offset, output_flag_offset; // Offsets for sync flags
    unsigned id; // Module ID
    unsigned nprio; // Priority: 1 (highest) - 10 (lowest)
    bool cpu_invoke; // Should we invoke accelerator through CPU?
    nn_hpthread_list *th_list;
    nn_task_descr *descr_list;
} nn_module;

// User APIs for nn_module
void nn_module_load(nn_module *m, const char *n);
void nn_module_register(nn_module *m);
void nn_module_load_and_register(nn_module *m, const char *n);
void nn_module_release(nn_module *m);
static inline const char *nn_module_get_name(nn_module *m) { return m->graph->name; }

void nn_module_add_hpthread(nn_module *m, hpthread_t *th);
void nn_module_setpriority(nn_module *m, unsigned nprio);

nn_token_t *nn_module_forward_file(nn_module *m, const char *input_file);

// Data structures for BFS traversal of NN graph
typedef struct {
    nn_node_list *head, *tail;
} nn_queue_t;
void nn_queue_push(nn_queue_t *q, nn_node_t *n);
nn_node_t *nn_queue_pop(nn_queue_t *q);
bool nn_set_add_node(nn_node_list **s, nn_node_t *n);
static inline void nn_queue_delete(nn_queue_t *q) { while (nn_queue_pop(q) != NULL); }

void initialize_data(const char *input_file, nn_token_t *mem, unsigned len);

void nn_module_add_task_descr(nn_module *m, nn_task_descr *descr);
void print_descr_list(nn_module *m);
void print_hpthread_list(nn_module *m);

#endif // __NN_MODULE_H__
