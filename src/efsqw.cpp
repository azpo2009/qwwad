/**
 * \file    efsqw.cpp
 * \brief   Calculate the 1-particle energies and wavefunctions in a single quantum well
 * \author  Paul Harrison  <p.harrison@shu.ac.uk>
 * \author  Alex Valavanis <a.valavanis@leeds.ac.uk>
 *
 * \details One particle energies and wavefunctions in a single
 *          quantum well.  Three basic theoretical approaches are
 *          contained within this sourcecode
 *
   (i) Constant mass

   \f[
   -\frac{\hbar^2}{2m^*} \frac{\mathrm{d}^2}{\mathrm{d}z^2}\psi + V(z) \psi = E \psi,   -\frac{l_w}{2} < z < +\frac{l_w}{2},  V=0
   \f]

   (ii) Different masses in well and barrier

   \f[
   -\frac{\hbar^2}{2m^*} \frac{\mathrm{d}^2}{\mathrm{d}z^2}\psi + V(z) \psi = E \psi,   -l_w < z < +l_w,  V=0
   \f]

   with the additional constraint of the boundary conditions

   \f[
   \psi \mathrm{and} \frac{1}{m}\frac{d\psi}{dz}, \mathrm{continuous}
   \f]
 
   this represents the Hamiltonian \f$P_z\frac{1}{m] P_z + V\f$

   (iii) Different masses in well and barrier

   \f[
   -\frac{\hbar^2}{2m^*} \frac{\mathrm{d}^2}{\mathrm{d}z^2}\psi + V(z) \psi = E \psi,   -l_w < z < +l_w,  V=0
   \f]

   with the additional constraint of the boundary conditions

   \f[
   \psi \mathrm{and} \frac{d\psi}{dz}, \mathrm{continuous}
   \f]

   this represents the Hamiltonian \f$\frac{1}{\sqrt{m}}P_zP_z\frac{1}{\sqrt{m}] + V\f$

   The code is based around point (ii). Point (i) is 
   implemented by the user selecting m_b=m_w.  Point
   (iii) is implemented by allowing different m_b and m_w
   in the evaluation of k and K, but m_b is artificially
   forced to equal m_w for the boundary conditions.

   The system is solved by expressing the
   standard condition as a function f(x)=0 and
   implementing a Newton-Raphson iterative method
   where the independent variable x is the energy.
   The first estimate of the energy is found by 
   following the function along the x axis until it
   changes sign then using a midpoint rule.
*/

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <valarray>
#include "qclsim-constants.h"
#include "qwwad-schroedinger-finite-well.h"
#include "qwwad-options.h"

using namespace Leeds;
using namespace constants;

/**
 * Configure command-line options for the program
 */
Options configure_options(int argc, char* argv[])
{
    Options opt;
    opt.add_numeric_option("well-width,a",    100,   "Width of quantum well [angstrom].");
    opt.add_numeric_option("barrier-width,b", 200,   "Width of barrier [angstrom]. Note that this is only used "
                                                     "for the purposes of outputting the data. The calculation here "
                                                     "assumes that the barriers are infinitely thick.  As such, the "
                                                     "wavefunctions do not decay to precisely zero at the boundaries.");
    opt.add_numeric_option("well-mass,m",     0.067, "Effective mass in well (relative to that of a free electron)");
    opt.add_numeric_option("barrier-mass,n",  0.067, "Effective mass in barrier (relative to that of a free electron)");
    opt.add_switch        ("alt-KE,k",               "Use alternative kinetic energy operator (1/m)PP");
    opt.add_switch        ("output-equations",       "Output the matching equations for the system. The left-hand "
                                                     "side of the equation is output to 'lhs.r' and each branch of "
                                                     "the right-hand side is output to a set of 'rhs_i.r' files, "
                                                     "where 'i' is the index of the state that lies on that branch. "
                                                     "An rhs file is output for all the bound states in the system and "
                                                     "one additional branch (with no real solution)");
    opt.add_switch        ("output-potential",       "Output the potential profile for the system to v.r");
    opt.add_char_option   ("particle,p",       'e',  "ID of particle to be used: 'e', 'h' or 'l', for electrons, heavy holes or light holes respectively.");
    opt.add_size_option   ("nz,N",             1000, "Number of spatial points for output file.");
    opt.add_size_option   ("nst,s",              1,  "Number of states to find");
    opt.add_numeric_option("potential",        100,  "Barrier potential [meV]");
    opt.add_numeric_option("E-cutoff",          "Cut-off energy for solutions [meV]");

    std::string summary("Find the eigenstates of a single finite quantum well. ");
    std::string details("The following output text files are created:\n"
                        "  'E*.r'   \tEnergy of each state:\n"
                        "           \tCOLUMN 1: state index.\n"
                        "           \tCOLUMN 2: energy [meV].\n"
                        "  'wf_*i.r'\tWave function amplitude at each position\n"
                        "           \tCOLUMN 1: position [m].\n"
                        "           \tCOLUMN 2: wave function amplitude [m^{-1/2}].\n"
                        "  'v.r'    \tPotential profile (if --output-potential flag is used)\n"
                        "           \tCOLUMN 1: position [m].\n"
                        "           \tCOLUMN 2: potential [J].\n"
                        "  'lhs.r'  \tLeft-hand side of matching function (if --output-equations flag is used)\n"
                        "           \tCOLUMN 1: Normalised well wave-vector\n"
                        "           \tCOLUMN 2: Normalised barrier decay constant\n"
                        "  'rhs_i.r'\tRight-hand side of matching function for branch i (if --output-equations flag is used)\n"
                        "           \tCOLUMN 1: Normalised well wave-vector\n"
                        "           \tCOLUMN 2: Normalised barrier decay constant\n"
                        "\n"
                        "\tIn each case, the '*' is replaced by the particle ID and the 'i' is replaced by the number of the state.\n"
                        "\n"
                        "Examples:\n"
                        "   Compute the first three states in a 150-angstrom well with 100 meV confining potential:\n\n"
                        "   efsqw --well-width 150 --potential 100 --nst 3\n"
                        "\n"
                        "   Compute the first three heavy-hole states in a 200-angstrom well, using effective mass = 0.62 m0:\n\n"
                        "   efsqw --well-width 200 --well-mass 0.62 --particle h\n"
                        "\n"
                        "   Compute the ground state in a 200 angstrom well with 100 meV barriers, and dump plots of the matching equations to file:\n\n"
                        "   efsqw --well-width 200 --potential 100 --output-equations");

    opt.add_prog_specific_options_and_parse(argc, argv, summary, details);

    return opt;
}

