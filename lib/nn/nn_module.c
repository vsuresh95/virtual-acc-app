#include <stdlib.h>
#include <nn_module.h>
#include <gemm_node_args.h>
#include <string.h>
#include <libesp.h>
#ifndef ENABLE_VAM
#include <dirent.h>
#include <fnmatch.h>
#include <vam_physical_accel.h>
#include <gemm_def.h>
#endif

// Load a model using a description in a txt model_def
void nn_module_load(nn_module *m, const char *n) {
    // Check if the model description file exists
    FILE *model_def = fopen(n, "r");
	if (!model_def) {
		perror("Failed to read model description file");
		exit(1);
	}

    // Create an NN computational graph and memory for this model
    m->graph = (nn_graph_t *) malloc (sizeof(nn_graph_t));
    nn_graph_create(m->graph);
    const unsigned mem_size = 8 * 1024 * 1024; // 8MB
    m->mem = esp_alloc(mem_size);
    m->mem_allocated = 0;
    m->req_cnt = 0;
    m->th_list = NULL;
    m->descr_list = NULL;
    m->loop_around = 1;
    m->pending_requeues = 0;
    #ifndef ENABLE_VAM
    m->accel_list = NULL;
    m->active_cycles = 0;
    #endif

	char in_line_buf[256]; // Max 256 characters in one line
    if (fgets(in_line_buf, 100, model_def) != NULL) {
        if ((strlen(in_line_buf) > 0) && (in_line_buf[strlen (in_line_buf) - 1] == '\n')) {
            in_line_buf[strlen(in_line_buf) - 1] = '\0';
        }

	    char module_name[50];
        sscanf(in_line_buf, "%s %d", module_name, &m->nprio);
        snprintf(m->graph->name, 100, "%s.%d", module_name, m->id);
	    HIGH_DEBUG(printf("[NN%d] MODEL NAME = %s\n", m->id, nn_module_get_name(m)));
    }

    while (fgets(in_line_buf, 256, model_def) != NULL) {
        if ((strlen(in_line_buf) > 0) && (in_line_buf[strlen (in_line_buf) - 1] == '\n')) {
            in_line_buf[strlen(in_line_buf) - 1] = '\0';
        }
	    HIGH_DEBUG(printf("[NN%d] IN LINE = %s\n", m->id, in_line_buf));

        // Check first letter of trace line
        char type = in_line_buf[0];
        switch(type) {
            case 'N': { // Node
                int node_id;
                unsigned nn_op;
                int ofs = 0;
                sscanf(in_line_buf, "N %d %d %n", &node_id, &nn_op, &ofs);
                switch(nn_op) {
                    case NN_OP_GEMM: { // GEMM operator
                        // Add a new GEMM node to the graph
                        nn_node_t *gemm_node = (nn_node_t *) malloc (sizeof(nn_node_t));
                        nn_node_create(gemm_node, node_id, NN_OP_GEMM);
                        // Read the parameters of the GEMM from the following chars
                        gemm_node_args *args = (gemm_node_args *) malloc (sizeof(gemm_node_args));
                        gemm_params_t *params= &(args->params);
                        if (sscanf(in_line_buf + ofs, "%d %d %d %s", &params->dim_m, &params->dim_n, &params->dim_k, args->input_file) == 3) {
                            // If no input file was provided, the pointer is marked invalid.
                            HIGH_DEBUG(printf("[NN%d] No input file provided for GEMM node %d, using random data.\n", m->id, node_id);)
                            args->input_file[0] = '\n'; args->input_file[1] = '\0';
                        } 
                        gemm_node->args = (void *) args;
                        // Add the created node to the graph for the module
                        nn_graph_add_nn_node(m->graph, gemm_node);
                        snprintf(gemm_node->name, 120, "%s.gemm.%d", m->graph->name, node_id);
                        HIGH_DEBUG(
                            printf("\t[NN%d] Adding node %s...", m->id, nn_node_get_name(gemm_node));
                            nn_node_dump(gemm_node);
                        )
                        break;
                    }
                    case NN_OP_NONE: {
                        nn_node_t *none_node = (nn_node_t *) malloc (sizeof(nn_node_t));
                        nn_node_create(none_node, node_id, NN_OP_NONE);
                        nn_graph_add_nn_node(m->graph, none_node);
                        break;
                    }
                    default:
                        break;
                }
            }
            break;
            case 'E': { // Edge
                int src_id, dst_id, len;
                sscanf(in_line_buf+2, "%d %d %d", &src_id, &dst_id, &len);
                // Get source and destination node pointers
                nn_node_t *src = nn_graph_get_nn_node(m->graph, src_id);
                nn_node_t *dst= nn_graph_get_nn_node(m->graph, dst_id);
                nn_edge_t *e = (nn_edge_t *) malloc (sizeof(nn_edge_t));
                nn_edge_create(e, src, dst);
                // Assign length to the edge from the input values
                e->args = (nn_edge_args *) malloc (sizeof(nn_edge_args));
                e->args->len = len;
                nn_node_add_out_edge(src, e);
                nn_node_add_in_edge(dst, e);
                HIGH_DEBUG(
                    printf("\t[NN%d] Adding edge...", m->id);
                    nn_edge_dump(e);
                )
                break;
            }
            break;
            case '#': // Comment
                break;
            default:
                printf("[NN%d] Bad character read at 0!", m->id);
                break;
        }
    }
    HIGH_DEBUG(nn_graph_dump(m->graph);)
}

