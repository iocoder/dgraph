#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "proto.h"

FILE *debug;
int unit_id;
int unit_count;

typedef struct edge_str {
    struct edge_str *next;
    int dest;
} edge_t;

typedef struct node_str {
    struct node_str *next;
    int id;
    struct edge_str *edges;
} node_t;

node_t *nodes = NULL;

node_t *get_node(int id) {
    node_t *ptr = nodes;
    while (ptr && ptr->id != id && (ptr=ptr->next));
    return ptr;
}

void print_subgraph() {
    node_t *curnode = nodes;
    edge_t *curedge;
    fprintf(debug, "CURRENT GRAPH\n");
    fprintf(debug, "--------------\n");
    while (curnode) {
        fprintf(debug, "node %d: ", curnode->id);
        curedge = curnode->edges;
        while (curedge) {
            fprintf(debug, "%d ", curedge->dest);
            curedge = curedge->next;
        }
        fprintf(debug, "\n");
        curnode = curnode->next;
    }
    fprintf(debug, "---------------------------------------\n");
}

int add_edge(int node1, int node2) {
    node_t *node = get_node(node1);
    edge_t *edge = malloc(sizeof(edge_t));
    if (!edge) {
        /* EMEM */
        return 0;
    }
    /* src node doesn't exist */
    if (!node) {
        /* create src node */
        if (!(node = malloc(sizeof(node_t)))) {
            /* EMEM */
            free(edge);
            return 0;
        }
        node->next = nodes;
        node->id = node1;
        node->edges = NULL;
        nodes = node;
    }
    /* add edge */
    edge->next = node->edges;
    edge->dest = node2;
    node->edges = edge;
    /* debug */
    fprintf(debug, "added %d->%d\n", node1, node2);
    print_subgraph();
    /* done */
    return 1;
}

int rem_edge(int node1, int node2) {
    node_t *node = get_node(node1);
    edge_t *edge, *prev;
    if (!node) {
        /* src node doesn't exist */
        return 0;
    }
    /* loop over edges */
    prev = NULL;
    edge = node->edges;
    while (edge) {
        if (edge->dest == node2) {
            /* edge found */
            if (prev) {
                prev->next = edge->next;
            } else {
                node->edges = edge->next;
            }
            /* debug */
            fprintf(debug, "removed %d->%d\n", node1, node2);
            print_subgraph();
            /* done */
            return 1;
        }
        edge = (prev = edge)->next;
    }
    /* edge not found */
    return 0;
}

void exit_unit(int sig) {
    /* safe exit */
    fclose(debug);
    exit(0);
}

char *__add_edge(char *input) {
    int *ret = malloc(sizeof(int));
    *ret = add_edge(((pars_t *)input)->first, ((pars_t *)input)->second);
    return (char *) ret;
}

char *__rem_edge(char *input) {
    int *ret = malloc(sizeof(int));
    *ret = rem_edge(((pars_t *)input)->first, ((pars_t *)input)->second);
    return (char *) ret;
}

char *__init_dist(char *input) {
    int *ret = malloc(sizeof(int));
    *ret = init_dist(*(int *)input);
    return (char *) ret;
}

char *__update_dist(char *input) {
    int *ret = malloc(sizeof(int));
    *ret = update_dist(((pars_t *)input)->first, ((pars_t *)input)->second);
    return (char *) ret;
}

char *__bellmanford_phase(char *input) {
    int *ret = malloc(sizeof(int));
    *ret = bellmanford_phase();
    return (char *) ret;
}

char *__get_dist(char *input) {
    int *ret = malloc(sizeof(int));
    *ret = get_dist(*(int *)input);
    return (char *) ret;
}

int main(int argc, char *argv[]) {
    char fname[100];
    /* get id */
    if (argc != 3) {
        fprintf(stderr, "Error: dgraph unit: invalid arguments\n");
        return -1;
    }
    /* extract parameters */
    unit_id = atoi(argv[1]);
    unit_count = atoi(argv[2]);
    /* open debug file */
    sprintf(fname, "/var/log/dgraph/unit%d", unit_id);
    debug = fopen(fname, "w");
    if (!debug) {
        fprintf(stderr, "Error: dgraph unit: can't open %s\n", fname);
        return -1;
    }
    /* register signal handler */
    signal(SIGINT, exit_unit);
    /* register rpc routines */
    registerrpc(PRGBASE+unit_id, PRGVERS, ADDEDGE,
                __add_edge, (xdrproc_t) xdr_pars, (xdrproc_t) xdr_ret);
    registerrpc(PRGBASE+unit_id, PRGVERS, REMEDGE,
                __rem_edge, (xdrproc_t) xdr_pars, (xdrproc_t) xdr_ret);
    /* output something */
    fprintf(debug, "Unit %d started.\n", unit_id);
    /* run server */
    svc_run();
}
