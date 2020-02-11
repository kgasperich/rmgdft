/*
 *
 * Copyright 2020 The RMG Project Developers. See the COPYRIGHT file 
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

#include <complex>
#include <blas.h>

#if GPU_ENABLED
    #include <cuda.h>
    #include <cuda_runtime_api.h>
    #include <cublas_v2.h>
#endif

// These are used to pack and unpack symmetric and Hermetian matrices from a full NxN
// format to a packed format. Lower triangular format is assumed for the NxN matrix.
void PackSqToTr(char *uplo, int N, double *Sq, double *Tr)
{
    int info; 
    dtrttp(uplo, &N, Sq, &N, Tr, &info);

}

void PackSqToTr(char *uplo, int N, std::complex<double> *Sq, std::complex<double> *Tr)
{
    int info; 
    ztrttp(uplo, &N, Sq, &N, Tr, &info);
}

void UnPackSqToTr(char *uplo, int N, double *Sq, double *Tr)
{
    int info;
    dtpttr(uplo, &N, Tr, Sq, &N, &info);
}

void UnPackSqToTr(char *uplo, int N, std::complex<double> *Sq, std::complex<double> *Tr)
{
    int info;
    ztpttr(uplo, &N, Tr, Sq, &N, &info);
}