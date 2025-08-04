#include <nn_frontend.h>

// External instance of NN FE interface
extern nn_intf_t nn_intf;

// Used in wakeup_vam().
std::thread nn_fe_th;

// Function to wake up VAM for the first time
void wakeup_nn_fe() {
	HIGH_DEBUG(printf("[NN FE] Launching a new thread for NN FRONTEND!\n");)

    // Create an object of VAM and launch a std::thread for VAM
    nn_frontend *nn_fe = new nn_frontend;
    nn_fe_th = std::thread(&nn_frontend::run_frontend, nn_fe);
    nn_fe_th.detach();
}

void nn_frontend::run_frontend() {
	HIGH_DEBUG(printf("[NN FE] Hello from NN FRONTEND!\n");)

    uint64_t start_time = get_counter();
    uint64_t nn_fe_sleep = NN_SLEEP_MIN;

    // Run loop will run forever
    while (1) {
        // Test the interface state
        nn::fe_state_t state = nn_intf.test();

        switch(state) {
            case nn::IDLE: {
                if (get_counter() - start_time > nn_fe_sleep) {
					if (nn_fe_sleep < NN_SLEEP_MAX) nn_fe_sleep += NN_SLEEP_MIN;
				}
                sched_yield();
                break;
            }
            case nn::REGISTER: {
                HIGH_DEBUG(printf("[NN FE] Received a request for registering model %s\n", nn_intf.m->get_name());)

                parse_nn_graph(nn_intf.m);

                // Set the interface state to DONE
                nn_intf.set(nn::DONE);

                HIGH_DEBUG(printf("[NN FE] Completed the processing of request.\n");)

                // Reset trigger delay after servicing a request
                start_time = get_counter();
                nn_fe_sleep = NN_SLEEP_MIN;
                break;
            }
            case nn::RELEASE: {
                HIGH_DEBUG(printf("[NN FE] Received a request for releasing model %s\n", nn_intf.m->get_name());)

                // Join all the hpthreads for this model
                for (auto &hpthread_map : model_hpthread_mapping[nn_intf.m]) {
                    hpthread_t *th = hpthread_map.second;
                    th->join();
                }

                // Set the interface state to DONE
                nn_intf.set(nn::DONE);

                HIGH_DEBUG(printf("[NN FE] Completed the processing of request.\n");)

                // Reset trigger delay after servicing a request
                start_time = get_counter();
                nn_fe_sleep = NN_SLEEP_MIN;
                break;
            }
            default:
                break;
        }
    }
}
	
void nn_frontend::parse_nn_graph(nn_module *m) {
    // We will maintain a queue of nodes to-be-visited in BFS order and a list of visited nodes
    std::queue<nn_node_t *> q;
    std::unordered_set<nn_node_t *> visited;

    // Do BFS traversal of the routine DFG by starting at the entry node
    nn_node_t *entry = m->graph->get_entry();;
    q.push(entry);
    visited.insert(entry);

    LOW_DEBUG(printf("[NN FE] Parsing NN graph for %s\n", m->get_name()));

    while (!q.empty()) {
        nn_node_t *current = q.front();
        q.pop();

        HIGH_DEBUG(printf("[NN FE] Found node %s(%d)\n", current->dump_op(), current->id));

        // Iterate through all out edges of this node and add the destinations to the visitor set and the BFS queue.
        for (nn_edge_t *out : current->out_edges) {
            // Check that the consumer was not visited earlier and push into the queue.
            if (visited.insert(out->get_destination()).second) {
                q.push(out->get_destination());
            }
            
            // Allocate memory for the edge and store the offset in the args
            size_t buf_len = out->args->len + PAYLOAD_OFFSET/sizeof(nn_token_t);
            out->args->offset = m->mem->alloc(buf_len);
            // Zero out the sync flag
            nn_token_t *sync = ((nn_token_t *) (m->mem->hw_buf)) + out->args->offset;
            sync[0] = 0; sync[1] = 0;
        }

        // Ensure that the current node is an entry or exit
        if (current->get_op() != nn::NONE) {
            switch(current->get_op()) {
                case nn::GEMM: {
                    gemm_node_args *args = (gemm_node_args *) current->args;
                    args->mem = m->mem->hw_buf;
                    args->weight_base = m->mem->alloc(args->dim_n * args->dim_k); 
                    // Retrieve input and output offsets from its edges
                    args->input_base = current->in_edges[0]->args->offset;
                    args->output_base = current->out_edges[0]->args->offset;

                    // Initialize the weights if present
                    initialize_data(args->input_file, 
                                    ((nn_token_t *) m->mem->hw_buf) + args->weight_base,
                                    args->dim_n * args->dim_k);

                    // Declare hpthread and assign attributes
                    current->th = new hpthread_t;
                    // TODO th->setroutine(sw_gemm);
                    current->th->setargs(current->args);
                    current->th->attr_init();
                    current->th->attr_setprimitive(hpthread_prim_t::GEMM);
                    current->th->attr_setname(current->get_name());
                    current->th->attr_setpriority(m->nprio);
                    current->th->user_id = m->id + 1;

                    HIGH_DEBUG(printf("[NN FE] Before hpthread create request...\n"));

                    // Create a hpthread
                    if (current->th->create()) {
                        perror("error in hpthread_create!");
                        exit(1);
                    }

                    // Assign the thread to the model mapping
                    model_hpthread_mapping[m][current] = current->th;
                    break;
                }
                default: break;
            }
        }
    }

    // Calculate the input and output flag addresses for this module
    nn_token_t *in = ((nn_token_t *) (m->mem->hw_buf)) + m->graph->get_entry()->out_edges[0]->args->offset;
    nn_token_t *out = ((nn_token_t *) (m->mem->hw_buf)) + m->graph->get_exit()->in_edges[0]->args->offset;
    m->input_flag.flag = (volatile uint64_t *) in;
    m->output_flag.flag = (volatile uint64_t *) out;
    m->input_data = in + PAYLOAD_OFFSET/sizeof(nn_token_t);
    m->output_data = out + PAYLOAD_OFFSET/sizeof(nn_token_t);
}