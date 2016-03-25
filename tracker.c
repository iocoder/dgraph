#include <stdio.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "proto.h"

#define UNIT_COUNT         "unit_count"
#define UNIT_IP            "unit"

int unit_count;
char **unit_ip;

int node_to_unit(int id) {
    return (id % unit_count);
}

int add_edge(int f, int s) {
    int unit_id = node_to_unit(f);
    enum clnt_stat clnt_stat;
    pars_t input = {f, s};
    int ret;
    clnt_stat = callrpc (unit_ip[unit_id], PRGBASE+unit_id, PRGVERS, ADDEDGE,
                         (xdrproc_t) xdr_pars, (char *) &input,
                         (xdrproc_t) xdr_ret, (char *) &ret);
    if (clnt_stat != 0)
        clnt_perrno (clnt_stat);
    return ret;
}

int rem_edge(int f, int s) {
    int unit_id = node_to_unit(f);
    enum clnt_stat clnt_stat;
    pars_t input = {f, s};
    int ret;
    clnt_stat = callrpc (unit_ip[unit_id], PRGBASE+unit_id, PRGVERS, REMEDGE,
                         (xdrproc_t) xdr_pars, (char *) &input,
                         (xdrproc_t) xdr_ret, (char *) &ret);
    if (clnt_stat != 0)
        clnt_perrno (clnt_stat);
    return ret;
}

int main() {
    int i;
    char i_str[100], unit_count_str[100];
    /* print splash */
    printf("**************************************************************\n");
    printf("* Welcome to dgraph system for distributed graph processing! *\n");
    printf("**************************************************************\n");
    /* read configuration file */
    read_conf();
    /* read number of units */
    unit_count = get_val_int(UNIT_COUNT, -1);
    if (unit_count < 0) {
        fprintf(stderr, "Error: can't read %s property.\n", UNIT_COUNT);
        return -1;
    }
    sprintf(unit_count_str, "%d", unit_count);
    /* get unit IP addresses */
    unit_ip = malloc(sizeof(char *) * unit_count);
    for (i = 0; i < unit_count; i++) {
        unit_ip[i] = get_val_str(UNIT_IP, i+1);
        if (!unit_ip[i]) {
            fprintf(stderr,"Error: can't read %s%d property.\n",UNIT_IP,i+1);
            return -1;
        }
        printf("Node %d on %s\n", i+1, unit_ip[i]);
        sprintf(i_str, "%d", i);
        exec_ssh(unit_ip[i], "/usr/local/bin/dgraph_unit", i_str,
                 unit_count_str, NULL);
    }
    /* wait for 1 second */
    system("sleep 1");
    /* print R */
    printf("R\n");
    /* some stuff */
    printf("%d\n", add_edge(1, 3));
    printf("%d\n", add_edge(1, 4));
    printf("%d\n", add_edge(1, 5));
    printf("%d\n", rem_edge(1, 5));
    printf("%d\n", rem_edge(5, 1));
    /* kill all units */
    for (i = 0; i < unit_count; i++) {
        exec_ssh(unit_ip[i], "killall", "-2",
                 "dgraph_unit", "2>", "/dev/null", NULL);
    }
    /* done */
    return 0;
}
