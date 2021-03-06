#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <fcntl.h>
#include <netdb.h>

#include "proto.h"

#define UNIT_COUNT         "unit_count"
#define UNIT_IP            "unit"
#define TRACKER_IP         "tracker"

int unit_count;
int node_count = 0;
char **unit_ip;
char *tracker_ip;
batch_entry_t *batch_head = NULL;
batch_entry_t *batch_tail = NULL;
int *unit_batch_size;
volatile int finished;
int pipefd[2] = {10, 11};

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

int add_edge(int f, int s) {
    int unit_id, ret;
    enum clnt_stat clnt_stat;
    pars_t input;
    /* send to src machine */
    input.first = f;
    input.second = s;
    unit_id = node_to_unit(f);
    clnt_stat = callrpctcp(unit_ip[unit_id], PRGBASE+unit_id, PRGVERS,
                           ADDEDGE,
                           (xdrproc_t) xdr_pars, (char *) &input,
                           (xdrproc_t) xdr_ret, (char *) &ret);
    if (clnt_stat != 0)
        clnt_perrno (clnt_stat);
    if (!ret)
        return 0;
    /* send to dest machine */
#if 0
    input.first = s;
    input.second = -1;
    unit_id = node_to_unit(s);
    clnt_stat = callrpc (unit_ip[unit_id], PRGBASE+unit_id, PRGVERS, ADDEDGE,
                         (xdrproc_t) xdr_pars, (char *) &input,
                         (xdrproc_t) xdr_ret, (char *) &ret);
    if (clnt_stat != 0)
        clnt_perrno (clnt_stat);
    if (!ret)
        return 0;
#endif
    /* update count */
    if (f+1 > node_count)
        node_count = f+1;
    if (s+1 > node_count)
        node_count = s+1;
    /* done */
    return 1;
}

int rem_edge(int f, int s) {
    int unit_id = node_to_unit(f);
    enum clnt_stat clnt_stat;
    pars_t input = {f, s};
    int ret;
    clnt_stat = callrpctcp(unit_ip[unit_id], PRGBASE+unit_id, PRGVERS,
                           REMEDGE,
                           (xdrproc_t) xdr_pars, (char *) &input,
                           (xdrproc_t) xdr_ret, (char *) &ret);
    if (clnt_stat != 0)
        clnt_perrno (clnt_stat);
    return ret;
}

int init_dist(int unit_id, int src) {
    enum clnt_stat clnt_stat;
    pars_t input = {src, 0};
    int ret;
    clnt_stat = callrpctcp(unit_ip[unit_id], PRGBASE+unit_id, PRGVERS,
                           INITDIST,
                           (xdrproc_t) xdr_pars, (char *) &input,
                           (xdrproc_t) xdr_ret, (char *) &ret);
    if (clnt_stat != 0)
        clnt_perrno (clnt_stat);
    return ret;
}

int update_dist(int src, int dist) {
    int unit_id = node_to_unit(src);
    enum clnt_stat clnt_stat;
    pars_t input = {src, dist};
    int ret;
    clnt_stat = callrpctcp(unit_ip[unit_id], PRGBASE+unit_id, PRGVERS,
                           UPDATEDIST,
                           (xdrproc_t) xdr_pars, (char *) &input,
                           (xdrproc_t) xdr_ret, (char *) &ret);
    if (clnt_stat != 0)
        clnt_perrno (clnt_stat);
    return ret;
}

int bellmanford_phase(int unit_id) {
    enum clnt_stat clnt_stat;
    pars_t input = {0, 0};
    int ret;
    clnt_stat = callrpctcp(unit_ip[unit_id], PRGBASE+unit_id, PRGVERS,
                           BELLFORD,
                           (xdrproc_t) xdr_pars, (char *) &input,
                           (xdrproc_t) xdr_ret, (char *) &ret);
    if (clnt_stat != 0)
        clnt_perrno (clnt_stat);
    return ret;
}