// Register the model with NN frontend
void nn_module_register(nn_module *m) {
    // We will maintain a queue of nodes to-be-visited in BFS order and a list of visited nodes
    nn_queue_t *q = (nn_queue_t *) malloc (sizeof(nn_queue_t));
    q->head = q->tail = NULL;
    nn_node_list *visited = NULL;
    // Do BFS traversal of the routine DFG by starting at the entry node
    nn_node_t *entry = nn_graph_get_entry(m->graph);
    // Add entry to queue and set
    nn_queue_push(q, entry);
    nn_set_add_node(&visited, entry);
    LOW_DEBUG(printf("[NN%d] Parsing NN graph for %s\n", m->id, nn_module_get_name(m)));
    // Allocate queues for each if the user specified a number
    unsigned *queue_list = NULL;
    bool limit_threads = false;
    unsigned queue_count = 0;
    unsigned layer_count = 0;
    unsigned thread_count = 0;
    if (m->n_threads > 0) {
        queue_list = (unsigned *) malloc (sizeof(unsigned) * (m->n_threads + 1)); // one for each thread + one for exit
        for (unsigned i = 0; i < m->n_threads + 1; i++) {
            // Allocate input queue
            queue_list[i] = nn_module_malloc(m, SM_COMMON_SIZE + (SM_QUEUE_SIZE * 2)); // uint64_t entries
            sm_queue_t *q = (sm_queue_t *) ((unsigned *) (m->mem) + queue_list[i]);
            sm_queue_init(q);
            HIGH_DEBUG(printf("[NN%d] Queue %d offset = %d\n", m->id, i, queue_list[i]);)
        }
        limit_threads = true;
    }

    while (q->head != NULL) { // !empty
        nn_node_t *current = nn_queue_pop(q);
        if (!current) break;

        HIGH_DEBUG(printf("[NN%d] Found node %s(%d)\n", m->id, nn_node_dump_op(current), current->id));

        // Iterate through all out edges of this node and add the destinations to the visitor set and the BFS queue.
        nn_edge_list *cons = current->out_edges;
        while(cons != NULL) {
            // Check that the consumer was not visited earlier and push into the queue.
            nn_edge_t *out = cons->e;
            nn_node_t *dst = nn_edge_get_destination(out);
            if (nn_set_add_node(&visited, dst)) {
                nn_queue_push(q, dst);

                // Allocate memory for the edge and store the offset in the args (for QUEUE_SIZE)
                nn_edge_args *edge_args = out->args;
                edge_args->data_offset = nn_module_malloc(m, SM_QUEUE_SIZE * edge_args->len);
                HIGH_DEBUG(printf("[NN%d] Data offset for edge to %s = %d\n", m->id, nn_node_get_name(dst), edge_args->data_offset);)
                // TODO assumes all destination tasks are GEMM and have same task parameters
                // Allocate descriptor pointers + descriptors
                edge_args->descr_offset = nn_module_malloc(m, GEMM_TASK_DESCR_WORDS + (SM_QUEUE_SIZE * GEMM_ENTRY_SIZE));
                gemm_task_descr *descr = (gemm_task_descr *) ((unsigned *) (m->mem) + edge_args->descr_offset);
                descr->common.prim = PRIM_GEMM;
                // Descriptors are arranged contiguously
                HIGH_DEBUG(printf("[NN%d] Descr offset for edge to %s = %d\n", m->id, nn_node_get_name(dst), edge_args->descr_offset);)
                HIGH_DEBUG(printf("[NN%d] Printing task descriptor for edge to %s...\n", m->id, nn_node_get_name(dst));)
                for (int i = 0; i < SM_QUEUE_SIZE; i++) {
                    descr->descr_offset[i] = edge_args->descr_offset + GEMM_TASK_DESCR_WORDS + (i * GEMM_ENTRY_SIZE);
                    HIGH_DEBUG(printf("\tdescr->descr_offset[%d] = %lu\n", i, descr->descr_offset[i]);)
                }
                if (limit_threads) {
                    // Check if this is the last node in the graph; if yes, assign the final queue
                    edge_args->queue_offset = (dst->is_exit) ? queue_list[m->n_threads] : queue_list[queue_count % m->n_threads];
                    queue_count++;
                    HIGH_DEBUG(printf("[NN%d] Assigned queue offset for edge to %s = %d\n", m->id, nn_node_get_name(dst), edge_args->queue_offset);)
                } else {
                    // Allocate queue descriptors for the output edge
                    edge_args->queue_offset = nn_module_malloc(m, SM_COMMON_SIZE + (SM_QUEUE_SIZE * 2)); // uint64_t entries
                    // Init the queue
                    sm_queue_t *q = (sm_queue_t *) ((unsigned *) (m->mem) + edge_args->queue_offset);
                    sm_queue_init(q);
                    HIGH_DEBUG(printf("[NN%d] Queue offset for edge to %s = %d\n", m->id, nn_node_get_name(dst), edge_args->queue_offset);)
                }
            }
            cons = cons->next;
        }

        // Ensure that the current node is an entry or exit
        unsigned nn_op = nn_node_get_op(current);
        if (nn_op != NN_OP_NONE) {
            switch(nn_op) {
                case NN_OP_GEMM: {
                    // Get node params for allocating weights (input/output done above)
                    gemm_node_args *gemm_args = (gemm_node_args *) current->args;
                    gemm_params_t *params = &(gemm_args->params);
                    // Allocate memory for weights
                    params->weight_base = nn_module_malloc(m, params->dim_n * params->dim_k); 
                    // Retrieve input and output offsets from its first edges; assumes single producer, single consumer
                    nn_edge_args *in_args = current->in_edges->e->args; nn_edge_args *out_args = current->out_edges->e->args;
                    // Get the descriptors of incoming edge
                    gemm_task_descr *descr = (gemm_task_descr *) ((unsigned *) (m->mem) + in_args->descr_offset);
                    for (int i = 0; i < SM_QUEUE_SIZE; i++) {
                        gemm_queue_entry_t *descr_entry = (gemm_queue_entry_t *) ((unsigned *) (m->mem) + descr->descr_offset[i]);
                        sm_queue_entry_t *descr_common = &(descr_entry->common);
                        gemm_params_t *descr_params = &(descr_entry->gemm_params);
                        *descr_params = *params;
                        descr_params->input_base = in_args->data_offset + i * (in_args->len);
                        descr_params->output_base = out_args->data_offset + i * (out_args->len);
                        if (limit_threads) {
                            // Check if this is the desctiptor for the last thread
                            descr_common->output_queue = (layer_count % m->n_threads == m->n_threads - 1) ? queue_list[m->n_threads] : out_args->queue_offset;
                        } else {
                            descr_common->output_queue = out_args->queue_offset;
                        }
                        descr_common->output_entry = out_args->descr_offset + GEMM_TASK_DESCR_WORDS + i * (GEMM_ENTRY_SIZE);
                        HIGH_DEBUG(print_gemm_entry(descr_entry);)
                    }
                    layer_count++;

                    // if we are using VAM, request for hpthread
                    #ifdef ENABLE_VAM
                    // Create a hpthread for this node (if not already created for Mozart)
                    if (!(limit_threads && thread_count == m->n_threads)) {
                        hpthread_args_t *h_args = (hpthread_args_t *) malloc(sizeof(hpthread_args_t));
                        // Set hpthread mem to model mem
                        h_args->mem = m->mem;
                        // Set queue pointer from edge arguments
                        h_args->queue_ptr = in_args->queue_offset;
                        // Declare hpthread and assign attributes
                        hpthread_t *th = (hpthread_t *) malloc(sizeof(hpthread_t));
                        th->cpu_invoke = m->cpu_invoke;
                        hpthread_init(th, m->id);
                        hpthread_setargs(th, h_args);
                        char hpthread_name[50];
                        snprintf(hpthread_name, 50, nn_node_get_name(current));
                        hpthread_setname(th, hpthread_name);
                        hpthread_setprimitive(th, PRIM_GEMM);
                        hpthread_setpriority(th, m->nprio);
                        #if !defined(ENABLE_SM) || defined(ENABLE_MOZART)
                        hpthread_setaffinity(th, ((m->id - 1) * m->n_threads) + thread_count + 1); // Assign to different accelerators with m->n_threads
                        #else
                        hpthread_setaffinity(th, (m->id * 3) + (thread_count % 3) + 1); // Try to assign to same accelerator as module ID
                        #endif
                        HIGH_DEBUG(printf("[NN%d] queue ptr for %s = %d...\n", m->id, hpthread_name, h_args->queue_ptr););
                        HIGH_DEBUG(printf("[NN%d] Before hpthread create for %s...\n", m->id, hpthread_name));
                        // Create a hpthread
                        hpthread_create(th);
                        // Assign the thread to the model list
                        nn_module_add_hpthread(m, th);
                        thread_count++;
                    }
                    #else
                    // Check if an accelerator for GEMM has already been assigned
                    physical_accel_t *accel_list = m->accel_list;
                    bool accel_found = false;
                    while (accel_list != NULL) {
                        if (accel_list->prim == PRIM_GEMM) accel_found = true;
                        accel_list = accel_list->next;
                    }
                    if (accel_found == false) {
                        // Probe the system for an accelerator
                        if (!nn_module_search_accel(m, PRIM_GEMM)) {
                            printf("[NN%d] No GEMM accelerator found during search!\n", m->id);
                            exit(1);
                        }
                    }
                    #endif

                    // Initialize the weights if present
                    nn_token_t *wgt_address = ((nn_token_t *) (m->mem) + params->weight_base);
                    initialize_data(gemm_args->input_file, wgt_address, params->dim_n * params->dim_k);
                    nn_module_add_task_descr(m, (nn_task_descr *) descr);
                    break;
                }
                default: break;
            }
        }
    }
    // Set the number of loop arounds for this module (if limited threads)
    // TODO: unaligned layers will not work in pipelining mode
    if (limit_threads) {
        m->loop_around = (layer_count + m->n_threads - 1) / m->n_threads; // ceiling division
        HIGH_DEBUG(printf("[NN%d] Setting loop_around = %d for model %s\n", m->id, m->loop_around, nn_module_get_name(m));)
    }

    // Populate the descriptor for the exit stage -- required for output data checking
    // TODO assuming exit node is a GEMM -- need to fix input/output fields in queue entry
    nn_node_t *exit = nn_graph_get_exit(m->graph);
    // Retrieve input offsets from its first edges; assumes single producer, single consumer
    nn_edge_args *in_args = exit->in_edges->e->args;
    // Get the descriptors of incoming edge
    gemm_task_descr *descr = (gemm_task_descr *) ((unsigned *) (m->mem) + in_args->descr_offset);
    descr->common.prim = PRIM_NONE;
    for (int i = 0; i < SM_QUEUE_SIZE; i++) {
        gemm_queue_entry_t *descr_entry = (gemm_queue_entry_t *) ((unsigned *) (m->mem) + descr->descr_offset[i]);
        gemm_params_t *descr_params = &(descr_entry->gemm_params);
        descr_params->input_base = in_args->data_offset + i * (in_args->len);
        sm_queue_entry_t *descr_common = &(descr_entry->common);
        descr_common->output_queue = 42424242; // MAGIC number for last stage
        HIGH_DEBUG(print_gemm_entry(descr_entry);)
    }
    nn_module_add_task_descr(m, (nn_task_descr *) descr);
    HIGH_DEBUG(print_descr_list(m);)

    // Calculate the input and output flag addresses for this module
    nn_edge_list *cur; nn_edge_t *edge = NULL;
    for (cur = exit->in_edges; cur; cur = cur->next) {
        edge = cur->e;
    } m->output_queue = (sm_queue_t *) ((unsigned *) (m->mem) + edge->args->queue_offset);
    HIGH_DEBUG(printf("[NN] output_queue_offset for model %s = %d\n", nn_module_get_name(m), edge->args->queue_offset);)
    for (cur = entry->out_edges; cur; cur = cur->next) {
        edge = cur->e;
    } m->input_queue = (sm_queue_t *) ((unsigned *) (m->mem) + edge->args->queue_offset);
    HIGH_DEBUG(printf("[NN] input_queue_offset for model %s = %d\n", nn_module_get_name(m), edge->args->queue_offset);)

    #ifdef ENABLE_VAM
    HIGH_DEBUG(print_hpthread_list(m);)
    #endif

    // Clean up temporary data structures
    free(queue_list);
    nn_queue_delete(q);
    while(visited != NULL) {
        nn_node_list *next = visited->next;
        free(visited);
        visited = next;
    }
}

