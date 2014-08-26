#include <complex>
#include "TradeImages.h"
#include "FiniteDiff.h"
#include "Mgrid.h"
#include "BlasWrappers.h"
#include "auxiliary.h"
#include "const.h"
#include "rmgtypedefs.h"
#include "typedefs.h"
#include "common_prototypes.h"
#include "common_prototypes1.h"
#include "rmg_alloc.h"
#include "rmg_error.h"
#include "RmgTimer.h"
#include "GlobalSums.h"
#include "Kpoint.h"
#include "packfuncs.h"
#include "transition.h"


static std::mutex vtot_sync_mutex;

template void PotentialAcceleration<double,float>(Kpoint<double> *, STATE *, double *, double *, float *, double *);
template void PotentialAcceleration<std::complex<double>, std::complex<float> >(Kpoint<std::complex<double> > *, STATE *, double *, double *, std::complex<float> *, std::complex<double> *);



template <typename OrbitalType, typename CalcType>
void PotentialAcceleration(Kpoint<OrbitalType> *kptr, STATE *sp, double *vtot_psi, double *nvtot_psi, CalcType *tmp_psi_t, OrbitalType *saved_psi)
{

    int pbasis = kptr->pbasis;


    // Save potential used for this orbital and update potential for future orbitals
    for(int idx = 0;idx <pbasis;idx++) {
        sp->dvhxc[idx] = nvtot_psi[idx];
    }


    if(ct.potential_acceleration_constant_step > 0.0) {

        double t1 = 1.8 * ct.potential_acceleration_constant_step;
        if(sp->occupation[0] < 0.5) t1 = 0.0;

        vtot_sync_mutex.lock();
        for(int idx = 0;idx <pbasis;idx++) {
//           vtot_psi[idx] = vtot_psi[idx] + t1 * PI * sp->occupation[0] * tmp_psi_t[idx] * (tmp_psi_t[idx] - (CalcType)saved_psi[idx]);
           vtot_psi[idx] = vtot_psi[idx] + t1 * PI * sp->occupation[0] * std::real(tmp_psi_t[idx] * std::conj((tmp_psi_t[idx] - (CalcType)saved_psi[idx])));
        }
        vtot_sync_mutex.unlock();

    }
#if 0
    if(ct.potential_acceleration_poisson_step > 0.0) {

        BaseGrid *G = kptr->G;
        Lattice *L = kptr->L;
        TradeImages *T = kptr->T;
        Mgrid MG(L, T);
        int sbasis = sp->sbasis;


        int eig_pre[6] = { 2, 3, 6, 2, 2, 2 };
        int eig_post[6] = { 2, 3, 6, 2, 2, 2 };
        int dimx = G->get_PX0_GRID(1);
        int dimy = G->get_PY0_GRID(1);
        int dimz = G->get_PZ0_GRID(1);
        double hxgrid = G->get_hxgrid(1);
        double hygrid = G->get_hygrid(1);
        double hzgrid = G->get_hzgrid(1);

        CalcType *res_t = new CalcType[2 * sbasis];
        CalcType *work_t = new CalcType[2 * sbasis];
        CalcType *work2_t = new CalcType[4 * sbasis];
        CalcType *sg_psi_t = new CalcType[2 * sbasis];


        // construct delta_rho
        for(int idx = 0;idx <pbasis;idx++) {
            res_t[idx] = -4.0 * PI * sp->occupation[0] *
                       (tmp_psi_t[idx] - saved_psi[idx]) * (2.0*saved_psi[idx] + (tmp_psi_t[idx] - saved_psi[idx]));
        }

        // zero out solution vector
        for(int idx = 0;idx <pbasis;idx++) {
            work_t[idx] = 0.0;
        }

        /* Pack delta_rho into multigrid array */
        CPP_pack_ptos<CalcType> (sg_psi_t, res_t, dimx, dimy, dimz);
        T->trade_images<CalcType> (sg_psi_t, dimx, dimy, dimz, FULL_TRADE);
        /* Smooth it once and store the smoothed charge in res */
        CPP_app_smooth1<CalcType> (sg_psi_t, res_t, G->get_PX0_GRID(1), G->get_PY0_GRID(1), G->get_PZ0_GRID(1));

        // neutralize cell with a constant background charge
        double t2 = 0.0;
        for(int idx = 0;idx <pbasis;idx++) {
            t2 += res_t[idx];
        }
        t2 = real_sum_all(t2, pct.grid_comm) / (G->get_NX_GRID(1) * G->get_NY_GRID(1) * G->get_NZ_GRID(1));
        for(int idx = 0;idx <pbasis;idx++) {
            res_t[idx] -= t2;
        }

        /* Do multigrid step with solution returned in work_t */
        int levels=1;

        RmgTimer *RT1 = new RmgTimer("Mg_eig: mgrid_solv");
        MG.mgrid_solv (work_t, res_t, work2_t,
                    dimx, dimy, dimz, hxgrid,
                    hygrid, hzgrid, 0, G->get_neighbors(), levels, eig_pre, eig_post, 1, 1.0, 0.0,
                    G->get_NX_GRID(1), G->get_NY_GRID(1), G->get_NZ_GRID(1),
                    G->get_PX_OFFSET(1), G->get_PY_OFFSET(1), G->get_PZ_OFFSET(1),
                    G->get_PX0_GRID(1), G->get_PY0_GRID(1), G->get_PZ0_GRID(1), ct.boundaryflag);
        delete(RT1);

        for(int idx = 0;idx <pbasis;idx++) {
            res_t[idx] = 0.0;
        }
        CPP_pack_stop_axpy<CalcType> (work_t, res_t, 1.0, dimx, dimy, dimz);
        double t1 = ct.potential_acceleration_poisson_step;
        if(sp->occupation[0] < 0.5) t1 = 0.0;
        vtot_sync_mutex.lock();
        for(int idx = 0;idx <pbasis;idx++) {
           vtot_psi[idx] = vtot_psi[idx] + t1 * res_t[idx];
        }
        vtot_sync_mutex.unlock();
        delete [] sg_psi_t;
        delete [] work2_t;
        delete [] work_t;
        delete [] res_t;

    }
#endif
}

