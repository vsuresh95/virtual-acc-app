#include <stdlib.h>
#include <nn_graph.h>

void nn_graph_create(nn_graph_t *g) {
    g->nodes = NULL;
    g->entry = (nn_node_t *) malloc (sizeof(nn_node_t));
    nn_node_create(g->entry, 0, NN_OP_NONE);
    g->entry->is_entry = true;
    nn_graph_add_nn_node(g, g->entry);
    g->exit = (nn_node_t *) malloc (sizeof(nn_node_t));
    nn_node_create(g->exit, -1, NN_OP_NONE);
    g->exit->is_exit = true;
    nn_graph_add_nn_node(g, g->exit);
}

void nn_graph_add_nn_node(nn_graph_t *g, nn_node_t *n) {
    nn_node_list *item = (nn_node_list *) malloc (sizeof(nn_node_list));
    item->n = n;
    item->next = g->nodes;
    g->nodes = item;
}

nn_node_t *nn_graph_get_nn_node(nn_graph_t *g, int id) {
    nn_node_list *cur = g->nodes;
    while(cur != NULL) {
        if (cur->n->id == id) {
            return cur->n;
        }
        cur = cur->next;
    }
    return NULL;
}

void nn_graph_dump(nn_graph_t *g) {
    printf("\nPrinting nodes of graph %s:\n", nn_graph_get_name(g));
    nn_node_list *cur = g->nodes;
    while(cur != NULL) {
        nn_node_dump(cur->n);
        cur = cur->next;
    }
}

void nn_graph_delete(nn_graph_t *g) {
    nn_node_list *cur = g->nodes;
    while(cur != NULL) {
        nn_node_list *next = cur->next;
        if (cur->n) nn_node_delete(cur->n);
        free(cur);
        cur = next;
    }
    free(g);
}

void nn_node_create(nn_node_t *n, int i, unsigned op) { 
    n->id = i;
    n->nn_op = op;
    n->in_edges = NULL;
    n->out_edges = NULL;
    n->is_entry = false;
    n->is_exit = false;
    n->th = NULL;
}

void nn_node_add_in_edge(nn_node_t *n, nn_edge_t *e) {
    nn_edge_list *item = (nn_edge_list *) malloc (sizeof(nn_edge_list));
    item->e = e;
    item->next = n->in_edges;
    n->in_edges = item;
}

void nn_node_add_out_edge(nn_node_t *n, nn_edge_t *e) {
    nn_edge_list *item = (nn_edge_list *) malloc (sizeof(nn_edge_list));
    item->e = e;
    item->next = n->out_edges;
    n->out_edges = item;
}

const char *nn_node_dump_op(nn_node_t *n) {
    switch(n->nn_op) {
        case NN_OP_NONE : return (const char *) "NONE";
        case NN_OP_GEMM : return (const char *) "GEMM";
        default: return (const char *) "Unknown operand";
    }
}

void nn_node_dump(nn_node_t *n) {
    printf("\tNode operator: %s(%d) Consumers: ", nn_node_dump_op(n), n->id);
    nn_edge_list *cur = n->out_edges;
    while(cur != NULL) {
        nn_node_t *cons = nn_edge_get_destination(cur->e);
        printf("%s(%d), ", nn_node_dump_op(cons), cons->id);
        cur = cur->next;
    }
    printf("\n");
}

void nn_node_delete(nn_node_t *n) {
    nn_edge_list *cur = n->out_edges;
    while(cur != NULL) {
        nn_edge_list *next = cur->next;
        if (cur->e) nn_edge_delete(cur->e);
        free(cur);
        cur = next;
    }
    free(n);
}

void nn_edge_dump(nn_edge_t *e) {
    printf("\tEdge in: %s(%d), out: %s(%d)\n", nn_node_dump_op(e->src), e->src->id, nn_node_dump_op(e->dst), e->dst->id);
}
