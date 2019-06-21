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
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#ifdef USE_NUMA
    #include <numa.h>
#endif
#include <sys/mman.h>
#include "transition.h"
#include "const.h"
#include "RmgTimer.h"
#include "rmgtypedefs.h"
#include "params.h"
#include "typedefs.h"
#include "common_prototypes.h"
#include "common_prototypes1.h"
#include "rmg_error.h"
#include "Kpoint.h"
#include "Subdiag.h"
#include "Functional.h"
#include "GpuAlloc.h"
#include "../Headers/prototypes.h"
#include "ErrorFuncs.h"
#include "RmgException.h"
#include "Functional.h"
#include "Solvers.h"
#include "Atomic.h"
#include "RmgParallelFft.h"
#include "FiniteDiff.h"
#include "Scalapack.h"
#include "Elpa.h"
#include "GatherScatter.h"
extern Scalapack *MainSp;
#if USE_ELPA
extern Elpa *MainElpa;
#endif

static void init_alloc_nonloc_mem (void);


template <typename OrbitalType> void Init (double * vh, double * rho, double * rho_oppo, double * rhocore, double * rhoc,
        double * vnuc, double * vxc,  Kpoint<OrbitalType> **Kptr);

// Instantiate gamma and non-gamma versions
template void Init<double>(double*, double*, double*, double*, double*, double*, double*, Kpoint<double>**);
template void Init<std::complex<double> >(double*, double*, double*, double*, double*, double*, double*, Kpoint<std::complex <double> >**);

