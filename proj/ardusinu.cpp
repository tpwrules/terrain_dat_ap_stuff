

#include <errno.h>
#include <math.h>

#include "proj.h"
#include "proj_internal.h"

PROJ_HEAD(ardusinu, "AP Sinusoidal (Evil)") "\n\tPCyl, Sph";

#define EPS10 1e-10
#define MAX_ITER 8
#define LOOP_TOL 1e-7

namespace { // anonymous namespace
struct pj_ardusinu_data {
};
} // anonymous namespace

static PJ_XY ardusinu_s_forward(PJ_LP lp, PJ *P) { /* Spheroidal, forward */
    PJ_XY xy = {0.0, 0.0};
    struct pj_ardusinu_data *Q =
        static_cast<struct pj_ardusinu_data *>(P->opaque);

    // lam: longitude
    // phi: latitude

    xy.x = lp.lam * cos(lp.phi*0.5);
    xy.y = lp.phi;

    return xy;
}

static PJ_LP ardusinu_s_inverse(PJ_XY xy, PJ *P) { /* Spheroidal, inverse */
    PJ_LP lp = {0.0, 0.0};
    struct pj_ardusinu_data *Q =
        static_cast<struct pj_ardusinu_data *>(P->opaque);

    lp.phi = xy.y;
    lp.lam = xy.x / cos(xy.y*0.5);
    return lp;
}

static PJ *pj_ardusinu_destructor(PJ *P, int errlev) { /* Destructor */
    if (nullptr == P)
        return nullptr;

    if (nullptr == P->opaque)
        return pj_default_destructor(P, errlev);

    return pj_default_destructor(P, errlev);
}

PJ *PJ_PROJECTION(ardusinu) {
    struct pj_ardusinu_data *Q = static_cast<struct pj_ardusinu_data *>(
        calloc(1, sizeof(struct pj_ardusinu_data)));
    if (nullptr == Q)
        return pj_default_destructor(P, PROJ_ERR_OTHER /*ENOMEM*/);
    P->opaque = Q;
    P->destructor = pj_ardusinu_destructor;

    P->es = 0;
    P->inv = ardusinu_s_inverse;
    P->fwd = ardusinu_s_forward;

    return P;
}
