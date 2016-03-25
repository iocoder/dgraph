#ifndef __PROTO
#define __PROTO

#include <rpc/rpc.h>

#define PRGBASE ((u_long) 0x20000001)   /* server program (suite) number */
#define PRGVERS ((u_long) 1)    /* program version number */
#define ADDEDGE ((u_long) 1)    /* procedure number for look-up */
#define REMEDGE ((u_long) 2)    /* procedure number for look-up */

typedef struct pars {
    int first;
    int second;
} pars_t;

/* RPC outines */
int registerrpc(unsigned long prognum, unsigned long versnum,
                unsigned long procnum, char *(*procname)(char *),
                xdrproc_t inproc, xdrproc_t outproc);

/* XDR routines */
int xdr_pars(struct XDR *xdrpars, void *input);
int xdr_ret(struct XDR *xdrpars, void *input);

/* unit routines */
char *__add_edge(char *input);
char *__rem_edge(char *input);

/* configuration routines */
void read_conf();
char *get_val_str(char *name, int index);
int get_val_int(char *name, int index);

/* ssh routines */
int exec_ssh(char *hostname, char *cmd, ...);

#endif