// Helper: Load and register a model in one call
void nn_module_load_and_register(nn_module *m, const char *n) {
    nn_module_load(m, n);
    nn_module_register(m);
}

// Release the resources for this model
void nn_module_release(nn_module *m) {
    #ifdef ENABLE_VAM
    nn_hpthread_list *cur = m->th_list;
    while(cur != NULL) {
        nn_hpthread_list *next = cur->next;
        hpthread_join(cur->th);
        free(cur->th->args);
        free(cur->th);
        free(cur);
        cur = next;
    }
    #else
    physical_accel_t *cur = m->accel_list;
    while (cur != NULL) {
        physical_accel_t *next = cur->next;
        free(cur->esp_access_desc);
        free(cur);
        cur = next;
    }
    #endif
    esp_free(m->mem);
    nn_graph_delete(m->graph);
}

void nn_module_setprio(nn_module *m, unsigned nprio) {
    m->nprio = nprio;
    nn_hpthread_list *cur = m->th_list;
    while(cur != NULL) {
        nn_hpthread_list *next = cur->next;
        hpthread_setpriority(cur->th, nprio);
        cur = next;
    }
}

void nn_module_add_hpthread(nn_module *m, hpthread_t *th) {
    nn_hpthread_list *item = (nn_hpthread_list *) malloc (sizeof(nn_hpthread_list));
    item->next = NULL;
    item->th = th;
    if (!m->th_list) {
        m->th_list = item;
        return;
    }
    nn_hpthread_list *list = m->th_list;
    while (list->next) list = list->next;
    list->next = item;
}

