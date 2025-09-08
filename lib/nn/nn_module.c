#include <stdlib.h>
#include <nn_module.h>
#include <gemm_node_args.h>
#include <string.h>
#include <libesp.h>

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
    const unsigned mem_size = 2 * 1024 * 1024; // 2MB
    m->mem = esp_alloc(mem_size);
    m->mem_allocated = 0;

	char in_line_buf[256]; // Max 256 characters in one line

    if (fgets(in_line_buf, 100, model_def) != NULL) {
        if ((strlen(in_line_buf) > 0) && (in_line_buf[strlen (in_line_buf) - 1] == '\n')) {
            in_line_buf[strlen(in_line_buf) - 1] = '\0';
        }

	    char module_name[50];
        sscanf(in_line_buf, "%s %d", module_name, &m->nprio);
        snprintf(m->graph->name, 100, "%s.%d", module_name, m->id);

	    HIGH_DEBUG(printf("[NN] MODEL NAME = %s\n", nn_module_get_name(m)));
    }

    while (fgets(in_line_buf, 256, model_def) != NULL) {

        if ((strlen(in_line_buf) > 0) && (in_line_buf[strlen (in_line_buf) - 1] == '\n')) {
            in_line_buf[strlen(in_line_buf) - 1] = '\0';
        }

	    HIGH_DEBUG(printf("[NN] IN LINE = %s\n", in_line_buf));

        // Check first letter of trace line
        char type = in_line_buf[0];
        switch(type) {
            case 'N': { // Node
                int node_id;
                unsigned nn_op;
                sscanf(in_line_buf+2, "%d %d", &node_id, &nn_op);
                switch(nn_op) {
                    case NN_OP_GEMM: { // GEMM operator
                        // Add a new GEMM node to the graph
                        nn_node_t *gemm_node = (nn_node_t *) malloc (sizeof(nn_node_t));
                        nn_node_create(gemm_node, node_id, NN_OP_GEMM);
                        // Read the parameters of the GEMM from the following chars
                        gemm_node_args *args = (gemm_node_args *) malloc (sizeof(gemm_node_args));
                        gemm_params_t *params= &(args->params);
                        if (sscanf(in_line_buf+5, "%d %d %d %s", &params->dim_m, &params->dim_n, &params->dim_k, args->input_file) == 3) {
                            // If no input file was provided, the pointer is marked invalid.
                            args->input_file[0] = '\n'; args->input_file[1] = '\0';
                        } 
                        gemm_node->args = (void *) args;
                        // Add the created node to the graph for the module
                        nn_graph_add_nn_node(m->graph, gemm_node);
                        snprintf(gemm_node->name, 120, "%s.gemm.%d", m->graph->name, node_id);
                        HIGH_DEBUG(
                            printf("\t[NN] Adding node %s...", nn_node_get_name(gemm_node));
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
                    printf("\t[NN] Adding edge...");
                    nn_edge_dump(e);
                )
                break;
            }
            break;
            case '#': // Comment
                break;
            default:
                printf("[APP] Bad character read at 0!");
                break;
        }
    }

    HIGH_DEBUG(
        nn_graph_dump(m->graph);
    )
}

// Register the model with NN frontend
void nn_module_register(nn_module *m) {
    // We will maintain a queue of nodes to-be-visited in BFS order and a list of visited nodes
    nn_queue_t *q = (nn_queue_t *) malloc (sizeof(nn_queue_t));
    q->head = q->tail = NULL;
    nn_node_list *visited = NULL;
    m->th_list = NULL;

    // Do BFS traversal of the routine DFG by starting at the entry node
    nn_node_t *entry = nn_graph_get_entry(m->graph);
    
    // Add entry to queue and set
    nn_queue_push(q, entry);
    nn_set_add_node(&visited, entry);

    LOW_DEBUG(printf("[NN FE] Parsing NN graph for %s\n", nn_module_get_name(m)));

    while (q->head != NULL) { // !empty
        nn_node_t *current = nn_queue_pop(q);
        if (!current) break;

        HIGH_DEBUG(printf("[NN FE] Found node %s(%d)\n", nn_node_dump_op(current), current->id));

        // Iterate through all out edges of this node and add the destinations to the visitor set and the BFS queue.
        nn_edge_list *cons = current->out_edges;
        while(cons != NULL) {
            // Check that the consumer was not visited earlier and push into the queue.
            nn_edge_t *out = cons->e;
            nn_node_t *dst = nn_edge_get_destination(out);
            if (nn_set_add_node(&visited, dst)) {
                nn_queue_push(q, dst);
            }
            
            // Allocate memory for the edge and store the offset in the args
            nn_edge_args *args = out->args;
            size_t buf_len = args->len + PAYLOAD_OFFSET/sizeof(nn_token_t);
            args->offset = m->mem_allocated; m->mem_allocated += buf_len;
            // Zero out the sync flag
            unsigned *sync = ((unsigned *) (m->mem)) + args->offset;
            sync[0] = 0; sync[1] = 0;
            cons = cons->next;
        }

        // Ensure that the current node is an entry or exit
        unsigned nn_op = nn_node_get_op(current);
        if (nn_op != NN_OP_NONE) {
            switch(nn_op) {
                case NN_OP_GEMM: {
                    gemm_node_args *gemm_args = (gemm_node_args *) current->args;
                    gemm_params_t *params = &(gemm_args->params);
                    // Allocate memory for weights
                    params->weight_base = m->mem_allocated; m->mem_allocated += params->dim_n * params->dim_k; 
                    // Retrieve input and output offsets from its first edges
                    nn_edge_list *cur; nn_edge_t *edge = NULL;
                    for (cur = current->in_edges; cur; cur = cur->next) {
                        edge = cur->e;
                    } params->input_base = edge->args->offset;
                    for (cur = current->out_edges; cur; cur = cur->next) {
                        edge = cur->e;
                    } params->output_base = edge->args->offset;

                    // Initialize the weights if present
                    nn_token_t *wgt_address = ((nn_token_t *) m->mem) + params->weight_base;
                    initialize_data(gemm_args->input_file, wgt_address, params->dim_n * params->dim_k);

                    // Create 1 GEMM descriptor
                    unsigned gemm_descr = m->mem_allocated; m->mem_allocated += ACCEL_PARAM_SIZE;
                    create_gemm_descr(((unsigned *) m->mem) + gemm_descr, params);
                    // Create 1 JUMP descriptor 
                    unsigned jump_descr = m->mem_allocated; m->mem_allocated += ACCEL_PARAM_SIZE;
                    create_jump_descr(((unsigned *) m->mem) + jump_descr, gemm_descr);
                    HIGH_DEBUG(
                        printf("[APP] Printing GEMM descriptor...\n");
                        print_descr(((unsigned *) m->mem) + gemm_descr);
                        printf("[APP] Printing JUMP descriptor...\n");
                        print_descr(((unsigned *) m->mem) + jump_descr);
                    )
                    // Create base stat descriptor
                    unsigned stat_descr = m->mem_allocated; m->mem_allocated += 2; // stat descriptor is 2 words
                    set_context_avail(((unsigned *) m->mem) + stat_descr, gemm_descr);

                    // Create the hpthread general arguments
                    hpthread_args_t *args = (hpthread_args_t *) malloc(sizeof(hpthread_args_t));
                    args->mem = m->mem;
                    args->base_ptr = stat_descr;

                    // Declare hpthread and assign attributes
                    hpthread_t *th = (hpthread_t *) malloc(sizeof(hpthread_t));
                    hpthread_init(th, m->id + 1);
                    hpthread_setargs(th, args);
                    hpthread_setname(th, nn_node_get_name(current));
                    hpthread_setprimitive(th, PRIM_GEMM);
                    hpthread_setpriority(th, m->nprio);

                    HIGH_DEBUG(printf("[NN] Before hpthread create request...\n"));

                    // Create a hpthread
                    hpthread_create(th);

                    // Assign the thread to the model mapping
                    current->th = th;
                    nn_module_add_hpthread(m, th);
                    break;
                }
                default: break;
            }
        }
    }

    // Calculate the input and output flag addresses for this module
    nn_node_t *exit = nn_graph_get_exit(m->graph);
    nn_edge_list *cur; nn_edge_t *edge = NULL;
    for (cur = exit->in_edges; cur; cur = cur->next) {
        edge = cur->e;
    } m->output_flag_offset = edge->args->offset;
    HIGH_DEBUG(printf("[NN] output_flag_offset for model %s = 0x%x\n", nn_module_get_name(m), m->output_flag_offset);)
    for (cur = entry->out_edges; cur; cur = cur->next) {
        edge = cur->e;
    } m->input_flag_offset = edge->args->offset;
    HIGH_DEBUG(printf("[NN] input_flag_offset for model %s = 0x%x\n", nn_module_get_name(m), m->input_flag_offset);)

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
    nn_hpthread_list *cur = m->th_list;
    while(cur != NULL) {
        nn_hpthread_list *next = cur->next;
        hpthread_join(cur->th);
        free(cur->th->args);
        free(cur->th);
        free(cur);
        cur = next;
    }
    esp_free(m->mem);
    nn_graph_delete(m->graph);
}

void nn_module_add_hpthread(nn_module *m, hpthread_t *th) {
    nn_hpthread_list *item = (nn_hpthread_list *) malloc (sizeof(nn_hpthread_list));
    item->th = th;
    item->next = m->th_list;
    m->th_list = item;
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
        HIGH_DEBUG(printf("[NN FE] Initializing %d words with random input\n", len));
        const float float_min = 0.0f, float_max = 1.0f;
        for (unsigned i = 0; i < len; i++)
            mem[i] = nn_token_from_float(float_min + ((float)rand() / (float)RAND_MAX) * (float_max - float_min));
    } else {
        FILE *file = fopen(input_file, "r");

        HIGH_DEBUG(printf("[NN FE] Initializing %d words from %s\n", len, input_file));
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
