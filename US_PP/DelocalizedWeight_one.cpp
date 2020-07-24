/*
 *
 * Copyright 2014 The RMG Project Developers. See the COPYRIGHT file 
 * at the top-level directory of this distribution or in the current
 * directory.
 * 
 * This file is part of RMG. 
 * RMG is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * any later version.
 *
 * RMG is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
*/

#include <float.h>
#include <sys/stat.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sstream>

#include <complex>

#include <fcntl.h>

#include "const.h"

#include "rmgtypedefs.h"
#include "typedefs.h"
#include <complex>
#include "Kpoint.h"
#include "common_prototypes.h"
#include "common_prototypes1.h"
#include "transition.h"
#include "RmgParallelFft.h"



void DelocalizedWeight_one (int ion, int kindex, double kvec[3], Pw &pwave)
{

    std::complex<double> ZERO_t(0.0);
    std::complex<double> I_t(0.0, 1.0);

    int pbasis = pwave.Grid->get_P0_BASIS(1);
    /*Pointer to the result of forward transform on the coarse grid */

    int isp = Atoms[ion].species;
    SPECIES &AtomType = Species[Atoms[ion].species];

    std::complex<double> *forward_beta = new std::complex<double>[AtomType.nh * pbasis];
    std::complex<double> *Nlweight = new std::complex<double>[AtomType.nh * pbasis];

    std::complex<double> *fftw_phase = new std::complex<double>[pbasis];
    std::complex<double> *gbptr = new std::complex<double>[pbasis];

    if ((gbptr == NULL))
        rmg_error_handler (__FILE__, __LINE__, "can't allocate memory\n");


        /* Get species type */

    int amode = S_IREAD;
    std::string filename = "PROJECTORS/forward_beta_species" + std::to_string(isp) + "_kpt" + std::to_string(kindex);
    int fhand = open(filename.c_str(), O_RDWR, amode);
    size_t count = sizeof(std::complex<double>) * AtomType.nh * pbasis;
    read(fhand, forward_beta, count);
    close(fhand);

    int nlxdim = Rmg_G->get_NX_GRID(1);
    int nlydim = Rmg_G->get_NY_GRID(1);
    int nlzdim = Rmg_G->get_NZ_GRID(1);

    double vect[3], crds[3];
    vect[0] = Atoms[ion].xtal[0] - 0.5;
    vect[1] = Atoms[ion].xtal[1] - 0.5;
    vect[2] = Atoms[ion].xtal[2] - 0.5;

    /*The vector we are looking for should be */
    to_cartesian (vect, crds);

    /*Calculate the phase factor for delocalized case */
    double tpiba = 2.0*PI / Rmg_L.celldm[0];
    for(int ix = 0; ix < nlxdim; ix++)
    {
        for(int iy = 0; iy < nlydim; iy++)
        {
            for(int iz = 0; iz < nlzdim; iz++)
            {
                int idx = ix * nlydim * nlzdim + iy * nlzdim + iz;


                double theta = crds[0] * (tpiba*pwave.g[idx].a[0] + kvec[0]) +
                    crds[1] * (tpiba*pwave.g[idx].a[1] + kvec[1]) +
                    crds[2] * (tpiba*pwave.g[idx].a[2] + kvec[2]);

                fftw_phase[idx] = exp(std::complex<double>(0.0, theta));
            }
        }
    }

    /* Loop over radial projectors */
    for (int ip = 0; ip < AtomType.num_projectors; ip++)
    {

        /*Apply the phase factor */
        for (int idx = 0; idx < pbasis; idx++) gbptr[idx] =  forward_beta[ip * pbasis + idx] * std::conj(fftw_phase[idx]);

        /*Do the backwards transform */
        pwave.FftInverse(gbptr, &Nlweight[ip*pbasis]);

    }

    amode = S_IREAD | S_IWRITE;
    filename = "PROJECTORS/NLprojectors_ion" + std::to_string(ion) + "_kpt" + std::to_string(kindex);
    fhand = open(filename.c_str(), O_CREAT | O_TRUNC | O_RDWR, amode);
    count = sizeof(std::complex<double>) * AtomType.nh * pbasis;
    write(fhand, Nlweight, count);
    close(fhand);


    delete [] fftw_phase;
    delete [] gbptr;
    delete [] Nlweight;
    delete [] forward_beta;


} 