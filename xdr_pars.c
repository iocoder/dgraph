#include <rpc/rpc.h>
#include "proto.h"

int xdr_pars(struct XDR *xdrpars, void *input) {
   if (!xdr_int(xdrpars, &((pars_t *)input)->first))
      return (FALSE);
   if (!xdr_int(xdrpars, &((pars_t *)input)->second))
      return (FALSE);
   return (TRUE);
}

int xdr_ret(struct XDR *xdrpars, void *input) {
   if (!xdr_int(xdrpars, (int *) input))
      return (FALSE);
   return (TRUE);
}
