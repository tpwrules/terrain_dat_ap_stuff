

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
    double m, n, C_x, C_y;
};
} // anonymous namespace

static PJ_XY ardusinu_s_forward(PJ_LP lp, PJ *P) { /* Spheroidal, forward */
    PJ_XY xy = {0.0, 0.0};
    struct pj_ardusinu_data *Q =
        static_cast<struct pj_ardusinu_data *>(P->opaque);

    if (Q->m == 0.0)
        lp.phi = Q->n != 1. ? aasin(P->ctx, Q->n * sin(lp.phi)) : lp.phi;
    else {
        int i;

        const double k = Q->n * sin(lp.phi);
        for (i = MAX_ITER; i; --i) {
            const double V =
                (Q->m * lp.phi + sin(lp.phi) - k) / (Q->m + cos(lp.phi));
            lp.phi -= V;
            if (fabs(V) < LOOP_TOL)
                break;
        }
        if (!i) {
            proj_errno_set(P, PROJ_ERR_COORD_TRANSFM_OUTSIDE_PROJECTION_DOMAIN);
            return xy;
        }
    }
    xy.x = Q->C_x * lp.lam * (Q->m + cos(lp.phi));
    xy.y = Q->C_y * lp.phi;

    return xy;
}

static PJ_LP ardusinu_s_inverse(PJ_XY xy, PJ *P) { /* Spheroidal, inverse */
    PJ_LP lp = {0.0, 0.0};
    struct pj_ardusinu_data *Q =
        static_cast<struct pj_ardusinu_data *>(P->opaque);

    xy.y /= Q->C_y;
    lp.phi = (Q->m != 0.0)
                 ? aasin(P->ctx, (Q->m * xy.y + sin(xy.y)) / Q->n)
                 : (Q->n != 1. ? aasin(P->ctx, sin(xy.y) / Q->n) : xy.y);
    lp.lam = xy.x / (Q->C_x * (Q->m + cos(xy.y)));
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

    Q->C_y = sqrt((Q->m + 1.) / Q->n);
    Q->C_x = Q->C_y / (Q->m + 1.);
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
        Q->n = 1.;
        Q->m = 0.;
        pj_ardusinu_setup(P);
    }
    return P;
}
