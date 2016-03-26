#include <stdlib.h>
#include <rpc/rpc.h>
#include "proto.h"

int xdr_pars(XDR *xdrpars, void *input) {
   if (!xdr_int(xdrpars, &((pars_t *)input)->first))
      return (FALSE);
   if (!xdr_int(xdrpars, &((pars_t *)input)->second))
      return (FALSE);
   return (TRUE);
}

int xdr_ret(XDR *xdrpars, void *input) {
   if (!xdr_int(xdrpars, (int *) input))
      return (FALSE);
   return (TRUE);
}

int xdr_batch_encode(XDR *xdrpars, void *input) {
    int i;
    batch_t *batch = (batch_t *) input;
    if (!xdr_int(xdrpars, &batch->count))
        return (FALSE);
    for (i = 0; i < batch->count; i++)
        if (!xdr_int(xdrpars, &batch->first[i]))
            return (FALSE);
    for (i = 0; i < batch->count; i++)
        if (!xdr_int(xdrpars, &batch->second[i]))
            return (FALSE);
    return (TRUE);
}

int xdr_batch_decode(XDR *xdrpars, void *input) {
    int i;
    batch_t *batch = (batch_t *) input;
    if (!xdr_int(xdrpars, &batch->count))
        return (FALSE);
    batch->first  = malloc(sizeof(int) * batch->count + 1);
    batch->second = malloc(sizeof(int) * batch->count + 1);
    for (i = 0; i < batch->count; i++)
        if (!xdr_int(xdrpars, &batch->first[i]))
            return (FALSE);
    for (i = 0; i < batch->count; i++)
        if (!xdr_int(xdrpars, &batch->second[i]))
            return (FALSE);
    return (TRUE);
}
