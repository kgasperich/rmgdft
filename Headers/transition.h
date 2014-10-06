#ifndef RMG_transition_h
#define RMG_transition_h

#if __cplusplus
#include <complex>
#include "BaseGrid.h"
#include "Lattice.h"
#include "TradeImages.h"
#include "const.h"
#include "RmgTimer.h"
#include "rmgtypedefs.h"
#include "params.h"
#include "typedefs.h"
#include "common_prototypes.h"
#include "common_prototypes1.h"
#include "rmg_error.h"
#include "Kpoint.h"


extern BaseGrid *Rmg_G;
extern TradeImages *Rmg_T;
extern Lattice Rmg_L;

extern "C"
{
double my_crtc (void);
MPI_Comm transition_get_grid_comm(void);
void thread_barrier_wait(void);
int transition_get_gridpe(void);
void get_vxc (double * rho, double * rho_oppo, double * rhocore, double * vxc);
void symmetry (int *ibrav, int *s, int *nsym, int *irg, int *irt,
               int *ftau, int *nat, rmg_double_t * tau, int *ityp, int *nks,
               rmg_double_t * xk, rmg_double_t * wk, rmg_double_t * celldm, int *nr1, int *nr2,
               int *nr3, rmg_double_t *a1, rmg_double_t *a2, rmg_double_t *a3, rmg_double_t *omega, int *wflag);
}
extern "C" void get_vh (double * rho, double * rhoc, double * vh_eig, int min_sweeps, int max_sweeps, int maxlevel, double rms_target, int boundaryflag);
extern "C" void get_vtot_psi (double * vtot_psi, double * vtot, int grid_ratio);
extern "C" void mix_rho (double * new_rho, double * rho, double *rhocore, int length, int length_x, int length_y, int length_z);
extern "C" void  get_rho_oppo (double * rho, double * rho_oppo);
extern "C" void get_ddd (double *veff);
extern "C" void mix_betaxpsi (int mix);
extern "C" void rmg_lbfgs (void);
extern "C" void write_restart (char *name, double * vh, double * rho, double * rho_oppo, double * vxc, STATE * states);

template <typename OrbitalType> void GetNewRho(Kpoint<OrbitalType> **Kpts, double *rho);
template <typename OrbitalType> void Init (double * vh, double * rho, double * rho_oppo, double * rhocore, double * rhoc,
           double * vnuc, double * vxc, Kpoint<OrbitalType> **Kptr);
template <typename OrbitalType> void Relax (int steps, double * vxc, double * vh, double * vnuc,
              double * rho, double * rho_oppo, double * rhocore, double * rhoc, Kpoint<OrbitalType> **Kptr);
template <typename OrbitalType> bool Quench (double * vxc, double * vh, double * vnuc, double * rho,
             double * rho_oppo, double * rhocore, double * rhoc, Kpoint<OrbitalType> **Kptr);
template <typename OrbitalType> bool Scf (double * vxc, double * vh, double *vh_ext,
          double * vnuc, double * rho, double * rho_oppo, double * rhocore, double * rhoc, int spin_flag,
          int hartree_min_sweeps, int hartree_max_sweeps , int boundaryflag, Kpoint<OrbitalType> **Kptr);
template <typename KpointType> void AppNls(Kpoint<KpointType> *kpoint, KpointType *sintR);


template <typename OrbitalType, typename CalcType> void MgEigState (Kpoint<OrbitalType> *kptr, State<OrbitalType> * sp, double * vtot_psi);


// Gamma point float version
void CPP_genvpsi (float * psi, float * sg_twovpsi, double * vtot, void * kd,
              double kmag, int dimx, int dimy, int dimz);
// complex float version
void CPP_genvpsi (std::complex<float> * psi, std::complex<float> * sg_twovpsi, double * vtot, void * kd,
              double kmag, int dimx, int dimy, int dimz);
// complex double version
void CPP_genvpsi (std::complex<double> * psi, std::complex<double> * sg_twovpsi, double * vtot, void * kd,
              double kmag, int dimx, int dimy, int dimz);
// Gamma point double version
void CPP_genvpsi (double * psi, double * sg_twovpsi, double * vtot, void * kd,
              double kmag, int dimx, int dimy, int dimz);

void pack_to_complex(double *psi, int nstates, int pbasis);
void pack_to_standard(double *psi, int nstates, int pbasis);
void MixBetaxpsi (int mix, int kpt);
template <typename OrbitalType>
void MixBetaxpsi1 (State<OrbitalType> *sp);
template  <typename OrbitalType> void AppCilrDriver (TradeImages *T, OrbitalType * psi, OrbitalType * a_psi, OrbitalType *b_psi, double *vtot,
    int dimx, int dimy, int dimz, double hx, double hy, double hz, int order);



