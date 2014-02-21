/************************** SVN Revision Information **************************
 **    $Id$    **
******************************************************************************/
 
/*
    get_Hvnlij:
    
    Get the elements of the Hamiltonian matrix due to the non-local
    potential, and add them into Aij.


*/
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "main_on.h"


void get_matB_qnm(double *Aij)
{
    int nh, ion, ip1, ip2, st1, st2, ist;
    double time1, time2, alpha;
    MPI_Status mstatus;
    int ion1, ion2, ion1_global, ion2_global;
    int iip1, iip2, iip1a, iip2a;
    int size, proc, proc1, proc2, idx;
    SPECIES *sp;
    ION *iptr;
    rmg_double_t *qqq;
    double tem;



    time1 = my_crtc();


    alpha = 1. / ct.vel;


    /* Loop over states on this proce onle 
       (distribution of work AND Aij contributions) */
    proc = pct.gridpe;
    for (st1 = ct.state_begin; st1 < ct.state_end; st1++)
        for (st2 = ct.state_begin; st2 < ct.state_end; st2++)
        {

            ist = (st1-ct.state_begin) * ct.num_states + st2;
            iip1 = (st1 - ct.state_begin) * num_nonlocal_ion[proc] * ct.max_nl;
            iip2 = (st2 - ct.state_begin) * num_nonlocal_ion[proc] * ct.max_nl;

            for (ion = 0; ion < num_nonlocal_ion[proc]; ion++)
            {
                /* begin shuchun wang */
                ion1 = pct.ionidx[ion];
                iptr = &ct.ions[ion1];
                sp = &ct.sp[iptr->species];
                nh = pct.prj_per_ion[ion1];
                qqq = pct.qqq[ion1];
                for (ip1 = 0; ip1 < nh; ip1++)
                {
                    for (ip2 = 0; ip2 < nh; ip2++)
                    {
                        if (fabs(kbpsi[iip1 + ip1]) > 0.)
                            Aij[ist] +=
                                alpha * qqq[ip2 * nh + ip1] * kbpsi[iip1 + ip1] * kbpsi[iip2 + ip2];
                    }
                }
                iip1 += ct.max_nl;
                iip2 += ct.max_nl;
                /*end shuchun wang */
            }                   /* end for ion */
        }                       /* end for st1 and st2 */


    /* Now calculate the part that kbpsi is stored in other processors */

    size = ct.state_per_proc * max_ion_nonlocal * ct.max_nl;

    for (idx = 1; idx < NPES; idx++)
    {

        proc1 = pct.gridpe + idx;
        if (proc1 >= NPES)
            proc1 = proc1 - NPES;
        proc2 = pct.gridpe - idx;
        if (proc2 < 0)
            proc2 += NPES;


        MPI_Sendrecv(kbpsi, size, MPI_DOUBLE, proc1, idx, kbpsi_comm, size,
                MPI_DOUBLE, proc2, idx, MPI_COMM_WORLD, &mstatus);


        for (ion1 = 0; ion1 < num_nonlocal_ion[proc]; ion1++)
            for (ion2 = 0; ion2 < num_nonlocal_ion[proc2]; ion2++)
            {
                ion1_global = ionidx_allproc[proc * max_ion_nonlocal + ion1];
                ion2_global = ionidx_allproc[proc2 * max_ion_nonlocal + ion2];

                if (ion1_global == ion2_global)
                {

                    /* begin shuchun wang */
                    iptr = &ct.ions[ion1_global];
                    sp = &ct.sp[iptr->species];
                    nh = pct.prj_per_ion[ion1_global];
                    qqq = pct.qqq[ion1_global];

                    for (st1 = ct.state_begin; st1 < ct.state_end; st1++)
                    {
                        iip1 = (st1 - ct.state_begin) * num_nonlocal_ion[proc] * ct.max_nl;

                        for (ip2 = 0; ip2 < nh; ip2++)
                        {

                            tem = 0.0;
                            for (ip1 = 0; ip1 < nh; ip1++)
                            {
                                iip1a = iip1 + ion1 * ct.max_nl + ip1;
                                tem += alpha * qqq[ip2 * nh + ip1] * kbpsi[iip1a];
                            }

                            for (st2 = state_begin[proc2]; st2 < state_end[proc2]; st2++)
                            {

                                ist = (st1 - ct.state_begin) * ct.num_states + st2;
                                iip2 = (st2 - state_begin[proc2]) * num_nonlocal_ion[proc2] * ct.max_nl;

                                iip2a = iip2 + ion2 * ct.max_nl + ip2;
                                Aij[ist] += tem * kbpsi_comm[iip2a];
                            }
                        }   /* end shuchun wang */
                    }       /* end if (ion1_glo... */
                }           /* end for ion1 and ion2 */

            }                   /* end for st1 and st2 */
    }                           /* end for idx */



    time2 = my_crtc();
    rmg_timings(matB_QNM, (time2 - time1));


}