template <typename OrbitalType> void Init (double * vh, double * rho, double * rho_oppo, double * rhocore, double * rhoc,
        double * vnuc, double * vxc,  Kpoint<OrbitalType> **Kptr)
{

    RmgTimer RT0("Init");
    int kpt, ic, idx, state, ion, st1, P0_BASIS, FP0_BASIS;
    int species;
    int FPX0_GRID, FPY0_GRID, FPZ0_GRID;

    ct.nvme_orbital_fd = -1;
    ct.nvme_work_fd = -1;
    ct.nvme_weight_fd = -1;
    ct.nvme_Bweight_fd = -1;

    SPECIES *sp;
    OrbitalType *rptr = NULL, *nv, *ns = NULL, *Bns = NULL;
    double *vtot;
    double time2=0.0, fac;
    bool need_ns = true;
    if(ct.norm_conserving_pp && (ct.discretization != MEHRSTELLEN_DISCRETIZATION) && ct.is_gamma) need_ns = false;


    if (pct.imgpe == 0)
    {
	printf ("\n");
	printf ("               * * * * * * * * * *\n");
	printf ("               *    R   M   G    *\n");
	printf ("               * * * * * * * * * *\n");
	printf ("\n");
	printf (" -- A Real Space Multigrid Electronic structure code --\n");
	printf (" --      More information at www.rmgdft.org          --\n");
    }

    P0_BASIS =  Rmg_G->get_P0_BASIS(1);
    FP0_BASIS = Rmg_G->get_P0_BASIS(Rmg_G->default_FG_RATIO);

    FPX0_GRID = Rmg_G->get_PX0_GRID(Rmg_G->get_default_FG_RATIO());
    FPY0_GRID = Rmg_G->get_PY0_GRID(Rmg_G->get_default_FG_RATIO());
    FPZ0_GRID = Rmg_G->get_PZ0_GRID(Rmg_G->get_default_FG_RATIO());

    // Initialize buffers for gather scatter
    GatherScatterInit(P0_BASIS * pct.coalesce_factor);

    ct.total_scf_steps = 0;
    ct.md_steps = 0;

    ct.psi_nbasis = Rmg_G->get_NX_GRID(1) * Rmg_G->get_NY_GRID(1) * Rmg_G->get_NZ_GRID(1);
    ct.psi_fnbasis = Rmg_G->get_NX_GRID(Rmg_G->default_FG_RATIO) * Rmg_G->get_NY_GRID(Rmg_G->default_FG_RATIO) * Rmg_G->get_NZ_GRID(Rmg_G->default_FG_RATIO);


    // Set ct.num_states to ct.init_states. After init it is set to ct.run_states
    ct.num_states = ct.init_states;

    // Initialize some commonly used plans for our parallel ffts
    FftInitPlans();

    // If ecutwfc was set then adjust filter factor
    if(ct.ecutwfc > 0.0)
    {
        double tpiba2 = 4.0 * PI * PI / (Rmg_L.celldm[0] * Rmg_L.celldm[0]);
        ct.filter_factor = 2.0*ct.ecutwfc / (coarse_pwaves->gmax * tpiba2);

        if(ct.filter_factor > 1.0)
            rmg_printf("WARNING: The value of ecutwfc you have selected is to large for the specified grid. Reduce by %7.2f\n", ct.filter_factor);
    }
    //int fgcount = coarse_pwaves->count_filtered_gvectors(ct.filter_factor);
    //printf("Adjusted filtering factor = %f\n", ct.filter_factor);
    //printf("Adjusted gcount           = %d\n", fgcount);

    pct.localpp = NULL;
    pct.localrhoc = NULL;
    pct.localrhonlcc = NULL;
    pct.localatomicrho = NULL;



    /* Set hartree boundary condition stuff */
    ct.vh_pxgrid = FPX0_GRID;
    ct.vh_pygrid = FPY0_GRID;
    ct.vh_pzgrid = FPZ0_GRID;

    ct.vh_pbasis = ct.vh_pxgrid * ct.vh_pygrid * ct.vh_pzgrid;
    ct.vh_ext = new double[ct.vh_pbasis];

    for (idx = 0; idx < FP0_BASIS; idx++)
    {
        vh[idx] = 0.0;
        ct.vh_ext[idx] = 0.0;
    }



    /* Get the crystal or cartesian coordinates of the ions */
    init_pos ();


    ct.hmaxgrid = Rmg_L.get_xside() * Rmg_G->get_hxgrid(1);
    if (Rmg_L.get_yside() * Rmg_G->get_hygrid(1) > ct.hmaxgrid)
        ct.hmaxgrid = Rmg_L.get_yside() * Rmg_G->get_hygrid(1);
    if (Rmg_L.get_zside() * Rmg_G->get_hzgrid(1) > ct.hmaxgrid)
        ct.hmaxgrid = Rmg_L.get_zside() * Rmg_G->get_hzgrid(1);

    if(ct.ecutrho <= 0.0) ct.ecutrho = (2.0 *PI/ct.hmaxgrid) *(2.0 *PI/ct.hmaxgrid);
    ct.hmingrid = Rmg_L.get_xside() * Rmg_G->get_hxgrid(1);
    if (Rmg_L.get_yside() * Rmg_G->get_hygrid(1) < ct.hmingrid)
        ct.hmingrid = Rmg_L.get_yside() * Rmg_G->get_hygrid(1);
    if (Rmg_L.get_zside() * Rmg_G->get_hzgrid(1) < ct.hmingrid)
        ct.hmingrid = Rmg_L.get_zside() * Rmg_G->get_hzgrid(1);


    if ((ct.hmaxgrid / ct.hmingrid) > 1.1)
    {
        if (pct.imgpe == 0)
        {
            printf ("hxgrid = %7.5f\n", Rmg_G->get_hxgrid(1) * Rmg_L.get_xside());
            printf ("hygrid = %7.5f\n", Rmg_G->get_hygrid(1) * Rmg_L.get_yside());
            printf ("hzgrid = %7.5f\n", Rmg_G->get_hzgrid(1) * Rmg_L.get_zside());
        }
        rmg_error_handler (__FILE__, __LINE__, "Anisotropy too large");
    }


    /* Set discretization array */
    ct.xcstart = 0.0;
    ct.ycstart = 0.0;
    ct.zcstart = 0.0;

    // Compute some buffer sizes. Have to deal with the interaction of a couple of different options here
    //
    //    If an LCAO start is selected we need to allocate sufficient memory for the initial set of orbitals
    //    which may be larger than the number of orbitals used for the rest of the run.
    //    If potential acceleration is selected we need another buffer of size (run_states * P0_BASIS). If
    //    we allocate this in a single large buffer of 2*run_states*P0_BASIS it will probably be enough for
    //    the initialization even if an LCAO start is selected. Finally if Davidson diagonalization is
    //    requested we need to allocate memory for the expanded basis including the diagonalization arrays.

    bool potential_acceleration = (ct.potential_acceleration_constant_step > 0.0);


    int kpt_storage = ct.num_kpts_pe;
    if(ct.forceflag == BAND_STRUCTURE && (!ct.rmg2bgw)) kpt_storage = 1;

    /* Set state pointers and initialize state data */
#if GPU_ENABLED

    // Wavefunctions are actually stored here
    ct.non_local_block_size = std::max(ct.non_local_block_size, ct.max_states);

    rptr = (OrbitalType *)GpuMallocManaged(((size_t)kpt_storage * (size_t)ct.alloc_states * (size_t)P0_BASIS + (size_t)1024) * sizeof(OrbitalType));
    nv = (OrbitalType *)GpuMallocManaged((size_t)ct.non_local_block_size * (size_t)P0_BASIS * sizeof(OrbitalType));
    if(need_ns) ns = (OrbitalType *)GpuMallocManaged((size_t)ct.max_states * (size_t)P0_BASIS * sizeof(OrbitalType));
    if(!ct.norm_conserving_pp) {
        Bns = (OrbitalType *)GpuMallocManaged((size_t)ct.non_local_block_size * (size_t)P0_BASIS * sizeof(OrbitalType));
        pct.Bns = (double *)Bns;
    }
#else
    // Wavefunctions are actually stored here
    std::string newpath;

    if(ct.nvme_orbitals)
    {
        if(ct.nvme_orbital_fd != -1) close(ct.nvme_orbital_fd);

        newpath = ct.nvme_orbitals_path + std::string("rmg_orbital") + std::to_string(pct.spinpe) +
                  std::to_string(pct.kstart) + std::to_string(pct.gridpe);
        ct.nvme_orbital_fd = FileOpenAndCreate(newpath, O_RDWR|O_CREAT|O_TRUNC, (mode_t)0600);
        rptr = (OrbitalType *)CreateMmapArray(ct.nvme_orbital_fd, (kpt_storage * ct.alloc_states * P0_BASIS + 1024) * sizeof(OrbitalType));
        if(!rptr) rmg_error_handler(__FILE__,__LINE__,"Error: CreateMmapArray failed for orbitals. \n");
        madvise(rptr, ((size_t)kpt_storage * (size_t)ct.alloc_states * (size_t)P0_BASIS + (size_t)1024) * sizeof(OrbitalType), MADV_SEQUENTIAL);
    }
    else
    {
        rptr = new OrbitalType[(size_t)kpt_storage * (size_t)ct.alloc_states * (size_t)P0_BASIS + (size_t)1024]();
    }

    if(ct.nvme_work)
    {
        if(ct.nvme_work_fd != -1) close(ct.nvme_work_fd);

        newpath = ct.nvme_work_path + std::string("rmg_work") + std::to_string(pct.spinpe) +
                  std::to_string(pct.kstart) + std::to_string(pct.gridpe);
        ct.nvme_work_fd = FileOpenAndCreate(newpath, O_RDWR|O_CREAT|O_TRUNC, (mode_t)0600);
        if(need_ns) ns = (OrbitalType *)CreateMmapArray(ct.nvme_work_fd, (size_t)ct.max_states * (size_t)P0_BASIS * sizeof(OrbitalType));
        if(!ns) rmg_error_handler(__FILE__,__LINE__,"Error: CreateMmapArray failed for work arrays. \n");
        madvise(ns, (size_t)ct.max_states * (size_t)P0_BASIS * sizeof(OrbitalType), MADV_SEQUENTIAL);
    }
    else
    {
        if(need_ns) ns = new OrbitalType[(size_t)ct.max_states * (size_t)P0_BASIS]();
    }

    nv = new OrbitalType[(size_t)ct.non_local_block_size * (size_t)P0_BASIS]();
    if(!ct.norm_conserving_pp) {
        Bns = new OrbitalType[(size_t)ct.non_local_block_size * (size_t)P0_BASIS]();
        pct.Bns = (double *)Bns;
    }
#endif
    pct.nv = (double *)nv;
    pct.ns = (double *)ns;

    OrbitalType *rptr_k;
    rptr_k = rptr;
    for (kpt = 0; kpt < ct.num_kpts_pe; kpt++)
    {

    // for band structure calculation only one k point storage is initilized.
        if(ct.forceflag == BAND_STRUCTURE ) rptr_k = rptr;
        Kptr[kpt]->set_pool(rptr_k);
        Kptr[kpt]->nv = nv;
        Kptr[kpt]->ns = ns;

        if(!ct.norm_conserving_pp) Kptr[kpt]->Bns = Bns;

        for (st1 = 0; st1 < ct.max_states; st1++)
        {
            Kptr[kpt]->Kstates[st1].kidx = kpt;
            Kptr[kpt]->Kstates[st1].psi = rptr_k;
            Kptr[kpt]->Kstates[st1].vxc = vxc;
            Kptr[kpt]->Kstates[st1].vh = vh;
            Kptr[kpt]->Kstates[st1].vnuc = vnuc;
            Kptr[kpt]->Kstates[st1].pbasis = P0_BASIS;
            Kptr[kpt]->Kstates[st1].istate = st1;
            rptr_k +=P0_BASIS;
        }
    }


    //Dprintf ("If not an initial run read data from files");
    if ((ct.runflag == RESTART) || (ct.forceflag == BAND_STRUCTURE) )
    {
        ReadData (ct.infile, vh, rho, vxc, Kptr);

        /*For spin polarized calculation we need to get opposite charge density, eigenvalues and occupancies*/
        if (ct.spin_flag)
        {
            get_rho_oppo (rho, rho_oppo);
            GetOppositeEigvals (Kptr);
            GetOppositeOccupancies (Kptr);
        }
    }
    else 
    {
        /* Set the initial hartree potential to a constant */
        for (idx = 0; idx < FP0_BASIS; idx++)
            vh[idx] = 0.0;
    }


    //Dprintf ("Initialize the radial potential stuff");
    RmgTimer *RT1 = new RmgTimer("2-Init: radial potentials");
    InitPseudo (Kptr[0]->ControlMap);
    delete(RT1);

    /* Set initial ionic coordinates to the current ones. */
    for (ion = 0; ion < ct.num_ions; ion++)
    {
        for (ic = 0; ic < 3; ic++)
        {
            ct.ions[ion].icrds[ic] = ct.ions[ion].crds[ic];
            ct.ions[ion].ixtal[ic] = ct.ions[ion].xtal[ic];
        }
        SPECIES *sp = &ct.sp[ct.ions[ion].species];
        if(sp->num_ldaU_orbitals > 0) ct.num_ldaU_ions++;
    }


    /* Initialize symmetry stuff */
    if(ct.is_use_symmetry)
    {
        RmgTimer *RT1 = new RmgTimer("2-Init: symmetry");
        init_sym ();
        delete(RT1);
    }

    //Dprintf ("Allocate memory for arrays related to nonlocal PP");
    init_alloc_nonloc_mem ();

    /*Set max_nldim */
    ct.max_nldim = 0;
    for (species = 0; species < ct.num_species; species++)
    {
        /* Get species type */
        sp = &ct.sp[species];

        if (sp->nldim > ct.max_nldim)
            ct.max_nldim = sp->nldim;
    }


    if (ct.verbose == 1)
    {
	time2 = my_crtc ();

	rmg_printf ("\n\n init: Starting FFTW initialization ...");

	fflush (NULL);
    }


    /*Do forward transform for each species and store results on the coarse grid */
    RT1 = new RmgTimer("2-Init: weights");
    if(ct.localize_projectors)
    {
        InitLocalizedWeight ();
    }
    else
    {
        InitDelocalizedWeight ();
    }

    if(ct.atomic_orbital_type == LOCALIZED)
    {
        InitOrbital ();
    }
    else
    {
        InitDelocalizedOrbital ();
    }

    delete(RT1);

    if (ct.verbose == 1)
    {
	rmg_printf (" finished in %.1f s", my_crtc () - time2);
	fflush (NULL);
    }

    /* Initialize the qfunction stuff */
    RT1 = new RmgTimer("2-Init: qfunct");
    InitQfunct(Kptr[0]->ControlMap);
    delete(RT1);


    /* Update items that change when the ionic coordinates change */
    RT1 = new RmgTimer("2-Init: ReinitIonicPotentials");
    ReinitIonicPotentials (Kptr, vnuc, rhocore, rhoc);
    delete(RT1);


    /* Initialize orbitals */
    if (((ct.runflag == LCAO_START) || (ct.runflag == MODIFIED_LCAO_START)) && (ct.forceflag != BAND_STRUCTURE))
    {
        RmgTimer *RT2 = new RmgTimer("2-Init: LcaoGetPsi");
        for (kpt = 0; kpt < ct.num_kpts_pe; kpt++){
            LcaoGetPsi(Kptr[kpt]);
        }
        delete(RT2);
    }

    if (ct.runflag == RANDOM_START)
    {
        RmgTimer *RT2 = new RmgTimer("2-Init: RandomStart");
        for (kpt = 0; kpt < ct.num_kpts_pe; kpt++)
            Kptr[kpt]->random_init();
        delete(RT2);
    }



    /*Write out warnings, do it here before header otherwise they are hard to find*/
    /*if(Verify ("charge_mixing_type","pulay", Kptr[0]->ControlMap) &&	    
	    (ct.potential_acceleration_constant_step > 0.0))*/
    
    /*Switch to lapack if magma specified but not built with gpu support
     * Write warning as we are overriding user's choice*/

#if !MAGMA_LIBS
    if (ct.subdiag_driver == SUBDIAG_MAGMA)
    {
	rmg_printf("\n WARNING: MAGMA specified as subspace diagonalization driver, but RMG was not built with MAGMA support. Diagonalization driver will be auto selected");
       ct.subdiag_driver = SUBDIAG_AUTO;
    }
#endif    

    /*Take care of automatic settings, do it just before write header so that settings can be printed out  */
    /*Subspace diagonalization: Use magma if GPU-enabled, otherwise switch between lapack and Scalapack according to number of states*/
    if (ct.subdiag_driver ==  SUBDIAG_AUTO)
    {
#if GPU_ENABLED && MAGMA_LIBS
	ct.subdiag_driver = SUBDIAG_MAGMA;
#else
	if (ct.num_states < 128) 
	    ct.subdiag_driver = SUBDIAG_LAPACK;
	else
	    ct.subdiag_driver = SUBDIAG_SCALAPACK;
#endif
    }


    
    /* Write header, do it here rather than later, otherwise other information is printed first*/
    if (pct.imgpe == 0)
    {
        WriteHeader (); 
    }

    if (ct.forceflag == BAND_STRUCTURE) 
    {
        ct.num_states = ct.run_states;
        return;
    }

    // Normalize orbitals if not an initial run
    if (ct.runflag != RESTART) /* Initial run */
    {
        RmgTimer *RT2 = new RmgTimer("2-Init: normalization");
        for (kpt = 0; kpt < ct.num_kpts_pe; kpt++)
        {
            for (state = 0; state < ct.num_states; state++)
            {
                Kptr[kpt]->Kstates[state].normalize(Kptr[kpt]->Kstates[state].psi, state);
            }
        }
        delete(RT2);
    }


    /*For random start, use charge density equal to compensating charge */
    if (ct.runflag == RANDOM_START)
    {
        if (ct.spin_flag)
        {   
            fac = (2.0 - ct.init_equal_density_flag) / (3.0 - ct.init_equal_density_flag);
            for (idx = 0; idx < FP0_BASIS; idx++)
            {

                if (pct.spinpe == 0)
                {				    
                    rho[idx] = rhoc[idx] * fac;
                    rho_oppo[idx] = rhoc[idx] * (1.0 - fac);
                }
                else         /* if pct.spinpe = 1  */
                {
                    rho_oppo[idx] = rhoc[idx] * fac;
                    rho[idx] = rhoc[idx] * (1.0 - fac);
                }
            }

        }

        else
        {
            for (idx = 0; idx < FP0_BASIS; idx++)
                rho[idx] = rhoc[idx];
        }
    }

    if (((ct.runflag == LCAO_START) || (ct.runflag == MODIFIED_LCAO_START)) && (ct.forceflag != BAND_STRUCTURE)) {
        RT1 = new RmgTimer("2-Init: LcaoGetRho");
        InitLocalObject (rho, pct.localatomicrho, ATOMIC_RHO, false);

        if(ct.spin_flag) {
            get_rho_oppo (rho,  rho_oppo);
        }

        delete RT1;
    }


    ct.rms = 0.0;

    //Dprintf ("Generate initial vxc potential and hartree potential");
    pack_vhstod (vh, ct.vh_ext, FPX0_GRID, FPY0_GRID, FPZ0_GRID, ct.boundaryflag);


    //Dprintf ("Condition of run flag is %d", ct.runflag);
    /*If not a restart, get vxc and vh, for restart these quantities should read from the restart file*/
    if (ct.runflag != RESTART )
    {
        //get_vxc (rho, rho_oppo, rhocore, vxc);
        double etxc, vtxc;
        RT1 = new RmgTimer("2-Init: exchange/correlation");
        Functional *F = new Functional ( *Rmg_G, Rmg_L, *Rmg_T, ct.is_gamma);
        F->v_xc(rho, rhocore, etxc, vtxc, vxc, ct.spin_flag );
        // Initial vxc and vh can be very noisy
        FftFilter(vxc, *fine_pwaves, sqrt(ct.filter_factor) / (double)ct.FG_RATIO, LOW_PASS);
        delete F;
        delete RT1;

        RmgTimer *RT1 = new RmgTimer("2-Init: hartree");
        double rms_target = 1.0e-10;
        VhDriver(rho, rhoc, vh, ct.vh_ext, rms_target);
        FftFilter(vh, *fine_pwaves, sqrt(ct.filter_factor) / (double)ct.FG_RATIO, LOW_PASS);
        delete RT1;

    }

    // Generate initial Betaxpsi
    RmgTimer *RT3 = new RmgTimer("2-Init: betaxpsi");
    for (kpt =0; kpt < ct.num_kpts_pe; kpt++)
    {
        Betaxpsi (Kptr[kpt], 0, Kptr[kpt]->nstates, Kptr[kpt]->newsint_local, Kptr[kpt]->nl_weight);
    }
    delete RT3;

    if((ct.ldaU_mode != LDA_PLUS_U_NONE) && (ct.num_ldaU_ions > 0))
    {
        RT3 = new RmgTimer("3-MgridSubspace: ldaUop x psi");
        for (kpt =0; kpt < ct.num_kpts_pe; kpt++)
        {
            LdaplusUxpsi(Kptr[kpt], 0, Kptr[kpt]->nstates, Kptr[kpt]->orbitalsint_local, Kptr[kpt]->orbital_weight);
        }
        delete(RT3);
    }
 

    // If not a restart and diagonalization is requested do a subspace diagonalization otherwise orthogonalize
    if(ct.runflag != RESTART ){

        /*dnmI has to be stup before calling subdiag */
        vtot = new double[FP0_BASIS];
        double *vtot_psi = new double[P0_BASIS];

        for (idx = 0; idx < FP0_BASIS; idx++)
            vtot[idx] = vxc[idx] + vh[idx] + vnuc[idx];

        /*Generate the Dnm_I */
        get_ddd (vtot);

        // Transfer vtot from the fine grid to the wavefunction grid for Subdiag
        GetVtotPsi (vtot_psi, vtot, Rmg_G->default_FG_RATIO);

        /*Now we can do subspace diagonalization */
        double *new_rho=new double[FP0_BASIS];
        for (kpt =0; kpt < ct.num_kpts_pe; kpt++)
        {


            RmgTimer *RT2 = new RmgTimer("2-Init: subdiag");
            Subdiag (Kptr[kpt], vtot_psi, ct.subdiag_driver);
            // Force reinit of MainSp in case initialzation matrices are
            // not the same size
#if SCALAPACK_LIBS
            if(MainSp) {
                if(MainSp->Participates()) delete MainSp;
                MainSp = NULL;
            }
#if USE_ELPA
            if(MainElpa) {
                if(MainElpa->Participates()) delete MainElpa;
                MainElpa = NULL;
            }
#endif
#endif
            delete RT2;

            RmgTimer *RT3 = new RmgTimer("2-Init: betaxpsi");
            Betaxpsi (Kptr[kpt], 0, Kptr[kpt]->nstates, Kptr[kpt]->newsint_local, Kptr[kpt]->nl_weight);
            delete RT3;

            if((ct.ldaU_mode != LDA_PLUS_U_NONE) && (ct.num_ldaU_ions > 0))
            {
                RT3 = new RmgTimer("3-MgridSubspace: ldaUop x psi");
                LdaplusUxpsi(Kptr[kpt], 0, Kptr[kpt]->nstates, Kptr[kpt]->orbitalsint_local, Kptr[kpt]->orbital_weight);
                delete(RT3);
            }

        }


        if (ct.spin_flag)
            GetOppositeEigvals (Kptr);


        /* Take care of occupation filling */
        ct.efermi = Fill (Kptr, ct.occ_width, ct.nel, ct.occ_mix, ct.num_states, ct.occ_flag, ct.mp_order);
        OutputEigenvalues(Kptr, 0, -1);

        // Get new density 
        RmgTimer *RT2 = new RmgTimer("2-Init: GetNewRho");
        GetNewRho(Kptr, new_rho);
        MixRho(new_rho, rho, rhocore, vh, vh, rhoc, Kptr[0]->ControlMap, false);
        if (ct.spin_flag) get_rho_oppo (rho,  rho_oppo);

        delete RT2;
        delete [] new_rho;

        /*Release vtot memory */
        delete [] vtot_psi;
        delete [] vtot;


    }



    ct.num_states = ct.run_states;
    for (kpt =0; kpt < ct.num_kpts_pe; kpt++)
    {
        Kptr[kpt]->nstates = ct.run_states;
        Kptr[kpt]->dvh_skip = 8;
        // Set up potential acceleration arrays if required
        if(potential_acceleration) {
            if(ct.run_states <= 256) Kptr[kpt]->dvh_skip = 4;
            if(ct.run_states <= 128) Kptr[kpt]->dvh_skip = 2;
            if(ct.run_states <= 64) Kptr[kpt]->dvh_skip = 1;
            if(ct.coalesce_states)
            {
                int active_threads = ct.MG_THREADS_PER_NODE;
                if(ct.mpi_queue_mode && (active_threads > 1)) active_threads--;
                Kptr[kpt]->dvh_skip = active_threads * pct.coalesce_factor;
            }

            Kptr[kpt]->ndvh = ct.run_states / Kptr[kpt]->dvh_skip + 1;
            size_t sizr = Kptr[kpt]->ndvh * P0_BASIS * pct.coalesce_factor * sizeof(double);
            MPI_Alloc_mem(sizr , MPI_INFO_NULL, &Kptr[kpt]->dvh);

        }
    }

}                               /* end init */




static void init_alloc_nonloc_mem (void)
{
    int ion;


    pct.Qindex = new int *[ct.num_ions];
    pct.Qdvec = new int *[ct.num_ions];

    pct.Qidxptrlen = new int [ct.num_ions];
    pct.lptrlen = new int [ct.num_ions];

    pct.augfunc = new double *[ct.num_ions];
    pct.dnmI = new double *[ct.num_ions];
    pct.qqq = new double *[ct.num_ions];


    /*Initialize pointer arrays to NULL */
    for (ion = 0; ion < ct.num_ions; ion++)
    {

        pct.Qidxptrlen[ion] = 0;
        pct.lptrlen[ion] = 0;

        pct.Qindex[ion] = NULL;
        pct.Qdvec[ion] = NULL;

        pct.augfunc[ion] = NULL;
        pct.dnmI[ion] = NULL;
        pct.qqq[ion] = NULL;

    }                           /*end for(ion=0; ion<ct.num_ions; ion++) */

}                               /*end init_alloc_nonloc_mem */

/******/