void nn_module_setpriority(nn_module *m, unsigned nprio) {
    nn_hpthread_list *cur = m->th_list;
    while(cur != NULL) {
        nn_hpthread_list *next = cur->next;
        hpthread_setpriority(cur->th, nprio);
        cur = next;
    }
}

void nn_module_run(nn_module *m, nn_token_t *input_data, nn_token_t *output_data, unsigned input_len, unsigned output_len, bool real_data) {
    HIGH_DEBUG(printf("[NN%d] Starting nn_module_run for %s\n", m->id, nn_module_get_name(m)));
    #ifdef ENABLE_VAM
    nn_task_descr *descr_list = m->descr_list;
    uint64_t descr_offset;
    sm_queue_t *in_q = m->input_queue;
    sm_queue_t *out_q = m->output_queue;
    switch(descr_list->prim) {
        case PRIM_GEMM: {
            gemm_task_descr *descr = (gemm_task_descr *) descr_list;
            descr_offset = descr->descr_offset[0];
        }    
        default: break;
    }
    if (real_data) {
        nn_token_t *input_addr;
        // TODO: assumes it is a GEMM task
        gemm_queue_entry_t *descr = (gemm_queue_entry_t *) ((unsigned *) (m->mem) + descr_offset);
        input_addr = (nn_token_t *) ((unsigned *) (m->mem) + descr->gemm_params.input_base);
        memcpy(input_addr, input_data, input_len);
    }
    for (unsigned loop_count = 0; loop_count < m->loop_around; loop_count++) {
        HIGH_DEBUG(printf("[NN%d] Programming PRIM_GEMM descr at %lu for req %d...\n", m->id, descr_offset, m->req_cnt););
        sm_queue_push(in_q, descr_offset);
        while(sm_queue_empty(out_q)) { SCHED_YIELD; }
        descr_offset = sm_queue_pop(out_q);
    }
    if (real_data) {
        nn_token_t *output_addr;
        // TODO: assumes it is a GEMM task
        gemm_queue_entry_t *descr = (gemm_queue_entry_t *) ((unsigned *) (m->mem) + descr_offset);
        output_addr = (nn_token_t *) ((unsigned *) (m->mem) + descr->gemm_params.input_base);
        HIGH_DEBUG(printf("[NN%d] Output data to be read at %d for descr at %lu.\n", m->id, descr->gemm_params.input_base, descr_offset));
        memcpy(output_data, output_addr, output_len);
    }
    HIGH_DEBUG(printf("[NN%d] Dequeued descr at %lu for module %s.\n", m->id, descr_offset, nn_module_get_name(m)));
    m->req_cnt++;
    #else
    nn_task_descr *descr_list = m->descr_list;
    bool init_done = false;
    while (descr_list != NULL) {
        switch(descr_list->prim) {
            case PRIM_GEMM: {
                // Find the appropriate GEMM accelerator
                bool accel_found = false;
                physical_accel_t *accel = m->accel_list;
                while (accel != NULL) {
                    if (accel->prim == PRIM_GEMM) {
                        accel_found = true;
                        break;
                    }
                    accel = accel->next;
                } 
                if (!accel_found) {
                    printf("[NN%d] No GEMM accelerator found during run!\n", m->id);
                    exit(1);
                }
                gemm_task_descr *descr = (gemm_task_descr *) descr_list;
                uint64_t descr_offset = descr->descr_offset[0];
                gemm_queue_entry_t *descr_entry = (gemm_queue_entry_t *) ((unsigned *) (m->mem) + descr_offset);
                HIGH_DEBUG(printf("[NN%d] Printing descr at %lu for req %d...\n", m->id, descr_offset, m->req_cnt);)
                HIGH_DEBUG(print_gemm_entry(descr_entry);)
                if (real_data && !init_done) {
                    nn_token_t *input_addr = (nn_token_t *) ((unsigned *) (m->mem) + descr_entry->gemm_params.input_base);
                    memcpy(input_addr, input_data, input_len);
                    init_done = true;
                }
                HIGH_DEBUG(printf("[NN%d] Programming PRIM_GEMM descr at %lu for req %d...\n", m->id, descr_offset, m->req_cnt););
                gemm_run(accel, descr_entry);
                m->active_cycles += accel->context_active_cycles[0];
            }
            default: break;
        }
        if (descr_list->next == NULL) {
            if (real_data) {
                gemm_task_descr *descr = (gemm_task_descr *) descr_list;
                uint64_t descr_offset = descr->descr_offset[0];
                gemm_queue_entry_t *descr_entry = (gemm_queue_entry_t *) ((unsigned *) (m->mem) + descr_offset);
                nn_token_t *output_addr = (nn_token_t *) ((unsigned *) (m->mem) + descr_entry->gemm_params.input_base);
                memcpy(output_data, output_addr, output_len);
            }
        }
        descr_list = descr_list->next;
    }
    m->req_cnt++;
    #endif
}

