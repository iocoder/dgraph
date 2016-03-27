#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <netdb.h>
#include "proto.h"

#if 0
#define DEBUG
#endif

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
char *tracker_ip;
int *unit_batch_size;
int relaxed;
char stack[STACKSIZE];
int childpid;
int pipefd[2] = {10, 11};

batch_entry_t *batch_head = NULL;
batch_entry_t *batch_tail = NULL;

node_t **hash_nodes = NULL;
list_node_t *list_nodes = NULL;

void insert_an_update(int node, int dist);
void send_rem_batches();
bool_t pmap_unset(unsigned long prognum, unsigned long versnum);

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

int callrpctcp(char *host, int prognum, int versnum, int procnum,
               xdrproc_t inproc, char *in,
               xdrproc_t outproc, char *out) {
    /* local vars */
    struct hostent *hp;
    struct sockaddr_in server_addr;
    register CLIENT *client;
    int socket = RPC_ANYSOCK;
    struct timeval total_timeout;
    enum clnt_stat clnt_stat;
    /* get hostent structure */
    if ((hp = gethostbyname(host)) == NULL) {
        fprintf(stderr, "can't get addr for '%s'\n", host);
        return (-1);
    }
    /* initialize server_addr structure */
    bcopy(hp->h_addr, (caddr_t)&server_addr.sin_addr, hp->h_length);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port =  0;
    /* create client structure */
    if ((client = clnttcp_create(&server_addr, prognum,
                                 versnum, &socket, BUFSIZ, BUFSIZ)) == NULL) {
        fprintf(stderr, "can't create client structure\n");
        return (-1);
    }
    /* set timeout */
    total_timeout.tv_sec = 20;
    total_timeout.tv_usec = 0;
    /* do the call */
    clnt_stat = clnt_call(client, procnum,
                          inproc, in, outproc, out, total_timeout);
    /* destroy client */
    clnt_destroy(client);
    /* done */
    return ((int)clnt_stat);
}

int update_dist_client(int src, int dist) {
    int remote_unit_id = node_to_unit(src);
    enum clnt_stat clnt_stat;
    pars_t input = {src, dist};
    int ret;
    if (unit_id == remote_unit_id)
        return update_dist(src, dist);
    clnt_stat = callrpctcp(unit_ip[remote_unit_id], PRGBASE+remote_unit_id,
                           PRGVERS, UPDATEDIST,
                           (xdrproc_t) xdr_pars, (char *) &input,
                           (xdrproc_t) xdr_ret, (char *) &ret);
    if (clnt_stat != 0)
        clnt_perrno (clnt_stat);
    return ret;
}

int finished_client() {
    enum clnt_stat clnt_stat;
    pars_t input = {0, 0};
    int ret;
    clnt_stat = callrpctcp(tracker_ip, PRGBASE+unit_count,
                           PRGVERS, FINISHED,
                           (xdrproc_t) xdr_pars, (char *) &input,
                           (xdrproc_t) xdr_ret, (char *) &ret);
    if (clnt_stat != 0)
        clnt_perrno (clnt_stat);
    return ret;
}

void bellmanford_phase() {
    list_node_t *list_node = list_nodes;
    /*fprintf(debug, "bellman started %d\n", unit_id);*/
    while (list_node) {
        edge_t *edge = list_node->node->edges;
        while (list_node->node->dist_from_src != INF && edge) {
            insert_an_update(edge->dest, list_node->node->dist_from_src+1);
            edge = edge->next;
        }
        list_node = list_node->next;
    }
    send_rem_batches();
    finished_client();
    /*fprintf(debug, "bellman finished %d\n", unit_id);*/
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
    /*fprintf(debug, "received update batch! count: %d\n", batch->count);*/
    for (i = 0; i < batch->count; i++)
        update_dist(batch->first[i], batch->second[i]);
}

int update_batch_client(int unit_id, batch_t batch) {
    enum clnt_stat clnt_stat;
    int ret;
    clnt_stat = callrpctcp(unit_ip[unit_id], PRGBASE+unit_id, PRGVERS,
                           UPDBATCH,
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
        if (unit_batch_size[i]) {
            extract_batch(i);
        }
    }
}

int __add_edge(char *input) {
    return add_edge(((pars_t *)input)->first, ((pars_t *)input)->second);
}

int __rem_edge(char *input) {
    return rem_edge(((pars_t *)input)->first, ((pars_t *)input)->second);
}

int __init_dist(char *input) {
    return init_dist(((pars_t *)input)->first);
}

int __update_dist(char *input) {
    return update_dist(((pars_t *)input)->first, ((pars_t *)input)->second);
}

int __bellmanford_phase(char *input) {
    char c;
    write(pipefd[1], &c, 1);
    return 1;
}

int __get_dist(char *input) {
    return get_dist(((pars_t *)input)->first);
}

