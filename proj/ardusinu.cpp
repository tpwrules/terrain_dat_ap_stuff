

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
    double *en;
    double C_x, C_y;
};
} // anonymous namespace

static PJ_XY ardusinu_s_forward(PJ_LP lp, PJ *P) { /* Spheroidal, forward */
    PJ_XY xy = {0.0, 0.0};
    struct pj_ardusinu_data *Q =
        static_cast<struct pj_ardusinu_data *>(P->opaque);

    // lam: longitude
    // phi: latitude

    xy.x = Q->C_x * lp.lam * (0. + cos(lp.phi));
    xy.y = Q->C_y * lp.phi;

    return xy;
}

static PJ_LP ardusinu_s_inverse(PJ_XY xy, PJ *P) { /* Spheroidal, inverse */
    PJ_LP lp = {0.0, 0.0};
    struct pj_ardusinu_data *Q =
        static_cast<struct pj_ardusinu_data *>(P->opaque);

    xy.y /= Q->C_y;
    lp.phi = xy.y;
    lp.lam = xy.x / (Q->C_x * (0. + cos(xy.y)));
    return lp;
}

static PJ *pj_ardusinu_destructor(PJ *P, int errlev) { /* Destructor */
    if (nullptr == P)
        return nullptr;

    if (nullptr == P->opaque)
        return pj_default_destructor(P, errlev);

    free(static_cast<struct pj_ardusinu_data *>(P->opaque)->en);
    return pj_default_destructor(P, errlev);
}

/* for spheres, only */
static void pj_ardusinu_setup(PJ *P) {
    struct pj_ardusinu_data *Q =
        static_cast<struct pj_ardusinu_data *>(P->opaque);
    P->es = 0;
    P->inv = ardusinu_s_inverse;
    P->fwd = ardusinu_s_forward;

    Q->C_y = 1.;
    Q->C_x = 1.;
}

PJ *PJ_PROJECTION(ardusinu) {
    struct pj_ardusinu_data *Q = static_cast<struct pj_ardusinu_data *>(
        calloc(1, sizeof(struct pj_ardusinu_data)));
    if (nullptr == Q)
        return pj_default_destructor(P, PROJ_ERR_OTHER /*ENOMEM*/);
    P->opaque = Q;
    P->destructor = pj_ardusinu_destructor;

    if (!(Q->en = pj_enfn(P->n)))
        return pj_default_destructor(P, PROJ_ERR_OTHER /*ENOMEM*/);

    {
        pj_ardusinu_setup(P);
    }
    return P;
}