int main(int argc,char *argv[])
{
    Options opt = configure_options(argc, argv);
    const double a      = opt.get_numeric_option("well-width") * 1e-10;
    const double b      = opt.get_numeric_option("barrier-width") * 1e-10;
    const double m_w    = opt.get_numeric_option("well-mass") * me;
    const double m_b    = opt.get_numeric_option("barrier-mass") * me;
    const char   p      = opt.get_char_option("particle");         // particle ID (e, h or l)
    const double V      = opt.get_numeric_option("potential") * e / 1000;
    const size_t state  = opt.get_size_option("nst");
    const size_t N      = opt.get_size_option("nz");               // number of spatial steps
    const bool   T_flag = !opt.get_switch("alt-KE");

    SchroedingerSolverFiniteWell se(a, b, V, m_w, m_b, N, T_flag,state);

    // Set cut-off energy if desired
    if(opt.vm.count("E-cutoff") > 0)
        se.set_E_cutoff(opt.get_numeric_option("E-cutoff") * e/1000);

    if(opt.get_switch("output-equations"))
    {
        const size_t nst    = se.get_n_bound();
        const double v_max  = (nst+1)*pi/2;

        const size_t nv = (nst+1)*1000;
        const double dv = v_max/(nv-1);
        std::valarray<double> v(nv);
        std::valarray<double> lhs(nv);

        // Output lhs data in a single contiguous chunk
        for (unsigned int iv = 0; iv < nv; ++iv)
        {
            v[iv]   = iv*dv;
            lhs[iv] = se.get_lhs(v[iv]);
        }

        Leeds::write_table_xy("lhs.r", v, lhs);

        // Output a separate file for each rhs branch
        for (unsigned int ibranch = 0; ibranch < nst+1; ++ibranch)
        {
            const size_t nv_branch = 1000;

            std::valarray<double> v_branch(nv_branch);
            std::valarray<double> rhs(nv_branch);

            // Set the spacing between points so that we don't quite reach the
            // asymptote at the "end" of the branch
            const double dv_branch = (pi/2.0*0.999999)/(nv_branch-1);

            for (unsigned int iv_branch = 0; iv_branch < nv_branch; ++iv_branch)
            {
                v_branch[iv_branch] = pi/2.0 * ibranch  + iv_branch*dv_branch;
                rhs[iv_branch]      = se.get_rhs(v_branch[iv_branch]);
            }

            // Set filename
            std::ostringstream oss;
            oss << "rhs_" << ibranch+1 << ".r";
            Leeds::write_table_xy(oss.str().c_str(), v_branch, rhs);
        }
    }

    // Dump to file
    char energy_filename[9];
    sprintf(energy_filename,"E%c.r",p);

    char wf_prefix[9];
    sprintf(wf_prefix,"wf_%c",p);
    State::write_to_file(energy_filename,
                         wf_prefix,
                         ".r",
                         se.get_solutions(true),
                         se.get_z(),
                         true);

    // Write potential profile to file if wanted
    if(opt.get_switch("output-potential"))
    {
        const size_t nz = se.get_solutions()[0].size();
        const double Lp = a + b*2;
        const double dz = Lp/(nz-1);
        std::valarray<double> z(nz);
        std::valarray<double> V_out(nz);

        for(unsigned int iz = 0; iz < nz; ++iz)
        {
            z[iz] = iz*dz;

            if(z[iz] < b || z[iz] >= a+b)
                V_out[iz] = V;
        }

        Leeds::write_table_xy("v.r", z, V_out);
    }

    return EXIT_SUCCESS;
}
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