void nn_module_req(nn_module *m, nn_token_t *input_data, unsigned data_len, bool real_data) {
    HIGH_DEBUG(printf("[NN%d] Starting nn_module_req for %s\n", m->id, nn_module_get_name(m)));
    // Try to enqueue the first descriptor (for req_cnt_in) to the input_queue
    nn_task_descr *descr_list = m->descr_list;
    uint64_t descr_offset;
    sm_queue_t *in_q = m->input_queue;
    unsigned req = (m->req_cnt++) % SM_QUEUE_SIZE;
    switch(descr_list->prim) {
        case PRIM_GEMM: {
            gemm_task_descr *descr = (gemm_task_descr *) descr_list;
            descr_offset = descr->descr_offset[req];
        }    
        default: break;
    }
    HIGH_DEBUG(printf("[NN%d] Programming PRIM_GEMM descr at %lu for req %d...\n", m->id, descr_offset, req););
    while(sm_queue_full(in_q)) { SCHED_YIELD; }
    if (real_data) {
        nn_token_t *input_addr;
        // TODO: assumes it is a GEMM task
        gemm_queue_entry_t *descr = (gemm_queue_entry_t *) ((unsigned *) (m->mem) + descr_offset);
        input_addr = (nn_token_t *) ((unsigned *) (m->mem) + descr->gemm_params.input_base);
        memcpy(input_addr, input_data, data_len);
    }
    sm_queue_push(in_q, descr_offset);
    HIGH_DEBUG(printf("[NN%d] Enqueued descr to module %s.\n", m->id, nn_module_get_name(m)));
}

