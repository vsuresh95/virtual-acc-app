#include <nn_module.h>
#include <nn_intf.h>

// External instance of NN FE interface
extern nn_intf_t nn_intf;

// Helper function to wake up NN FE, if not started already
extern void wakeup_nn_fe();

// Register a model using a description in a txt model_def
void nn_module::load(const char *n) {
    // Check if the model description file exists
    FILE *model_def = fopen(n, "r");
	if (!model_def) {
		perror("Failed to read model description file");
		exit(1);
	}

    // Create an NN computational graph for this model
    graph = new nn_graph_t;
    mem = new mem_pool_t;

	char in_line_buf[256]; // Max 256 characters in one line

    if (fgets(in_line_buf, 100, model_def) != NULL) {
        if ((strlen(in_line_buf) > 0) && (in_line_buf[strlen (in_line_buf) - 1] == '\n')) {
            in_line_buf[strlen(in_line_buf) - 1] = '\0';
        }

	    char module_name[50];
        sscanf(in_line_buf, "%s", module_name);
        snprintf(graph->name, 100, "%s.%d", module_name, id);

	    HIGH_DEBUG(printf("[NN] MODEL NAME = %s\n", get_name()));
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
                int node_id, temp;
                nn::operator_t op;
                sscanf(in_line_buf+2, "%d %d", &node_id, &temp);
                op = static_cast<nn::operator_t>(temp); // for safe assignment
                switch(op) {
                    case nn::GEMM: { // GEMM operator
                        // Add a new GEMM node to the graph
                        nn_node_t *gemm_node = new nn_node_t(node_id, nn::GEMM);
                        // Read the parameters of the GEMM from the following chars
                        gemm_node_args *args = new gemm_node_args;
                        if (sscanf(in_line_buf+5, "%d %d %d %s", &args->dim_m, &args->dim_n, &args->dim_k, args->input_file) == 3) {
                            // If no input file was provided, the pointer is marked invalid.
                            args->input_file[0] = '\n'; args->input_file[1] = '\0';
                        } 
                        gemm_node->args = args;
                        graph->add_nn_node(gemm_node);
                        snprintf(gemm_node->name, 120, "%s.gemm.%d", graph->name, node_id);
                        HIGH_DEBUG(
                            printf("\t[NN] Adding node %s...", gemm_node->get_name());
                            gemm_node->dump();
                        )
                        break;
                    }
                    case nn::NONE: {
                        nn_node_t *none_node = new nn_node_t(node_id, nn::NONE);
                        graph->add_nn_node(none_node);
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
                nn_node_t *src = graph->get_nn_node(src_id);
                nn_node_t *dst= graph->get_nn_node(dst_id);
                nn_edge_t *e = new nn_edge_t(src, dst);
                e->args = new nn_edge_args;
                e->args->len = len;
                graph->add_nn_edge(e);
                HIGH_DEBUG(
                    printf("\t[NN] Adding edge...");
                    e->dump();
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
        graph->dump();
    )
}

// Register the model with NN frontend
void nn_module::_register() {
	HIGH_DEBUG(printf("[NN] Requested registration of model %s.\n", this->get_name());)

    // If nn_fe has not yet been started (i.e., interface is in nn::RESET, start one thread now)
	if (nn_intf.swap(nn::RESET, nn::WAKEUP)) {
		// Only one thread should enter here; remaining will wait for idle state
		wakeup_nn_fe();
		nn_intf.set(nn::IDLE);
    }

	// Check if the interface is IDLE. If yes, swap to BUSY. If not, block until it is
	while (!nn_intf.swap(nn::IDLE, nn::BUSY)) sched_yield();

	// Write the nn request to the interface
	nn_intf.m = this;

	// Set the interface state to CREATE
	nn_intf.set(nn::REGISTER);

	// Block until the request is complete (interface state is DONE), then swap to IDLE
	while (!nn_intf.swap(nn::DONE, nn::IDLE)) sched_yield();

	HIGH_DEBUG(printf("[NN] Registered model %s.\n", this->get_name());)
}

// Helper: Load and register a model in one call
void nn_module::load_and_register(const char *n) {
    load(n);
    _register();
}

// Release the resources for this model
void nn_module::release() {
	HIGH_DEBUG(printf("[NN] Requested release of model %s.\n", this->get_name());)

	// Check if the interface is IDLE. If yes, swap to BUSY. If not, block until it is
	while (!nn_intf.swap(nn::IDLE, nn::BUSY)) sched_yield();

	// Write the nn request to the interface
	nn_intf.m = this;

	// Set the interface state to CREATE
	nn_intf.set(nn::RELEASE);

	// Block until the request is complete (interface state is DONE), then swap to IDLE
	while (!nn_intf.swap(nn::DONE, nn::IDLE)) sched_yield();

	HIGH_DEBUG(printf("[NN] Released model %s.\n", this->get_name());)
}
