#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "proto.h"

#if 0
#define DEBUG
#endif

#define MAX_SIZE        10000  /* hashtable size */
#define MAXBATCH        1000   /* batch size */

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

typedef struct list_node_str {
    struct list_node_str *next;
    node_t *node;
} list_node_t;

FILE *debug;
int unit_id;
int unit_count;
char **unit_ip;
int *unit_batch_size;
int relaxed;

batch_entry_t *batch_head = NULL;
batch_entry_t *batch_tail = NULL;

node_t **hash_nodes = NULL;
list_node_t *list_nodes = NULL;

void insert_an_update(int node, int dist);
void send_rem_batches();

void batch_queue(batch_entry_t *entry) {
    if (batch_tail == NULL) {
        /* empty list */
        (batch_head = batch_tail = entry)->next = NULL;
    } else {
        /* non-empty list */
        entry->next = NULL;
        batch_tail->next = entry;
        batch_tail = entry;
    }
}

batch_entry_t *batch_dequeue() {
    batch_entry_t *entry = NULL;
    if (batch_head) {
        /* remove from head */
        entry = batch_head;
        batch_head = batch_head->next;
        if (!batch_head) {
            batch_tail = NULL;
        }
        entry->next = NULL;
    }
    return entry;
}

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
    node_t *list = hash_nodes[index];
    node_t *ret = get_node_list(id, list);
    return ret;
}

void print_list(node_t *list) {
#ifdef DEBUG
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
#endif
}

void print_subgraph() {
#ifdef DEBUG
    fprintf(debug, "CURRENT GRAPH\n");
    fprintf(debug, "--------------\n");
    int i;
    for(i=0; i<MAX_SIZE; i++) {
        fprintf(debug, "hash %d: \n", i);
        print_list(hash_nodes[i]);
    }
    fprintf(debug, "---------------------------------------\n");
#endif
}

void print_subgraph_() {
#ifdef DEBUG
    fprintf(debug, "CURRENT GRAPH\n");
    fprintf(debug, "--------------\n");
    list_node_t *curnode = list_nodes;
    edge_t *curedge;
    while (curnode) {
        fprintf(debug, "node %d (%d): ", curnode->node->id, curnode->node->dist_from_src);
        curedge = curnode->node->edges;
        while (curedge) {
            fprintf(debug, "%d ", curedge->dest);
            curedge = curedge->next;
        }
        fprintf(debug, "\n");
        curnode = curnode->next;
    }
    fprintf(debug, "---------------------------------------\n");
#endif
}

int add_edge(int node1, int node2) {
    node_t *node = get_node(node1);
    edge_t *edge;
    int index;
    list_node_t *list_node;
    /* src node doesn't exist */
    if (!node) {
        /* create src node */
        if (!(node = malloc(sizeof(node_t))))
            return 0; /* EMEM */
        index = node_to_index(node1);
        node->next = hash_nodes[index];
        node->id = node1;
        node->dist_from_src = INF;
        node->edges = NULL;
        hash_nodes[index] = node;
        /* create node in linked list of all nodes */
        if (!(list_node = malloc(sizeof(list_node_t))))
            return 0; /* EMEM */
        list_node->next = list_nodes;
        list_node->node = node;
        list_nodes = list_node;
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
#ifdef DEBUG
        fprintf(debug, "added %d->%d\n", node1, node2);
        print_subgraph();
#endif
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
#ifdef DEBUG
            fprintf(debug, "removed %d->%d\n", node1, node2);
            print_subgraph();
#endif
            /* done */
            return 1;
        }
        edge = (prev = edge)->next;
    }
    /* edge not found */
    return 0;
}

int init_dist(int src_id) {
    list_node_t *list_node = list_nodes;
    while (list_node) {
        if(list_node->node->id == src_id)
            list_node->node->dist_from_src = 0;
        else
            list_node->node->dist_from_src = INF;
        list_node = list_node->next;
    }
#ifdef DEBUG
    fprintf(debug, "distances initialized\n");
    print_subgraph();
#endif
    return 1;
}