bool nn_module_req_check(nn_module *m, nn_token_t *input_data, unsigned data_len) {
    HIGH_DEBUG(printf("[NN%d] Starting nn_module_req_check for %s\n", m->id, nn_module_get_name(m)));
    sm_queue_t *in_q = m->input_queue;
    if (m->loop_around > 1) {
        unsigned level = sm_queue_level(in_q);
        HIGH_DEBUG(printf("[NN%d] Current input queue level = %d, pending requeues = %d\n", m->id, level, m->pending_requeues););
        if (level + m->pending_requeues >= SM_QUEUE_SIZE) return false; // backpressure
    } else if (sm_queue_full(in_q)) {
        return false;
    }
    // Try to enqueue the first descriptor (for req_cnt_in) to the input_queue
    nn_task_descr *descr_list = m->descr_list;
    uint64_t descr_offset;
    unsigned req = (m->req_cnt++) % SM_QUEUE_SIZE;
    switch(descr_list->prim) {
        case PRIM_GEMM: {
            gemm_task_descr *descr = (gemm_task_descr *) descr_list;
            descr_offset = descr->descr_offset[req];
        }    
        default: break;
    }
    HIGH_DEBUG(printf("[NN%d] Programming PRIM_GEMM descr at %lu for req %d...\n", m->id, descr_offset, req););
    if (m->loop_around > 1) {
        m->pending_requeues += (m->loop_around - 1); // reserve slots for requeues
        HIGH_DEBUG(printf("[NN%d] Reserved %d slots for module %s.\n", m->id, m->pending_requeues, nn_module_get_name(m)));
    }
    sm_queue_push(in_q, descr_offset);
    HIGH_DEBUG(printf("[NN%d] Enqueued descr to module %s.\n", m->id, nn_module_get_name(m)));
    return true;
}

void nn_module_rsp(nn_module *m, nn_token_t *output_data, unsigned data_len, bool real_data) {
    sm_queue_t *out_q = m->output_queue;
    uint64_t descr_offset;
    HIGH_DEBUG(printf("[NN%d] Dequeueing descr for module %s.\n", m->id, nn_module_get_name(m)));
    while(sm_queue_empty(out_q)) { SCHED_YIELD; }
    if (real_data) {
        nn_token_t *output_addr;
        uint64_t tail = out_q->tail;
        descr_offset = out_q->entry[tail % SM_QUEUE_SIZE];
        // TODO: assumes it is a GEMM task
        gemm_queue_entry_t *descr = (gemm_queue_entry_t *) ((unsigned *) (m->mem) + descr_offset);
        output_addr = (nn_token_t *) ((unsigned *) (m->mem) + descr->gemm_params.input_base);
        HIGH_DEBUG(printf("[NN%d] Output data to be read at %d for descr at %lu.\n", m->id, descr->gemm_params.input_base, descr_offset));
        memcpy(output_data, output_addr, data_len);
    }
    sm_queue_pop(out_q);
    HIGH_DEBUG(printf("[NN%d] Dequeued descr at %lu for module %s.\n", m->id, descr_offset, nn_module_get_name(m)));
}

