/************************** SVN Revision Information **************************
 **    $Id$    **
******************************************************************************/
 
/*
 * for each orbit,  the centroids of charge is calculated 
 *
 *         	 x = <phi|x|phi>/<phi|phi>  
 *               y = <phi|y|phi>/<phi|phi>  
 *		 z = <phi|z|phi>/<phi|phi>   
 *                        
 *                                  
 *                                  */

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "main_on.h"


void get_orbit_center(STATE *state, rmg_double_t * x, rmg_double_t * y, rmg_double_t * z)
{
    int ix, iy, iz;
    int index;
    rmg_double_t temp;
    rmg_double_t sum;

    *x = 0.0;
    *y = 0.0;
    *z = 0.0;
    sum = 0.0;
    for (ix = 0; ix < state->orbit_nx; ix++)
        for (iy = 0; iy < state->orbit_ny; iy++)
            for (iz = 0; iz < state->orbit_nz; iz++)
            {
                index = ix * state->orbit_ny * state->orbit_nz + iy * state->orbit_nz + iz;
                temp = state->psiR[index] * state->psiR[index];
                sum += temp;
                *x += ix * temp;
                *y += iy * temp;
                *z += iz * temp;
            }

    *x = *x / sum;
    *y = *y / sum;
    *z = *z / sum;
    *x = (*x + state->ixmin) * ct.hxgrid * ct.xside;
    *y = (*y + state->iymin) * ct.hygrid * ct.yside;
    *z = (*z + state->izmin) * ct.hzgrid * ct.zside;

}