int update_dist(int id, int dist) {
    node_t *node = get_node(id);
    if (!node) {
#if 1
        /* create src node */
        if (!(node = malloc(sizeof(node_t))))
            return 0; /* EMEM */
        int index = node_to_index(id);
        node->next = hash_nodes[index];
        node->id = id;
        node->dist_from_src = INF;
        node->edges = NULL;
        hash_nodes[index] = node;
        /* create node in linked list of all nodes */
        list_node_t *list_node = NULL;
        if (!(list_node = malloc(sizeof(list_node_t))))
            return 0; /* EMEM */
        list_node->next = list_nodes;
        list_node->node = node;
        list_nodes = list_node;
#else
        return 0;
#endif
    }
    if (dist < node->dist_from_src) {
        relaxed = 1;
        node->dist_from_src = dist;
#ifdef DEBUG
        fprintf(debug, "distances updated\n");
        print_subgraph();
#endif
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
    list_node_t *list_node = list_nodes;
    while (list_node) {
        edge_t *edge = list_node->node->edges;
        while (list_node->node->dist_from_src != INF && edge) {
            insert_an_update(edge->dest, list_node->node->dist_from_src+1);
            edge = edge->next;
        }
        list_node = list_node->next;
    }
    send_rem_batches();
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
    fprintf(debug, "received add batch! count: %d\n", batch->count);
    for (i = 0; i < batch->count; i++)
        add_edge(batch->first[i], batch->second[i]);
}

int update_batch(batch_t *batch) {
    int i;
    fprintf(debug, "received update batch! count: %d\n", batch->count);
    for (i = 0; i < batch->count; i++)
        update_dist(batch->first[i], batch->second[i]);
}

int update_batch_client(int unit_id, batch_t batch) {
    enum clnt_stat clnt_stat;
    int ret;
    clnt_stat = callrpc (unit_ip[unit_id], PRGBASE+unit_id, PRGVERS, UPDBATCH,
                         (xdrproc_t) xdr_batch_encode, (char *) &batch,
                         (xdrproc_t) xdr_ret, (char *) &ret);
    if (clnt_stat != 0)
        clnt_perrno (clnt_stat);
    return ret;
}

void extract_batch(int uid) {
    batch_t batch;
    int j = 0;
    batch_entry_t *prev = NULL;
    batch_entry_t *entry = batch_head;
    batch_entry_t *next;
    batch.count  = unit_batch_size[uid];
    batch.first  = malloc(sizeof(int)*batch.count + 1);
    batch.second = malloc(sizeof(int)*batch.count + 1);
    while (entry) {
        next = entry->next;
        if (node_to_unit(entry->f) == uid) {
            batch.first[j] = entry->f;
            batch.second[j] = entry->s;
            j++;
            if (prev) {
                prev->next = next;
                if (!next)
                    batch_tail = prev;
            } else {
                batch_head = next;
                if (!next)
                    batch_tail = batch_head;
            }
            free(entry);
        } else {
            prev = entry;
        }
        entry = next;
    }
    /* now send batch to correspondent node */
    if (batch.count)
        update_batch_client(uid, batch);
    /* free allocated stuff */
    free(batch.first);
    free(batch.second);
    /* reset counter */
    unit_batch_size[uid] = 0;
}

void insert_an_update(int node, int dist) {
    int uid;
    batch_entry_t *entry;
    if (unit_id == node_to_unit(node)) {
        update_dist(node, dist);
    } else {
        /* add to batch */
        entry = malloc(sizeof(batch_entry_t));
        entry->cmd = 'U';
        entry->f = node;
        entry->s = dist;
        /* enqueue */
        batch_queue(entry);
        /* increase counters */
        uid = node_to_unit(entry->f);
        unit_batch_size[uid]++;
        /* reached maximum? */
        if (unit_batch_size[uid] == MAXBATCH) {
            extract_batch(uid);
        }
    }
}

void send_rem_batches() {
    /* send all remaining batches */
    int i;
    for (i = 0; i < unit_count; i++) {
        extract_batch(i);
    }
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

char *__update_batch(char *input) {
    batch_t *batch = (batch_t *) input;
    int *ret = malloc(sizeof(int));
    *ret = update_batch(batch);
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
    tok = strtok(argv[3], ",");
    while (tok) {
        unit_ip[i++] = strcpy(malloc(strlen(tok)+1), tok);
        tok = strtok(NULL, ",");
    }
    /* allocate unit batch size array */
    unit_batch_size = malloc(sizeof(int) * unit_count);
    for (i = 0; i < unit_count; i++) {
        unit_batch_size[i] = 0;
    }
    /* allocate hashtable */
    hash_nodes = malloc(sizeof(node_t*) * MAX_SIZE);
    for (i=0; i<MAX_SIZE; i++) {
        hash_nodes[i] = NULL;
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
    registerrpc(PRGBASE+unit_id, PRGVERS, UPDBATCH, __update_batch,
                (xdrproc_t) xdr_batch_decode, (xdrproc_t) xdr_ret);
    /* output something */
    fprintf(debug, "Unit %d started.\n", unit_id);
    // print_subgraph();
    // print_subgraph_();
    /* run server */
    svc_run();
}