bool nn_module_rsp_check(nn_module *m, nn_token_t *output_data, unsigned data_len) {
    HIGH_DEBUG(printf("[NN%d] Starting nn_module_rsp_check for %s\n", m->id, nn_module_get_name(m)));
    sm_queue_t *out_q = m->output_queue;
    if (sm_queue_empty(out_q)) return false;
    uint64_t descr_offset = sm_queue_pop(out_q);
    HIGH_DEBUG(printf("[NN%d] Dequeued descr for module %s.\n", m->id, nn_module_get_name(m)));
    gemm_queue_entry_t *descr = (gemm_queue_entry_t *) ((unsigned *) (m->mem) + descr_offset);
    if (descr->common.output_queue == 42424242) { // MAGIC number for last stage
        HIGH_DEBUG(printf("[NN%d] Found 42424242 for module %s.\n", m->id, nn_module_get_name(m)));
        return true;
    } else {
        sm_queue_t *in_q = m->input_queue;
        while(sm_queue_full(in_q)) { SCHED_YIELD; }
        sm_queue_push(in_q, descr_offset);
        if (m->pending_requeues != 0) {
            m->pending_requeues--;
            HIGH_DEBUG(printf("[NN%d] Released 1 reserved slot for module %s. Remaining = %d\n", m->id, nn_module_get_name(m), m->pending_requeues);)
        }
        return false;
    }
}

void nn_queue_push(nn_queue_t *q, nn_node_t *n) {
    nn_node_list *qnode = (nn_node_list *) malloc(sizeof(nn_node_list));
    qnode->n = n;
    qnode->next = NULL;
    if (q->tail) q->tail->next = qnode;
    else q->head = qnode;
    q->tail = qnode;
}

nn_node_t *nn_queue_pop(nn_queue_t *q) {
    if (!q->head) return NULL;
    nn_node_list *head = q->head;
    nn_node_t *out = head->n;
    q->head = head->next;
    if (!q->head) q->tail = NULL;
    free(head);
    return out;
}

bool nn_set_add_node(nn_node_list **s, nn_node_t *n) {
    nn_node_list *cur = *s;
    while(cur != NULL) {
        if (cur->n->id == n->id) {
            return false;
        }
        cur = cur->next;
    }
    nn_node_list *item = (nn_node_list *) malloc (sizeof(nn_node_list));
    item->n = n;
    item->next = *s;
    *s = item;
    return true;
}

void initialize_data(const char *input_file, nn_token_t *mem, unsigned len) {
    if (input_file[0] == '\n') {
        HIGH_DEBUG(printf("[NN] Initializing %d words with random input\n", len));
        const float float_min = 0.0f, float_max = 1.0f;
        for (unsigned i = 0; i < len; i++)
            mem[i] = nn_token_from_float(float_min + ((float)rand() / (float)RAND_MAX) * (float_max - float_min));
    } else {
        FILE *file = fopen(input_file, "r");

        HIGH_DEBUG(printf("[NN] Initializing %d words from %s\n", len, input_file));
        for (unsigned i = 0; i < len; i++) {
            float in;
            if (fscanf(file, "%f ", &in) != 1) {
                perror("Error reading input data file!");
                exit(1);
            }
            mem[i] = nn_token_from_float(in);

            #if 0
                if (i < 10) printf("%f ", in);
                if (i == 10) printf("\n");
            #endif
        }
        fclose(file);
    }
}

void nn_module_add_task_descr(nn_module *m, nn_task_descr *descr) {
    descr->next = NULL;
    if (!m->descr_list) {
        m->descr_list = descr;
        return;
    }
    nn_task_descr *p = m->descr_list;
    while (p->next) p = p->next;
    p->next = descr;
}

