/************************** SVN Revision Information **************************
 **    $Id$    **
******************************************************************************/

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <complex.h>
#include "main.h"
#include "common_prototypes.h"

void init_weight_s (SPECIES * sp, fftw_complex * rtptr, int ip, fftw_plan p1)
{

    int idx, ix, iy, iz, size, ibegin, iend;
    double r, ax[3], xc, yc, zc;
    double t1, hxx, hyy, hzz;
    double complex *weptr, *gwptr;

    /* nlfdim is size of the non-local box in the double grid */
    size = sp->nlfdim * sp->nlfdim * sp->nlfdim;

    weptr = (double complex *)fftw_malloc(sizeof(double complex) * size);
    gwptr = (double complex *)fftw_malloc(sizeof(double complex) * size);

    if ((weptr == NULL) || (gwptr == NULL))
        error_handler ("can't allocate memory\n");

    hxx = get_hxgrid() / (double) ct.nxfgrid;
    hyy = get_hygrid() / (double) ct.nyfgrid;
    hzz = get_hzgrid() / (double) ct.nzfgrid;

    /*We assume that ion is in the center of non-local box */
//    ibegin = -(sp->nldim / 2) * ct.nxfgrid;

    ibegin = -sp->nlfdim/2;
    iend = ibegin + sp->nlfdim;
    int ixx, iyy, izz;
    for (ix = ibegin; ix < iend; ix++)
    {
        ixx = ix;
        if (ixx < 0) ixx = ix + sp->nlfdim;
        xc = (double) ix *hxx;

        for (iy = ibegin; iy < iend; iy++)
        {
            iyy = iy;
            if (iyy < 0) iyy = iy + sp->nlfdim;
            yc = (double) iy *hyy;

            for (iz = ibegin; iz < iend; iz++)
            {

                izz = iz;
                if (izz < 0) izz = iz + sp->nlfdim;
                zc = (double) iz *hzz;

                ax[0] = xc;
                ax[1] = yc;
                ax[2] = zc;

                r = metric (ax);

                t1 = AtomicInterpolate(&sp->betalig[ip][0], r);
                idx = ixx *sp->nlfdim * sp->nlfdim + iyy * sp->nlfdim + izz;
                weptr[idx] = sqrt (1.0 / (4.0 * PI)) * t1 + 0.0I;

                if((ix*2 + sp->nlfdim) == 0 || (iy*2 + sp->nlfdim) == 0 || (iz*2 + sp->nlfdim) == 0 ) 
                    weptr[idx] = 0.0;

            }                   /* end for */

        }                       /* end for */

    }                           /* end for */

    fftw_execute_dft (p1, weptr, gwptr);

    pack_gftoc (sp, gwptr, rtptr);

    fftw_free (gwptr);
    fftw_free (weptr);
}