extern "C" void app_cilr_driver (rmg_double_t * psi, rmg_double_t * a_psi, rmg_double_t *b_psi, rmg_double_t *vtot_eig_s,
    int dimx, int dimy, int dimz, rmg_double_t hx, rmg_double_t hy, rmg_double_t hz, int order);

template  <typename OrbitalType> double AppCilrFourth (OrbitalType *psi, OrbitalType *a_psi, OrbitalType *b_psi, double *vtot, int dimx, int dimy, int dimz, double gridhx, double gridhy, double gridhz);
template  <typename OrbitalType> double AppCilrSixth (OrbitalType *psi, OrbitalType *a_psi, OrbitalType *b_psi, double *vtot, int dimx, int dimy, int dimz, double gridhx, double gridhy, double gridhz);
template <typename OrbitalType, typename CalcType>
void PotentialAcceleration(Kpoint<OrbitalType> *kptr, State<OrbitalType> *sp, double *vtot_psi, double *nvtot_psi, CalcType *tmp_psi_t, OrbitalType *saved_psi);
// Print function
void RmgPrintTimings(BaseGrid *G, const char *outfile, int steps);
template <typename KpointType>
void ReinitIonicPotentials (Kpoint<KpointType> **kptr, double * vnuc, double * rhocore, double * rhoc);
template <typename KpointType>
void GetNlop (Kpoint<KpointType> **Kptr);
template <typename KpointType>
void GetWeight (Kpoint<KpointType> **Kptr);
template <typename KpointType>
void AssignWeight (Kpoint<KpointType> *kptr, SPECIES * sp, int ion, fftw_complex * beptr, double * rtptr, KpointType *Bweight, KpointType *Nlweight);
template <typename KpointType>
void Betaxpsi (Kpoint<KpointType> *kptr);
template <typename RmgType>
void AppCirDriverBeta (Lattice *L, TradeImages *T, RmgType * a, RmgType * b, int dimx, int dimy, int dimz, int order);
template <typename DataType>
double ApplyAOperator (Lattice *L, TradeImages *T, DataType *a, DataType *b, int dimx, int dimy, int dimz, double gridhx, double gridhy, double gridhz, int order);
template <typename RmgType>
void ApplyBOperator (Lattice *L, TradeImages *T, RmgType * a, RmgType * b, int dimx, int dimy, int dimz, int order);
void LoadUpf(SPECIES *sp);
extern "C" void LoadUpf_C(SPECIES *sp);
extern "C" bool verify( char *tagname, const void *optvalue );
extern "C" void ReadPseudo(int nspecies, SPECIES *sp);
template <typename KpointType>
void OutputEigenvalues (Kpoint<KpointType> **Kptr, int ikbs, int iscf);
template <typename KpointType>
void ReadData (char *name, double * vh, double * rho, double * vxc, State<KpointType> * states);
template <typename KpointType>
void GetOppositeEigvals (Kpoint<KpointType> **Kptr);
template <typename KpointType>
void GetOppositeOccupancies (Kpoint<KpointType> **Kptr);
template <typename StateType>
void LcaoGetPsi (State<StateType> * states);
template <typename StateType>
void LcaoGetAwave (StateType *psi, ION *iptr, int awave_idx, int l, int m, double coeff);
template <typename StateType>
void GetTe (double * rho, double * rho_oppo, double * rhocore, double * rhoc, double * vh, double * vxc, State<StateType> * states, int ii_flag);
template <typename StateType>
void WriteRestart (char *name, double * vh, double * rho, double * rho_oppo, double * vxc, State<StateType> * states);
template <typename StateType>
void WriteData (int fhand, double * vh, double * rho, double * rho_oppo, double * vxc, State<StateType> * states);
template <typename KpointType>
double Fill (Kpoint<KpointType> **Kptr, double width, double nel, double mix, int num_st, int occ_flag);
template <typename KpointType>
void OutputBandPlot(Kpoint<KpointType> ** Kptr);
int GetRecommendedThreadNumber(int nthreads, int npes, int thispe, MPI_Comm comm);
void InitHybridModel(int nthreads, int npes, int thispe, MPI_Comm comm);




#endif
#endif

#define rmg_printf( message... ) \
         fprintf( ct.logfile, message )

