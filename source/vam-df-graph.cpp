#include <common-defs.hpp>
#include <vam-mem-helper.hpp>
#include <vam-df-graph.hpp>

// Function definitions for VAM DF graph

df_graph_t::df_graph_t(df_int_node_t *p) {
    parent = p;
    entry = new df_node_t(NONE, parent, false);
    exit = new df_node_t(NONE, parent, false);
    add_df_node(entry);
    add_df_node(exit);
}

df_int_node_t *df_node_t::get_root() {
    df_int_node_t *p = this->get_parent();
    if (p->is_root()) {
        return p;
    } else {
        return p->get_root();
    }
}

void df_int_node_t::add_df_edge(df_edge_t *edge) {
    child_graph->add_df_edge(edge);
    // Add the consumer and edges to the respective nodes also
    edge->get_source()->add_consumer(edge->get_destination());
    edge->get_source()->add_out_edge(edge);
    edge->get_destination()->add_in_edge(edge);
}

void df_graph_t::dump() {
    printf("\n\tPrinting out nodes of graph of %s:\n", parent->get_name());
    for (auto node : df_node_list) {
        if (node->is_internal()) ((df_int_node_t *) node)->dump();
        else node->dump();
    }
    printf("\n\tPrinting out edges of graph of %s:\n", parent->get_name());
    for (auto edge : df_edge_list) edge->dump();
}

void df_node_t::dump() {
    printf("\tNode primitive: %s, Consumers: ", dump_prim());
    for (auto cons : consumers) printf("%s, ", cons->dump_prim());
    printf("\n\tIncoming Edges: ");
    for (auto edge : in_edges) printf("%s->%s ", edge->get_source()->dump_prim(), edge->get_destination()->dump_prim());
    printf("\n\tOutgoing Edges: ");
    for (auto edge : out_edges) printf("%s->%s ", edge->get_source()->dump_prim(), edge->get_destination()->dump_prim());
    printf("\n");
}