int get_dist(int dest) {
    int unit_id = node_to_unit(dest);
    enum clnt_stat clnt_stat;
    pars_t input = {dest, 0};
    int ret;
    clnt_stat = callrpctcp(unit_ip[unit_id], PRGBASE+unit_id, PRGVERS,
                           GETDIST,
                           (xdrproc_t) xdr_pars, (char *) &input,
                           (xdrproc_t) xdr_ret, (char *) &ret);
    if (clnt_stat != 0)
        clnt_perrno (clnt_stat);
    if (ret == INF)
        return -1;
    return ret;
}

int init_relaxed(int unit_id) {
    enum clnt_stat clnt_stat;
    pars_t input = {0, 0};
    int ret;
    clnt_stat = callrpctcp(unit_ip[unit_id], PRGBASE+unit_id, PRGVERS,
                           INITRELAX,
                           (xdrproc_t) xdr_pars, (char *) &input,
                           (xdrproc_t) xdr_ret, (char *) &ret);
    if (clnt_stat != 0)
        clnt_perrno (clnt_stat);
    return ret;
}

int get_relaxed(int unit_id) {
    enum clnt_stat clnt_stat;
    pars_t input = {0, 0};
    int ret;
    clnt_stat = callrpctcp(unit_ip[unit_id], PRGBASE+unit_id, PRGVERS,
                           GETRELAX,
                           (xdrproc_t) xdr_pars, (char *) &input,
                           (xdrproc_t) xdr_ret, (char *) &ret);
    if (clnt_stat != 0)
        clnt_perrno (clnt_stat);
    return ret;
}

int add_batch(int unit_id, batch_t batch) {
    enum clnt_stat clnt_stat;
    int ret;
    clnt_stat = callrpctcp(unit_ip[unit_id], PRGBASE+unit_id, PRGVERS,
                           ADDBATCH,
                           (xdrproc_t) xdr_batch_encode, (char *) &batch,
                           (xdrproc_t) xdr_ret, (char *) &ret);
    if (clnt_stat != 0)
        clnt_perrno (clnt_stat);
    return ret;
}

int exec_batch(int unit_id, batch_t batch) {
    enum clnt_stat clnt_stat;
    int ret;
    clnt_stat = callrpctcp(unit_ip[unit_id], PRGBASE+unit_id, PRGVERS,
                           EXCBATCH,
                           (xdrproc_t) xdr_batch_encode, (char *) &batch,
                           (xdrproc_t) xdr_ret, (char *) &ret);
    if (clnt_stat != 0)
        clnt_perrno (clnt_stat);
    return ret;
}

