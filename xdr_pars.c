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
    if (!xdr_int(xdrpars, batch->count))
        return (FALSE);
    for (i = 0;)
    if (!xdr_int(xdrpars, batch->first))
    return (TRUE);
}
