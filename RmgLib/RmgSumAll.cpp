
#include "BaseThread.h"
#include "RmgSumAll.h"
#include <typeinfo>


using namespace std;

std::mutex RmgSumAllLock;
volatile int vector_state = 0;
double RmgSumAllVector[2*MAX_RMG_THREADS];
double RecvBuf[2*MAX_RMG_THREADS];

template int RmgSumAll<int>(int, MPI_Comm);
template float RmgSumAll<float>(float, MPI_Comm);
template double RmgSumAll<double>(double, MPI_Comm);

template <typename RmgType>
RmgType RmgSumAll (RmgType x, MPI_Comm comm)
{
    BaseThread *T = BaseThread::getBaseThread(0);
    int tid;
    RmgType inreg;
    RmgType outreg;
    RmgType *inbuf = (RmgType *)RmgSumAllVector;
    RmgType *outbuf = (RmgType *)RecvBuf;

    tid = T->get_thread_tid();
    if(tid < 0) {

        inreg = x;
        
        if(typeid(RmgType) == typeid(int))
            MPI_Allreduce (&inreg, &outreg, 1, MPI_INT, MPI_SUM, comm);

        if(typeid(RmgType) == typeid(float))
            MPI_Allreduce (&inreg, &outreg, 1, MPI_FLOAT, MPI_SUM, comm);

        if(typeid(RmgType) == typeid(double))
            MPI_Allreduce (&inreg, &outreg, 1, MPI_DOUBLE, MPI_SUM, comm);

        return outreg;

    }

    RmgSumAllLock.lock();
        vector_state = 1;
        inbuf[tid] = x;
    RmgSumAllLock.unlock();

    // Wait until everyone gets here
    T->thread_barrier_wait();

    RmgSumAllLock.lock();
        if(vector_state == 1) {

            if(typeid(RmgType) == typeid(int))
                MPI_Allreduce(inbuf, outbuf, T->get_threads_per_node(), MPI_INT, MPI_SUM, comm);

            if(typeid(RmgType) == typeid(float))
                MPI_Allreduce(inbuf, outbuf, T->get_threads_per_node(), MPI_FLOAT, MPI_SUM, comm);

            if(typeid(RmgType) == typeid(double))
                MPI_Allreduce(inbuf, outbuf, T->get_threads_per_node(), MPI_DOUBLE, MPI_SUM, comm);

            vector_state = 0;
        }
    RmgSumAllLock.unlock();

    return outbuf[tid]; 
}