int __init_relaxed(char *input) {
    return init_relaxed();
}

int __get_relaxed(char *input) {
    return get_relaxed();
}

int __add_batch(char *input) {
    return add_batch((batch_t *) input);
}

int __update_batch(char *input) {
    return update_batch((batch_t *) input);
}

void exit_unit(int sig) {
    /* safe exit */
    fclose(debug);
    exit(0);
}

void *child_main(void *arg) {
    while(1) {
        char c;
        read(pipefd[0], &c, 1);
        bellmanford_phase();
    }
}

void do_rpc(SVCXPRT *transp, int (*func)(char *),
            xdrproc_t inproc, xdrproc_t outproc) {
    char input[32];
    int array1[MAXBATCH];
    int array2[MAXBATCH];
    int ret;
    ((batch_t *) input)->first = array1;
    ((batch_t *) input)->second = array2;
    if (!svc_getargs(transp, inproc, input)) {
        svcerr_decode(transp);
        return;
    }
    ret = func(input);
    if (!svc_sendreply(transp, outproc, (char *) &ret))
        fprintf(debug, "Can't reply to RPC call\n");
}

void dispatch(struct svc_req *request, SVCXPRT *transp) {
    switch (request->rq_proc) {
        case NULLPROC:
            if (svc_sendreply(transp, (xdrproc_t) xdr_void, 0) == 0)
                fprintf(debug, "err: rcp_service");
            return;
        case ADDEDGE:
            do_rpc(transp, __add_edge,
                   (xdrproc_t) xdr_pars, (xdrproc_t) xdr_ret);
            return;
        case REMEDGE:
            do_rpc(transp, __rem_edge,
                   (xdrproc_t) xdr_pars, (xdrproc_t) xdr_ret);
            return;
        case INITDIST:
            do_rpc(transp, __init_dist,
                   (xdrproc_t) xdr_pars, (xdrproc_t) xdr_ret);
            return;
        case UPDATEDIST:
            do_rpc(transp, __update_dist, (
                xdrproc_t) xdr_pars, (xdrproc_t) xdr_ret);
            return;
        case BELLFORD:
            do_rpc(transp, __bellmanford_phase,
                   (xdrproc_t) xdr_pars, (xdrproc_t) xdr_ret);
            return;
        case GETDIST:
            do_rpc(transp, __get_dist,
                   (xdrproc_t) xdr_pars, (xdrproc_t) xdr_ret);
            return;
        case INITRELAX:
            do_rpc(transp, __init_relaxed,
                   (xdrproc_t) xdr_pars, (xdrproc_t) xdr_ret);
            return;
        case GETRELAX:
            do_rpc(transp, __get_relaxed,
                   (xdrproc_t) xdr_pars, (xdrproc_t) xdr_ret);
            return;
        case ADDBATCH:
            do_rpc(transp, __add_batch,
                   (xdrproc_t) xdr_batch_decode, (xdrproc_t) xdr_ret);
            return;
        case UPDBATCH:
            do_rpc(transp, __update_batch,
                   (xdrproc_t) xdr_batch_decode, (xdrproc_t) xdr_ret);
            return;
        default:
            fprintf(debug, "no RPC procedure\n");
            svcerr_noproc(transp);
            return;
    }
}

int main(int argc, char *argv[]) {
    char fname[100];
    char *tok;
    int i = 0;
    pthread_t child_thread;
    SVCXPRT *transp;
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
    while (i < unit_count) {
        unit_ip[i++] = strcpy(malloc(strlen(tok)+1), tok);
        tok = strtok(NULL, ",");
    }
    tracker_ip = strcpy(malloc(strlen(tok)+1), tok);
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
    fprintf(debug, "tracker: %s\n", tracker_ip);
    /* disable C buffer */
    setvbuf(debug, NULL, _IONBF, 0);
    /* open pipe */
    pipe(pipefd);
    /* open child process to handle bellmanford */
    /*childpid = clone(child_main, &stack[STACKSIZE], CLONE_VM, NULL);*/
    pthread_create(&child_thread, NULL, child_main, NULL);
    /* register signal handler */
    signal(SIGINT, exit_unit);
    /* register RPC */
    transp = svctcp_create(RPC_ANYSOCK, MAXBATCH*16, MAXBATCH*16);
    if (transp == NULL) {
        fprintf(debug, "Can't create an RPC server\n");
        exit(1);
    }
    pmap_unset(PRGBASE+unit_id, PRGVERS);
    if (!svc_register(transp,PRGBASE+unit_id,PRGVERS,dispatch,IPPROTO_TCP)) {
        fprintf(debug, "can't register RPC service\n");
        exit(1);
    }
    /* output something */
    fprintf(debug, "Unit %d started.\n", unit_id);
    /* run server */
    svc_run();
}
