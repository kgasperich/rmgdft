
#include <exception>
#include <iostream>
#include <vector>
#include <string>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/lexical_cast.hpp>

#include "make_conf.h"
#include "const.h"
#include "rmgtypedefs.h"
#include "params.h"
#include "typedefs.h"
#include "rmg_error.h"
#include "MapElements.h"
#include "transition.h"



// Converts a string containg a PP_MESH, PP_RAB, PP_LOCAL, etc into a double array and
// returns a pointer to the array. The calling function needs to have obtained the 
// number of elements expected in the array from the attributes field. The actual
// number of elements placed in the array is count-start where start elements
// are skipped. In other words if there are 100 elements and start=1 the first
// element is skipped and the remaining 99 are placed in the array starting from
// the zeroth position.
double * UPF_str_to_double_array(std::string str, int max_count, int start) {

   std::vector<std::string> strs;
   int count = 0;
   double *array = new double[max_count-start];
   std::string delims = " \t\n";
   boost::trim(str);
   boost::algorithm::split( strs, str, boost::is_any_of(delims), boost::token_compress_on );
 
   std::vector<std::string>::iterator it;
   for (it = strs.begin(); it != strs.end(); ++it) {
       std::string svalue = *it;
       boost::trim(svalue);
       if(count >= start)
           array[count-start] = std::atof(svalue.c_str());
       count++;
       if(count > max_count)
           rmg_error_handler(__FILE__,__LINE__,"Problem with UPF pseudopotential. Too many elements.\n");
       if(count == max_count) return array;
   }

   return array;
}

// Skips the first element in array since some UPF pseudopotentials
// have r[0] = 0 which causes us issues.
double * UPF_read_mesh_array(std::string str, int max_count)
{
    return UPF_str_to_double_array(str, max_count + 1, 1);
}

using boost::property_tree::ptree;