void extract_batch(int unit_id) {
    batch_t batch;
    int j = 0;
    batch_entry_t *prev = NULL;
    batch_entry_t *entry = batch_head;
    batch_entry_t *next;
    batch.count  = unit_batch_size[unit_id];
    batch.cmd    = malloc(sizeof(char)*batch.count);
    batch.first  = malloc(sizeof(int)*batch.count);
    batch.second = malloc(sizeof(int)*batch.count);
    while (entry && entry->cmd != 'Q') {
        next = entry->next;
        if (node_to_unit(entry->f) == unit_id) {
            batch.cmd[j] = entry->cmd;
            batch.first[j] = entry->f;
            batch.second[j] = entry->s;
            if (entry->f+1 > node_count)
                node_count = entry->f + 1;
            if (entry->s+1 > node_count)
                node_count = entry->s + 1;
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
    exec_batch(unit_id, batch);
    /* free allocated stuff */
    free(batch.cmd);
    free(batch.first);
    free(batch.second);
    /* reset counter */
    unit_batch_size[unit_id] = 0;
}

int query(int src, int dest) {
    int i, j;
    /* initialize dist in all nodes/units */
    for (i = 0; i < unit_count; i++)
        init_dist(i, src);
    /* perform n bellmanford phases */
    for (i = 0; i < node_count; i++) {
        int relaxed = 0;
        finished = 0;
        /* initialize relaxed */
        for (j = 0; j < unit_count; j++) {
            init_relaxed(j);
        }
        /* start bellmanford phase */
        for (j = 0; j < unit_count; j++) {
            /* printf("phase %d,%d\n", i, j); */
            bellmanford_phase(j);
        }
        /* wait until bellmanford is done */
        while (finished < unit_count) {
            char c;
            /* read from pipe */
            read(pipefd[0], &c, 1);
            /* increase counter */
            finished++;
        }
        /* relaxed? */
        for (j = 0; j < unit_count; j++) {
            relaxed |= get_relaxed(j);
        }
        /* break bellmanford? */
        if (!relaxed)
            break;
    }
    /* ask for shortest path */
    return get_dist(dest);
}

int __finished(char *input) {
    char c;
    /* write to pipe */
    write(pipefd[1], &c, 1);
    return 1;
}

void exit_tracker(int sig) {
    /* safe exit */
    exit(0);
}

void do_rpc(SVCXPRT *transp, int (*func)(char *),
            xdrproc_t inproc, xdrproc_t outproc) {
    char input[MAXBATCH*16];
    int ret;
    if (!svc_getargs(transp, inproc, input)) {
        svcerr_decode(transp);
        return;
    }
    ret = func(input);
    if (!svc_sendreply(transp, outproc, (char *) &ret))
        fprintf(stderr, "Can't reply to RPC call\n");
}

void dispatch(struct svc_req *request, SVCXPRT *transp) {
    switch (request->rq_proc) {
        case NULLPROC:
            if (svc_sendreply(transp, (xdrproc_t) xdr_void, 0) == 0)
                fprintf(stderr, "err: rcp_service");
            return;
        case FINISHED:
            do_rpc(transp, __finished,
                   (xdrproc_t) xdr_pars, (xdrproc_t) xdr_ret);
            return;
        default:
            fprintf(stderr, "no RPC procedure\n");
            svcerr_noproc(transp);
            return;
    }
}

void count_batch() {
    batch_entry_t *entry = batch_head;
    while (entry && entry->cmd != 'Q') {
        unit_batch_size[node_to_unit(entry->f)]++;
        entry = entry->next;
    }
}

int main() {
    int i;
    char i_str[100], unit_count_str[100];
    char ip_address_str[4096] = "";
    char buf[4096];
    int childpid;
    SVCXPRT *transp;
    batch_entry_t *entry;
    /* print splash */
    printf("**************************************************************\n");
    printf("* Welcome to dgraph system for distributed graph processing! *\n");
    printf("**************************************************************\n");
    /* register signal handler */
    signal(SIGINT, exit_tracker);
    /* read configuration file */
    read_conf();
    /* read number of units */
    unit_count = get_val_int(UNIT_COUNT, -1);
    if (unit_count < 0) {
        fprintf(stderr, "Error: can't read %s property.\n", UNIT_COUNT);
        return -1;
    }
    sprintf(unit_count_str, "%d", unit_count);
    /* allocate unit batch size array */
    unit_batch_size = malloc(sizeof(int) * unit_count);
    for (i = 0; i < unit_count; i++) {
        unit_batch_size[i] = 0;
    }
    /* get unit IP addresses */
    unit_ip = malloc(sizeof(char *) * unit_count);
    for (i = 0; i < unit_count; i++) {
        unit_ip[i] = get_val_str(UNIT_IP, i+1);
        if (!unit_ip[i]) {
            fprintf(stderr,"Error: can't read %s%d property.\n",UNIT_IP,i+1);
            return -1;
        }
        if (i)
            strcat(ip_address_str, ",");
        strcat(ip_address_str, unit_ip[i]);
        printf("Node %d on %s\n", i+1, unit_ip[i]);
    }
    /* get tracker ip */
    tracker_ip = get_val_str(TRACKER_IP, -1);
    if (!tracker_ip) {
        fprintf(stderr,"Error: can't read %s property.\n", TRACKER_IP);
        return -1;
    }
    strcat(ip_address_str, ",");
    strcat(ip_address_str, tracker_ip);
    /* run units */
    for (i = 0; i < unit_count; i++) {
        sprintf(i_str, "%d", i);
        exec_ssh(unit_ip[i], "/usr/local/bin/dgraph_unit", i_str,
                 unit_count_str, ip_address_str, NULL);
    }
    /* open pipe */
    pipe(pipefd);
    /* fork and reserve */
    if (!(childpid = fork())) {
        /* register RPC */
        transp = svctcp_create(RPC_ANYSOCK, MAXBATCH*16, MAXBATCH*16);
        if (transp == NULL) {
            fprintf(stderr, "Can't create an RPC server\n");
            exit(1);
        }
        pmap_unset(PRGBASE+unit_count, PRGVERS);
        if (!svc_register(transp, PRGBASE+unit_count, PRGVERS,
                          dispatch, IPPROTO_TCP)){
            fprintf(stderr, "can't register RPC service\n");
            exit(1);
        }
        /* serve RPC calls */
        svc_run();
    }
    /* wait for 1 second */
    system("sleep 2");
    /* process input graph */
    while (1) {
        int f, s, unit_id;
        batch_entry_t *entry;
        /* read line from standard input */
        fgets(buf, sizeof(buf)-1, stdin);
        /* end of init? */
        if (!strcmp(buf, "S\n"))
            break;
        /* empty line */
        if (!strcmp(buf, "\n"))
            continue;
        /* read f & s */
        sscanf(buf, "%d%d", &f, &s);
        /* add to batch */
        entry = malloc(sizeof(batch_entry_t));
        entry->cmd = 'A';
        entry->f = f;
        entry->s = s;
        /* enqueue */
        batch_queue(entry);
        /* increase counters */
        unit_id = node_to_unit(entry->f);
        unit_batch_size[unit_id]++;
        /* reached maximum? */
        if (unit_batch_size[unit_id] == MAXBATCH) {
            extract_batch(unit_id);
        }
    }
    /* send all remaining batches */
    for (i = 0; i < unit_count; i++) {
        extract_batch(i);
    }
    /* print R */
    printf("R\n");
    /* wait for batches? */
    while (1) {
        char cmd;
        int f, s;
        batch_entry_t *entry;
        /* read line from standard input */
        fgets(buf, sizeof(buf)-1, stdin);
        /* terminate? */
        if (!strcmp(buf, "Q\n"))
            break;
        /* empty line? */
        if (!strcmp(buf, "\n"))
            continue;
        /* end of batch? */
        if (!strcmp(buf, "F\n")) {
            /* process batch */
            while (batch_head) {
                /* there is something to process */
                if (batch_head->cmd == 'Q') {
                    /* query */
                    entry = batch_dequeue();
                    printf("%d\n", query(entry->f, entry->s));
                    free(entry);
                } else {
                    /* add/delete */
                    count_batch();
                    for (i = 0; i < unit_count; i++)
                        extract_batch(i);
                }
            }
        } else {
            /* add entry to batch queue */
            sscanf(buf, "%c%d%d", &cmd, &f, &s);
            /* allocate structure */
            entry = malloc(sizeof(batch_entry_t));
            entry->cmd = cmd;
            entry->f = f;
            entry->s = s;
            /* enqueue */
            batch_queue(entry);
        }
    }
    /* kill all units */
    for (i = 0; i < unit_count; i++) {
        exec_ssh(unit_ip[i], "killall", "-2",
                 "dgraph_unit", "2>", "/dev/null", NULL);
    }
    /* kill tracker RPC handler */
    kill(childpid, SIGINT);
    /* done */
    return 0;
}
