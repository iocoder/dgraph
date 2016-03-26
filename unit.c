#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "proto.h"

FILE *debug;
int unit_id;
int unit_count;
char **unit_ip;
int relaxed;
int MAX_SIZE = 100;

typedef struct edge_str {
    struct edge_str *next;
    int dest;
} edge_t;

typedef struct node_str {
    struct node_str *next;
    int id;
    int dist_from_src;
    struct edge_str *edges;
} node_t;

node_t **nodes = NULL;

int node_to_unit(int id) {
    return (id % unit_count);
}

int node_to_index(int id) {
    return (id/unit_count)%MAX_SIZE;
}

node_t *get_node_list(int id, node_t *list) {
    node_t *ptr = list;
    while (ptr && ptr->id != id && (ptr=ptr->next));
    return ptr;
}

node_t *get_node(int id) {
    int index = node_to_index(id);
    node_t *list = nodes[index];
    node_t *ret = get_node_list(id, list);
    return ret;
}

void print_list(node_t *list) {
    node_t *curnode = list;
    edge_t *curedge;
    while (curnode) {
        fprintf(debug, "node %d (%d): ", curnode->id, curnode->dist_from_src);
        curedge = curnode->edges;
        while (curedge) {
            fprintf(debug, "%d ", curedge->dest);
            curedge = curedge->next;
        }
        fprintf(debug, "\n");
        curnode = curnode->next;
    }
}

void print_subgraph() {
    fprintf(debug, "CURRENT GRAPH\n");
    fprintf(debug, "--------------\n");
    int i;
    for(i=0; i<MAX_SIZE; i++) {
        fprintf(debug, "hash: %d\n", i);
        print_list(nodes[i]);
    }
    fprintf(debug, "---------------------------------------\n");
}