// Reads a pseudopotential stored in UPF format into our internal data structures.
void LoadUpf(SPECIES *sp)
{

   ptree upf_tree;
   std::string PP_INFO;
   std::string upf_file = sp->pseudo_filename;
   int max_nlprojectors = 0;
   int l_max;
   double  ddd0[6][6];  // Used to read in the PP_DIJ
   double qqq[6][6];    // Used to read in the norms of the augmentation functions (PP_Q)


   // Get the compulsory stuff first
    try 
    {
        read_xml(upf_file, upf_tree);
        PP_INFO = upf_tree.get<std::string>("UPF.PP_INFO"); 
       
        // Atomic symbol, mass, number and zvalence and mesh size
        std::string atomic_symbol = upf_tree.get<std::string>("UPF.PP_HEADER.<xmlattr>.element");
        boost::trim(atomic_symbol);
        sp->atomic_symbol = new char[4]();
        std::strncpy(sp->atomic_symbol, atomic_symbol.c_str(), 3);
        sp->atomic_mass = GetAtomicMass(atomic_symbol);
        sp->atomic_number = GetAtomicNumber(atomic_symbol);

        sp->zvalence = upf_tree.get<double>("UPF.PP_HEADER.<xmlattr>.z_valence");
        sp->rg_points = upf_tree.get<int>("UPF.PP_HEADER.<xmlattr>.mesh_size");
        sp->rg_points--;   // Many upf pseudopotentials use r=0.0 as the first point which causes us issues so skip it.

        l_max = upf_tree.get<int>("UPF.PP_HEADER.<xmlattr>.l_max");
        sp->kkbeta = sp->rg_points;
       
        // Get the type of pseudopotential
        std::string pp_type = upf_tree.get<std::string>("UPF.PP_HEADER.<xmlattr>.pseudo_type");
        boost::to_upper(pp_type);

        if(!pp_type.compare(0, 2, "NC")) {
            ct.norm_conserving_pp = true;
        }
        else if(!pp_type.compare(0, 2, "SL")) {  // Norm conserving with extra information
            ct.norm_conserving_pp = true;
        }
        else if(!pp_type.compare(0, 2, "US")) {
            ct.norm_conserving_pp = false;
        }
        else {
            rmg_error_handler(__FILE__,__LINE__,"RMG only supports norm conserving and ultrasoft pseudpotentials.\n");
        }


        // Kind of redundant information in the format
        std::string s_is_ultrasoft = upf_tree.get<std::string>("UPF.PP_HEADER.<xmlattr>.is_ultrasoft"); 
        boost::to_upper(s_is_ultrasoft);
        if(!s_is_ultrasoft.compare(0,1,"F")) ct.norm_conserving_pp = true;
        if(!s_is_ultrasoft.compare(0,5,"FALSE")) ct.norm_conserving_pp = true;
        if(!s_is_ultrasoft.compare(0,1,"T")) ct.norm_conserving_pp = false;
        if(!s_is_ultrasoft.compare(0,4,"TRUE")) ct.norm_conserving_pp = false;

        // Core correction flag
        std::string s_core_correction = upf_tree.get<std::string>("UPF.PP_HEADER.<xmlattr>.core_correction");
        if(!s_core_correction.compare(0,1,"F")) sp->nlccflag = false;
        if(!s_core_correction.compare(0,5,"FALSE")) sp->nlccflag = false;
        if(!s_core_correction.compare(0,1,"T")) sp->nlccflag = true;
        if(!s_core_correction.compare(0,4,"TRUE")) sp->nlccflag = true;
 
        // Read in the radial mesh and convert it into a C style array
        std::string PP_R = upf_tree.get<std::string>("UPF.PP_MESH.PP_R");
        sp->r = UPF_read_mesh_array(PP_R, sp->rg_points);

        // Determine log mesh parameters directly from the mesh
        sp->aa = (sp->r[0] * sp->r[0]) / (sp->r[1] - 2 * sp->r[0]);
        sp->bb = log (sp->r[1] / sp->r[0] - 1);

        // Read in rab and convert it into a C style array
        std::string PP_RAB = upf_tree.get<std::string>("UPF.PP_MESH.PP_RAB");
        sp->rab = UPF_read_mesh_array(PP_RAB, sp->rg_points);

        // Local potential
        std::string PP_LOCAL = upf_tree.get<std::string>("UPF.PP_LOCAL");
        sp->vloc0 = UPF_read_mesh_array(PP_LOCAL, sp->rg_points);
        // Get into our internal units
        for(int ix = 0;ix < sp->rg_points;ix++) sp->vloc0[ix] /= 2.0;

        // Get the l-value for the local potential if present
        sp->local = upf_tree.get<int>("UPF.PP_HEADER.<xmlattr>.l_local", -3);

        // Atomic charge density
        std::string PP_RHOATOM = upf_tree.get<std::string>("UPF.PP_RHOATOM");
        sp->atomic_rho = UPF_read_mesh_array(PP_RHOATOM, sp->rg_points);
        // UPF stores rhoatom * r^2 so rescale
        for(int ix = 0;ix < sp->rg_points;ix++) sp->atomic_rho[ix] = sp->atomic_rho[ix] / sp->r[ix] / sp->r[ix];

        if(sp->nlccflag) {
            std::string PP_NLCC = upf_tree.get<std::string>("UPF.PP_NLCC");
            sp->rspsco = UPF_read_mesh_array(PP_NLCC, sp->rg_points);

        }

        // Number of atomic orbitals
        sp->num_atomic_waves = upf_tree.get<double>("UPF.PP_HEADER.<xmlattr>.number_of_wfc", 0);
        if(sp->num_atomic_waves  > 0) {

            sp->atomic_wave = new double *[sp->num_atomic_waves];
            sp->awave_lig = new double *[sp->num_atomic_waves];
            for(int iwf = 0;iwf < sp->num_atomic_waves;iwf++) {
                // Ugh. UPF format has embedded .s so use / as a separator
                typedef ptree::path_type path;
                std::string chi = "UPF/PP_PSWFC/PP_CHI." + boost::lexical_cast<std::string>(iwf + 1);
                std::string PP_CHI = upf_tree.get<std::string>(path(chi, '/'));
                sp->atomic_wave[iwf] = UPF_read_mesh_array(PP_CHI, sp->rg_points);
                // UPF stores atomic wavefunctions * r so divide through
                for(int idx = 0;idx < sp->rg_points;idx++) sp->atomic_wave[iwf][idx] /= sp->r[idx];
                sp->awave_lig[iwf] = new double[MAX_LOCAL_LIG];
                
            }

        }

        // Number of projectors
        sp->nbeta = upf_tree.get<double>("UPF.PP_HEADER.<xmlattr>.number_of_proj");
        if(sp->nbeta > 0) {

            for(int ip = 0;ip < sp->nbeta;ip++) {
                // Ugh. UPF format has embedded .s so use / as a separator
                typedef ptree::path_type path;
                std::string betapath = "UPF/PP_NONLOCAL/PP_BETA." + boost::lexical_cast<std::string>(ip + 1);
                std::string PP_BETA = upf_tree.get<std::string>(path(betapath, '/'));
                sp->beta[ip] = UPF_read_mesh_array(PP_BETA, sp->rg_points);
                for(int ix = 1;ix < sp->rg_points;ix++) sp->beta[ip][ix] /= sp->r[ix];
                sp->beta[ip][0] = sp->beta[ip][1];
                sp->llbeta[ip] =  upf_tree.get<int>(path(betapath + "/<xmlattr>/angular_momentum", '/'));
                if(sp->llbeta[ip] > ct.max_l) ct.max_l = sp->llbeta[ip];  // For all species
                if(sp->llbeta[ip] > l_max) l_max = sp->llbeta[ip];        // For this species
//               double cutoff_radius = upf_tree.get<int>(betapath + ".<xmlattr>.cutoff_radius");
                
            }

            /*read in the Matrix ddd0(nbeta,nbeta) */
            std::string PP_DIJ = upf_tree.get<std::string>("UPF.PP_NONLOCAL.PP_DIJ");
            double *tmatrix = UPF_str_to_double_array(PP_DIJ, sp->nbeta*sp->nbeta, 0);
            for (int j = 0; j < sp->nbeta; j++)
            {
                for (int k = 0; k < sp->nbeta; k++)
                {
                    ddd0[j][k] = tmatrix[j*sp->nbeta + k] / 2.0;
                }
            }
            delete [] tmatrix;
          
        }

        sp->nqf=0;
        sp->nlc=0;
        // Charge augmentation for US
        if(!ct.norm_conserving_pp) {
           char q_with_l = upf_tree.get<char>("UPF.PP_NONLOCAL.PP_AUGMENTATION.<xmlattr>.q_with_l");
           sp->nqf = upf_tree.get<int>("UPF.PP_NONLOCAL.PP_AUGMENTATION.<xmlattr>.nqf", 0);
           sp->nlc = upf_tree.get<int>("UPF.PP_NONLOCAL.PP_AUGMENTATION.<xmlattr>.nlqc", 2*l_max + 1);
           int tnlc = (sp->nbeta * (sp->nbeta + 1)) / 2;

           // read in the rinner for Q_L(r) function */
           if(sp->nqf > 0) {
               std::string PP_RINNER = upf_tree.get<std::string>("UPF.PP_NONLOCAL.PP_AUGMENTATION.PP_RINNER");
               sp->rinner = new double[tnlc * sp->nlc * sp->nqf]();
               double *tmatrix = UPF_str_to_double_array(PP_RINNER, sp->nlc, 0);
               for(int i = 0;i < sp->nlc;i++) sp->rinner[i] = tmatrix[i];
               delete [] tmatrix;
           }

           /*read in the Matrix qqq(nbeta,nbeta) */
           std::string PP_Q = upf_tree.get<std::string>("UPF.PP_NONLOCAL.PP_AUGMENTATION.PP_Q");
           double *tmatrix = UPF_str_to_double_array(PP_Q, sp->nbeta*sp->nbeta, 0);
           for (int j = 0; j < sp->nbeta; j++)
           {
               for (int k = 0; k < sp->nbeta; k++)
               {
                   qqq[j][k] = tmatrix[j*sp->nbeta + k];
               }
           }
           delete [] tmatrix;

           // Read in PP_QFCOEFF
           if(sp->nqf > 0) {
               // UPF format calls for full matrix so store in tmatrix
               std::string PP_QFCOEFF = upf_tree.get<std::string>("UPF.PP_NONLOCAL.PP_AUGMENTATION.PP_QFCOEF");
               double *tmatrix1 = UPF_str_to_double_array(PP_QFCOEFF, sp->nqf*sp->nlc*sp->nbeta*sp->nbeta, 0);
               // We only use upper half of the symmetric matrix
               sp->qfcoef = new double[tnlc * sp->nlc * sp->nqf];

               for (int idx1 = 0; idx1 < sp->nbeta; idx1++)
               {
                   for (int idx2 = idx1; idx2 < sp->nbeta; idx2++)
                   {
                       int nmb = idx2 * (idx2 + 1) / 2 + idx1;
                       for (int idx3 = 0; idx3 < sp->nlc; idx3++)
                       {
                           for (int idx4 = 0; idx4 < sp->nqf; idx4++)
                           {
                               int indx = nmb * sp->nlc * sp->nqf + idx3 * sp->nqf + idx4;
                               sp->qfcoef[indx] = tmatrix1[sp->nqf*sp->nlc*sp->nbeta*idx1 + sp->nqf*sp->nlc*idx2 + sp->nqf*idx3 + idx4];
                           }
                       }
                   }
               }

               delete [] tmatrix1;  

               // PP_Q.i.j
               sp->qnm = new double[tnlc * MAX_RGRID]();
               for(int i = 0;i < sp->nbeta;i++) {
                   for(int j = i;j < sp->nbeta;j++) {
                       typedef ptree::path_type path;
                       std::string pp_qij = "UPF/PP_NONLOCAL/PP_AUGMENTATION/PP_QIJ." + boost::lexical_cast<std::string>(i + 1);
                       pp_qij = pp_qij + "." + boost::lexical_cast<std::string>(j + 1);
                       std::string PP_Qij = upf_tree.get<std::string>(path(pp_qij, '/'));
                       double *tmatrix2 = UPF_read_mesh_array(PP_Qij, sp->rg_points);

                       int nmb = j * (j + 1) / 2 + i;
                       for (int idx = 0; idx < sp->kkbeta; idx++)
                       {
                           sp->qnm[nmb * MAX_RGRID + idx] = tmatrix2[idx];
                       }

                       delete [] tmatrix2;
                   }
               }
           }

        }

        for (int j = 0; j < 18; j++)
        {
            sp->nhtol[j] = 0;
            sp->nhtom[j] = 0;
            sp->indv[j] = 0;
        }

        int ih = 0;
        for (int j = 0; j < sp->nbeta; j++)
        {
            int l = sp->llbeta[j];
            for (int k = 0; k < 2 * l + 1; k++)
            {
                sp->nhtol[ih] = l;
                sp->nhtom[ih] = k;
                sp->indv[ih] = j;
                ++ih;
            }
        }
        sp->nh = ih;
        if (ih > max_nlprojectors)
            max_nlprojectors = ih;
        if (max_nlprojectors > MAX_NL)
            rmg_error_handler (__FILE__,__LINE__,"too many nonlocal projectors");

        for (int j = 0; j < ih; j++)
        {
            for (int k = 0; k < ih; k++)
            {
                if ((sp->nhtol[j] == sp->nhtol[k]) && (sp->nhtom[j] == sp->nhtom[k]))
                {
                    sp->ddd0[j][k] = ddd0[sp->indv[j]][sp->indv[k]];
                    sp->qqq[j][k] = qqq[sp->indv[j]][sp->indv[k]];
                    /*                          sp->ddd[j][k]=ddd[indv[j]][indv[k]]; */
                }
                else
                {
                    sp->ddd0[j][k] = 0.0;
                    sp->qqq[j][k] = 0.0;
                    /*                          sp->ddd[j][k]=0.0; */
                }
            }
        }

    }

    catch(std::exception& e)
    {
        std::cout << e.what() << '\n';
        rmg_error_handler(__FILE__,__LINE__,"Problem reading pseudopotential. Terminating.\n");
    }

    // Set the maximum number of non-local projecters needed
    if(max_nlprojectors > ct.max_nl) 
        ct.max_nl = max_nlprojectors;
   
    // Optional stuff next
    
    char *def_desc = "Pseudopotential";
    //sp->description = upf_tree.get<char *>("UPF.PP_HEADER.<xmlattr>.comment", def_desc);

    // Stuff not present in the UPF format that RMG requires. We need to find a consistent way of automatically
    // setting these
    sp->rc = 0.65;
    sp->lradius = 3.3;
    sp->nlradius = 3.3;
    sp->qradius = 3.3;
    sp->lrcut = 2.2;
    sp->rwidth = 8.5; 
    sp->gwidth = 8.0;
    if (ct.runflag == LCAO_START)
    {
        sp->aradius = 9.0;
        sp->acut = 7.0;
        sp->agwidth = 10.0;
        sp->arwidth = 25.0;
    }
    for(int ip = 0;ip < sp->nbeta;ip++) {
        sp->nlrcut[sp->llbeta[ip]] = 3.3;
    }

    // Leftover initializations
    sp->mill_radius = 9.0;
}

// C binding
extern "C" void LoadUpf_C(SPECIES *sp)
{
    LoadUpf(sp);
}

