#include <complex>
#include <omp.h>
#include "FiniteDiff.h"
#include "const.h"
#include "rmgtypedefs.h"
#include "typedefs.h"
#include "rmg_error.h"
#include "RmgTimer.h"
#include "GlobalSums.h"
#include "Kpoint.h"
#include "Subdiag.h"
#include "RmgGemm.h"
#include "GpuAlloc.h"
#include "ErrorFuncs.h"
#include "blas.h"

#include "prototypes.h"
#include "common_prototypes.h"
#include "common_prototypes1.h"
#include "transition.h"

// Performs the iteration step for the folded spectrum method
//
//  X_i(step+1) = X_i(step) + alpha  * (A - lambda_i*I) * X_i(step)
//  
// By doing this over a block of X_i we can optimize memory bandwidth usage
//
// Inputs: A is a square matrix of size n
//         eigs is a vector of length k
//         X is a matrix of size kxn
//         alpha is the multiplicative factor
//         iterations is the number of iterations

void FoldedSpectrumIterator(double *A, int n, double *eigs, int k, double *X, double alpha, int iterations)
{

    int ione = 1;
    double ONE_t = 1.0;
    double ZERO_t = 0.0;
    double *NULLptr = NULL;
    double *Agpu = NULL;
    double *Xgpu = NULL;
    double *Ygpu = NULL;
    double *Tgpu = NULL;
    double *eigs_gpu = NULL;
    double neg_rone = -1.0;
    int sizr = n * k;
    char *trans_n = "n";

#if GPU_ENABLED

    double *Y = (double *)GpuMallocHost(n * k * sizeof(double));
    for(int ix = 0;ix < n * k;ix++) Y[ix] = 0.0;

    cublasStatus_t custat;
    Agpu = (double *)GpuMalloc(n * n * sizeof(double));
    Xgpu = (double *)GpuMalloc(n * k * sizeof(double));
    Ygpu = (double *)GpuMalloc(n * k * sizeof(double));
    Tgpu = (double *)GpuMalloc(n * k * sizeof(double));
    eigs_gpu = (double *)GpuMalloc(n * sizeof(double));
    custat = cublasSetVector(n * n , sizeof(double), A, ione, Agpu, ione );
    RmgCudaError(__FILE__, __LINE__, custat, "Problem transferreing A from system memory to gpu.");

    // Must be a better way of doing this, setting matrix to zero on CPU and transferring seems wasteful
    custat = cublasSetVector(n * k , sizeof(double), Y, ione, Ygpu, ione );     // Must be a better way of doing this
    RmgCudaError(__FILE__, __LINE__, custat, "Problem transferreing Y from system memory to gpu.");

    // Must be a better way of doing this, setting matrix to zero on CPU and transferring seems wasteful
    custat = cublasSetVector(n * k , sizeof(double), X, ione, Xgpu, ione );     // Must be a better way of doing this
    RmgCudaError(__FILE__, __LINE__, custat, "Problem transferreing X from system memory to gpu.");

    custat = cublasSetVector(n, sizeof(double), eigs, ione, eigs_gpu, ione );     // Must be a better way of doing this
    RmgCudaError(__FILE__, __LINE__, custat, "Problem transferreing Y from system memory to gpu.");

    // outer loop over steps
    for(int step = 0;step < iterations;step++) {

        // Generate A * X for entire block
        RmgGemm(trans_n, trans_n, n, k, n, ONE_t, A, n, X, n, ZERO_t, Y, n, Agpu, Xgpu, Ygpu, false, false, false, false);

        // Subtract off lamda * I component. Gemm call is mainly for simplicity with GPU.
        custat = cublasDdgmm(ct.cublas_handle, CUBLAS_SIDE_RIGHT, n, k, Xgpu, n, eigs_gpu, ione, Tgpu, n);
        RmgCudaError(__FILE__, __LINE__, custat, "Problem executing cublasDdgmm.");
        custat = cublasDaxpy(ct.cublas_handle, sizr, &neg_rone, Tgpu, ione, Ygpu, ione);
        RmgCudaError(__FILE__, __LINE__, custat, "Problem executing cublasDaxpy.");
        custat = cublasDaxpy(ct.cublas_handle, sizr, &alpha, Ygpu, ione, Xgpu, ione);
        RmgCudaError(__FILE__, __LINE__, custat, "Problem executing cublasDaxpy.");

    }    

    custat = cublasGetVector(n * k, sizeof( double ), Xgpu, 1, X, 1 );
    RmgCudaError(__FILE__, __LINE__, custat, "Problem transferring X matrix from GPU to system memory.");

    GpuFree(eigs_gpu);
    GpuFree(Tgpu);
    GpuFree(Ygpu);
    GpuFree(Xgpu);
    GpuFree(Agpu);
    GpuFreeHost(Y);


#else

    double *Y = new double[n * k]();

    // outer loop over steps
    for(int step = 0;step < iterations;step++) {

        // Generate A * X for entire block
        RmgGemm(trans_n, trans_n, n, k, n, ONE_t, A, n, X, n, ZERO_t, Y, n, Agpu, Xgpu, Ygpu, false, false, false, false);

        // Subtract off lamda * I component. Gemm call is mainly for simplicity with GPU.
        for(int kcol = 0;kcol < k;kcol++) {
           for(int ix = 0;ix < n;ix++) {
               Y[kcol*n + ix] -= eigs[kcol] * X[kcol*n + ix];
           }
        }
        daxpy(&sizr, &alpha, Y, &ione, X, &ione);
        //for(int ix = 0;ix < n*k;ix++) X[ix] += alpha * Y[ix];
    }    
    delete [] Y;

#endif



}