int add_edge(int node1, int node2) {
    node_t *node = get_node(node1);
    edge_t *edge;
    /* src node doesn't exist */
    if (!node) {
        /* create src node */
        if (!(node = malloc(sizeof(node_t))))
            return 0; /* EMEM */
        int index = node_to_index(node1);
        node_t *list = nodes[index];
        node->next = list;
        node->id = node1;
        node->dist_from_src = INF;
        node->edges = NULL;
        list = node;
    }
    /* add edge? */
    if (node2 != -1) {
        /* allocate edge */
        if (!(edge = malloc(sizeof(edge_t))))
            return 0; /* EMEM */
        /* set struct members */
        edge->next = node->edges;
        edge->dest = node2;
        node->edges = edge;
        /* debug */
        fprintf(debug, "added %d->%d\n", node1, node2);
        print_subgraph();
    }
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

int init_dist(int src_id) {
    node_t *node = nodes;
    while (node) {
        if(node->id == src_id)
            node->dist_from_src = 0;
        else
            node->dist_from_src = INF;
        node = node->next;
    }
    fprintf(debug, "distances initialized\n");
    print_subgraph();
    return 1;
}

int update_dist(int id, int dist) {
    node_t *node = get_node(id);
    if (!node) {
#if 1
        /* create src node */
        if (!(node = malloc(sizeof(node_t)))) {
            /* EMEM */
            return 0;
        }
        node->next = nodes;
        node->id = id;
        node->dist_from_src = INF;
        node->edges = NULL;
        nodes = node;
#else
        return 0;
#endif
    }
    if (dist < node->dist_from_src) {
        relaxed = 1;
        node->dist_from_src = dist;
        fprintf(debug, "distances updated\n");
        print_subgraph();
    }
    return 1;
}

int update_dist_client(int src, int dist) {
    int remote_unit_id = node_to_unit(src);
    enum clnt_stat clnt_stat;
    pars_t input = {src, dist};
    int ret;
    if (unit_id == remote_unit_id)
        return update_dist(src, dist);
    clnt_stat = callrpc (unit_ip[remote_unit_id], PRGBASE+remote_unit_id,
                         PRGVERS, UPDATEDIST,
                         (xdrproc_t) xdr_pars, (char *) &input,
                         (xdrproc_t) xdr_ret, (char *) &ret);
    if (clnt_stat != 0)
        clnt_perrno (clnt_stat);
    return ret;
}

int bellmanford_phase() {
    node_t *node = nodes;
    while (node) {
        edge_t *edge = node->edges;
        while (node->dist_from_src != INF && edge) {
            update_dist_client(edge->dest, node->dist_from_src+1);
            edge = edge->next;
        }
        node = node->next;
    }
    return 1;
}

int get_dist(int id) {
    node_t *node = get_node(id);
    if (!node) {
        /* node to update doesn't exist */
        return INF;
    }
    return node->dist_from_src;
}

int init_relaxed() {
    relaxed = 0;
    return 1;
}

int get_relaxed() {
    return relaxed;
}

int add_batch(batch_t *batch) {
    int i;
    fprintf(debug, "received batch! count: %d\n", batch->count);
    for (i = 0; i < batch->count; i++)
        add_edge(batch->first[i], batch->second[i]);
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
    *ret = init_dist(((pars_t *)input)->first);
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
    *ret = get_dist(((pars_t *)input)->first);
    return (char *) ret;
}

char *__init_relaxed(char *input) {
    int *ret = malloc(sizeof(int));
    *ret = init_relaxed();
    return (char *) ret;
}

char *__get_relaxed(char *input) {
    int *ret = malloc(sizeof(int));
    *ret = get_relaxed();
    return (char *) ret;
}

char *__add_batch(char *input) {
    batch_t *batch = (batch_t *) input;
    int *ret = malloc(sizeof(int));
    *ret = add_batch(batch);
    free(batch->first);
    free(batch->second);
    return (char *) ret;
}

int main(int argc, char *argv[]) {
    char fname[100];
    char *tok;
    int i = 0;
    /* get id */
    if (argc != 4) {
        fprintf(stderr, "Error: dgraph unit: invalid arguments\n");
        return -1;
    }
    /* extract parameters */
    unit_id = atoi(argv[1]);
    unit_count = atoi(argv[2]);
    unit_ip = malloc(sizeof(char *) * unit_count);
    nodes   = malloc(sizeof(node_t*) * MAX_SIZE);
    tok = strtok(argv[3], ",");
    while (tok) {
        unit_ip[i++] = strcpy(malloc(strlen(tok)+1), tok);
        tok = strtok(NULL, ",");
    }
    /* open debug file */
    sprintf(fname, "/var/log/dgraph/unit%d", unit_id);
    debug = fopen(fname, "w");
    if (!debug) {
        fprintf(stderr, "Error: dgraph unit: can't open %s\n", fname);
        return -1;
    }
    /* disable C buffer */
    setvbuf(debug, NULL, _IONBF, 0);
    /* register signal handler */
    signal(SIGINT, exit_unit);
    /* register rpc routines */
    registerrpc(PRGBASE+unit_id, PRGVERS, ADDEDGE,
                __add_edge, (xdrproc_t) xdr_pars, (xdrproc_t) xdr_ret);
    registerrpc(PRGBASE+unit_id, PRGVERS, REMEDGE,
                __rem_edge, (xdrproc_t) xdr_pars, (xdrproc_t) xdr_ret);
    registerrpc(PRGBASE+unit_id, PRGVERS, INITDIST,
                __init_dist, (xdrproc_t) xdr_pars, (xdrproc_t) xdr_ret);
    registerrpc(PRGBASE+unit_id, PRGVERS, UPDATEDIST,
                __update_dist, (xdrproc_t) xdr_pars, (xdrproc_t) xdr_ret);
    registerrpc(PRGBASE+unit_id, PRGVERS, BELLFORD,
                __bellmanford_phase, (xdrproc_t) xdr_pars, (xdrproc_t) xdr_ret);
    registerrpc(PRGBASE+unit_id, PRGVERS, GETDIST,
                __get_dist, (xdrproc_t) xdr_pars, (xdrproc_t) xdr_ret);
    registerrpc(PRGBASE+unit_id, PRGVERS, INITRELAX,
                __init_relaxed, (xdrproc_t) xdr_pars, (xdrproc_t) xdr_ret);
    registerrpc(PRGBASE+unit_id, PRGVERS, GETRELAX,
                __get_relaxed, (xdrproc_t) xdr_pars, (xdrproc_t) xdr_ret);
    registerrpc(PRGBASE+unit_id, PRGVERS, ADDBATCH,
                __add_batch, (xdrproc_t) xdr_batch_decode, (xdrproc_t) xdr_ret);
    /* output something */
    fprintf(debug, "Unit %d started.\n", unit_id);
    /* run server */
    svc_run();
}