void print_descr_list(nn_module *m) {
    printf("[NN%d] Printing task descriptor for model %s...\n", m->id, nn_module_get_name(m));
    nn_task_descr *descr_list = m->descr_list;
    unsigned count = 0;
    while (descr_list != NULL) {
        printf("\t[D%d] prim=%s\n", count, hpthread_get_prim_name(descr_list->prim));
        switch(descr_list->prim) {
            case PRIM_GEMM: {
                gemm_task_descr *descr = (gemm_task_descr *) descr_list;
                for (int i = 0; i < SM_QUEUE_SIZE; i++) {
                    gemm_queue_entry_t *descr_entry = (gemm_queue_entry_t *) ((unsigned *) (m->mem) + descr->descr_offset[i]);
                    printf("\t[D%d] dim_m=%d\n", count, descr_entry->gemm_params.dim_m);
                    printf("\t[D%d] dim_n=%d\n", count, descr_entry->gemm_params.dim_n);
                    printf("\t[D%d] dim_k=%d\n", count, descr_entry->gemm_params.dim_k);
                    printf("\t[D%d] weight_base=%d\n", count, descr_entry->gemm_params.weight_base);
                    printf("\t[D%d] input_base=%d\n", count, descr_entry->gemm_params.input_base);
                    printf("\t[D%d] output_base=%d\n", count, descr_entry->gemm_params.output_base);
                    printf("\t[D%d] output_queue=%d\n", count, descr_entry->common.output_queue);
                    printf("\t[D%d] output_entry=%d\n", count, descr_entry->common.output_entry);
                }
            }
            default: break;
        }
        printf("\n");
        count++;
        descr_list = descr_list->next;
    }
}

void print_hpthread_list(nn_module *m) {
    printf("[NN%d] Printing hpthread list for model %s...\n", m->id, nn_module_get_name(m));
    nn_hpthread_list *th_list = m->th_list;
    unsigned count = 0;
    while (th_list != NULL) {
        hpthread_t *th = th_list->th;
        printf("\t[H%d] ID=%d\n", count, th->id);
        printf("\t[H%d] Prim=%s\n", count, hpthread_get_prim_name(hpthread_get_prim(th)));
        printf("\t[H%d] Name=%s\n", count, hpthread_get_name(th));
        printf("\t[H%d] Queue Ptr=%d\n", count, th->args->queue_ptr);
        printf("\n");
        count++;
        th_list = th_list->next;
    }
}

#ifndef ENABLE_VAM
bool nn_module_search_accel(nn_module *m, hpthread_prim_t prim) {
    // Search for all stratus accelerators and fill into accel_list
    HIGH_DEBUG(printf("[NN%d] Performing device probe for %s.\n", m->id, hpthread_get_prim_name(prim));)
    struct dirent *entry;
    bool accel_found = false;
    switch (prim) {
        case PRIM_GEMM: {
            // Open the devices directory to search for accels
            DIR *dir = opendir("/dev/");
            if (!dir) {
                perror("Failed to open directory");
                exit(1);
            }
            while ((entry = readdir(dir)) != NULL) {
                if (fnmatch("gemm_stratus.*", entry->d_name, FNM_NOESCAPE) == 0) {
                    HIGH_DEBUG(printf("[NN%d] Discovered device %s for %s\n", m->id, entry->d_name, nn_module_get_name(m));)
                    // Check if the accelerator is not already allocated to another thread
                    unsigned accel_idx;
                    sscanf(entry->d_name, "gemm_stratus.%u", &accel_idx);
                    // If the current accelerator is allocated, check next
                    if (!allocate_gemm_accel(accel_idx)) continue;
                    accel_found = true;
                    HIGH_DEBUG(printf("[NN%d] Allocated device %s for %s\n", m->id, entry->d_name, nn_module_get_name(m));)
                    // Else initialize this accelerator entry
                    physical_accel_t *accel_temp = (physical_accel_t *) malloc(sizeof(physical_accel_t));
                    accel_temp->accel_id = 0;
                    bitset_reset_all(accel_temp->valid_contexts);
                    for (int i = 0; i < MAX_CONTEXTS; i++) {
                        accel_temp->th[i] = NULL;
                        accel_temp->context_start_cycles[i] = 0;
                        accel_temp->context_active_cycles[i] = 0;
                        accel_temp->context_util[i] = 0.0;
                    }
                    strcpy(accel_temp->devname, entry->d_name);
                    accel_temp->init_done = false;
                    accel_temp->effective_util = 0.0;
                    accel_temp->prim = PRIM_GEMM;
                    __atomic_store_n(&accel_temp->accel_lock, 0, __ATOMIC_RELEASE);

                    char full_path[384];
                    snprintf(full_path, 384, "/dev/%s", entry->d_name);
                    accel_temp->fd = open(full_path, O_RDWR, 0);
                    if (accel_temp->fd < 0) {
                        fprintf(stderr, "Error: cannot open %s", full_path);
                        exit(EXIT_FAILURE);
                    }
                    gemm_init(accel_temp, m->mem);
                    // Add the accelerator the module's accel list
                    accel_temp->next = m->accel_list;
	                m->accel_list = accel_temp;
                    break; // Found the accel; can break
                }
            }
            closedir(dir);
        }
        break;
        default: break;
    }
    return accel_found;
}
#endif
