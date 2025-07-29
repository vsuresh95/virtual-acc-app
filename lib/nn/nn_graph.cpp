#include <nn_graph.h>

/////////////////////////////////////////
// NN graph
nn_graph_t::nn_graph_t() {
    entry = new nn_node_t(0, nn::NONE);
    exit = new nn_node_t(-1, nn::NONE);
    add_nn_node(entry);
    add_nn_node(exit);
}

void nn_graph_t::add_nn_node(nn_node_t *node) {
    if (std::find(nn_node_list.begin(), nn_node_list.end(), node) == nn_node_list.end())
        nn_node_list.push_back(node);
}

nn_node_t *nn_graph_t::get_nn_node(int id) {
    auto it = std::find_if(
        nn_node_list.begin(), nn_node_list.end(),
        [id](const nn_node_t *node) { return node->id == id; }
    );

    return *it;
}

void nn_graph_t::add_nn_edge(nn_edge_t *edge) {
    // Add the edge to the edge list
    if (std::find(nn_edge_list.begin(), nn_edge_list.end(), edge) == nn_edge_list.end())
        nn_edge_list.push_back(edge);

    // Add the consumer and edges to the respective nodes also
    edge->get_source()->add_consumer(edge->get_destination());
    edge->get_source()->add_out_edge(edge);
    edge->get_destination()->add_in_edge(edge);
}

void nn_graph_t::dump() {
    printf("\nPrinting nodes of graph %s:\n", get_name());
    for (auto node : nn_node_list) node->dump();
    printf("\nPrinting edges of graph %s:\n", get_name());
    for (auto edge : nn_edge_list) edge->dump();
}

/////////////////////////////////////////
// NN node
void nn_node_t::dump() {
    printf("\tNode operator: %s(%d) Consumers: ", dump_op(), id);
    for (auto cons : consumers) printf("%s(%d), ", cons->dump_op(), cons->id);
    printf("\n");
}

// Helper function for printing NN operator
// -- declared in nn_operators.h
const char *get_op_name(nn::operator_t p) {
    switch(p) {
        case nn::NONE : return (const char *) "NONE";
        case nn::GEMM : return (const char *) "GEMM";
        default: return (const char *) "Unknown operand";
    }
}
