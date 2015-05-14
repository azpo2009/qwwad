/**
 * \file   ado.cpp
 * \brief  Alloy disorder scattering rate solver
 * \author Alex Valavanis <a.valavanis@leeds.ac.uk>
 */

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <sstream>
#include <iostream>
#include <gsl/gsl_math.h>
#include "qwwad/constants.h"
#include "qclsim-subband.h"
#include "qwwad-options.h"

using namespace QWWAD;
using namespace constants;

Options configure_options(int argc, char* argv[])
{
    Options opt;

    std::string doc("Find the alloy disorder scattering rate.");

    opt.add_switch        ("noblocking,S",         "Disable final-state blocking.");
    opt.add_numeric_option("Vad",             600, "Alloy disorder potential [meV]");
    opt.add_numeric_option("cellfraction",      4, "Fraction of unit cell occupied by each scatterer");
    opt.add_numeric_option("latticeconst",   5.65, "Lattice constant in growth direction [angstrom]");
    opt.add_numeric_option("mass,m",        0.067, "Band-edge effective mass (relative to free electron)");
    opt.add_char_option   ("particle,p",      'e', "ID of particle to be used: 'e', 'h' or 'l', for "
                                                   "electrons, heavy holes or light holes respectively.");
    opt.add_numeric_option("temperature,T",   300, "Temperature of carrier distribution.");
    opt.add_numeric_option("Ecutoff",              "Cut-off energy for carrier distribution [meV]. If not specified, then 5kT above band-edge.");
    opt.add_size_option   ("nki",             101, "Number of initial wave-vector samples.");

    opt.add_prog_specific_options_and_parse(argc, argv, doc);

    return opt;
}

int main(int argc,char *argv[])
{
    const Options opt = configure_options(argc, argv);

    const double Vad     = opt.get_numeric_option("Vad")*e/1000;   // Alloy-disorder potential [J]
    const double Ncell   = opt.get_numeric_option("cellfraction"); // Fraction of cell occupied by each scatterer
    const double alatt   = opt.get_numeric_option("latticeconst") * 1e-10; // Lattice constant [m]
    const double m       = opt.get_numeric_option("mass")*me;      // Band-edge effective mass [kg]
    const char   p       = opt.get_char_option("particle");  	   // Particle ID
    const double T       = opt.get_numeric_option("temperature");  // Temperature [K]
    const bool   b_flag  = !opt.get_switch("noblocking");          // Include final-state blocking by default
    const size_t nki     = opt.get_size_option("nki");             // number of ki calculations

    std::ostringstream E_filename; // Energy filename string
    E_filename << "E" << p << ".r";
    std::ostringstream wf_prefix;  // Wavefunction filename prefix
    wf_prefix << "wf_" << p;

    // Read data for all subbands from file
    std::vector<Subband> subbands = Subband::read_from_file(E_filename.str(),
                                                            wf_prefix.str(),
                                                            ".r",
                                                            m);

    // Read and set carrier distributions within each subband
    std::valarray<double>       Ef;      // Fermi energies [J]
    std::valarray<double>       N;       // Subband populations [m^{-2}]
    std::valarray<unsigned int> indices; // Subband indices (garbage)
    read_table("Ef.r", indices, Ef);
    Ef *= e/1000.0; // Rescale to J
    read_table("N.r", N);	// read populations

    for(unsigned int isb = 0; isb < subbands.size(); ++isb)
        subbands[isb].set_distribution(Ef[isb], N[isb]);

    // Read alloy profile
    std::valarray<double> z;
    std::valarray<double> x;
    read_table("x.r", z, x);

    // Read list of wanted transitions
    std::valarray<unsigned int> i_indices;
    std::valarray<unsigned int> f_indices;

    read_table("rrp.r", i_indices, f_indices);

    FILE *Favg=fopen("ado-avg.dat","w"); // open file for output of weighted means

    // Loop over all desired transitions
    for(unsigned int itx = 0; itx < i_indices.size(); ++itx)
    {
        // State indices for this transition (NB., these are indexed from 1)
        unsigned int i = i_indices[itx];
        unsigned int f = f_indices[itx];

        // Convenience labels for each subband (NB., these are indexed from 0)
        const Subband isb = subbands[i-1];
        const Subband fsb = subbands[f-1];

        // Subband minima
        const double Ei = isb.get_E();
        const double Ef = fsb.get_E();

        // Find minimum initial wave-vector that allows scattering
        const double Efi = Ef - Ei;
        double kimin = 0.0;
        if(Efi > 0)
            kimin = sqrt(2*m*Efi)/hBar;

        double kimax = 0;
        double Ecutoff = 0.0; // Maximum kinetic energy in initial subband

        // Use user-specified value if given
        if(opt.vm.count("Ecutoff"))
        {
            Ecutoff = opt.get_numeric_option("Ecutoff")*e/1000;

            if(Ecutoff+Ei < Ef)
            {
                std::cerr << "No scattering permitted from state " << i << "->" << f << " within the specified cut-off energy." << std::endl;
                std::cerr << "Extending range automatically" << std::endl;
                Ecutoff += Ef;
            }
        }
        // Otherwise, use a fixed, 5kT range
        else
        {
            kimax   = isb.get_k_max(T);
            Ecutoff = hBar*hBar*kimax*kimax/(2*m);

            if(Ecutoff+Ei < Ef)
                Ecutoff += Ef;
        }

        kimax = isb.k(Ecutoff);

        const double dki=(kimax-kimin)/((float)nki - 1); // step length for loop over ki

        std::valarray<double> Wbar_integrand_ki(nki); // initialise integral for average scattering rate
        std::valarray<double> Wif(nki);               // Scattering rate for a given initial wave vector
        std::valarray<double> Ei_t(nki);              // Total energy of initial state (for output file) [meV]

        // Find alloy-disorder matrix element
        const std::valarray<double> psi_i = isb.psi_array();
        const std::valarray<double> psi_f = fsb.psi_array();
        const std::valarray<double> integrand_dz = psi_i*psi_i*psi_f*psi_f*x*(1.0-x);
        const double dz = z[1] - z[0];
        const double Omega = alatt*alatt*alatt/Ncell;
        const double I = m*Omega*Vad*Vad/(hBar*hBar*hBar) * integral(integrand_dz, dz);

        // calculate scattering rate for all ki
        for(unsigned int iki=0;iki<nki;iki++)
        {
            const double ki=kimin+dki*iki; // carrier momentum
            const double ki_sqr = ki*ki;

            // Find energy-conserving final wave-vector
            // This should be positive if the kimin value is correct
            const double kf_sqr = ki_sqr + 2*m*(Ei - Ef)/(hBar*hBar);
            assert(kf_sqr >= 0.0);
            const double kf = sqrt(kf_sqr);
            assert(!isnan(kf));

            Wif[iki] = I; // Scattering rate is same at all wave-vectors

            // Include final-state blocking factor
            if (b_flag)
                Wif[iki] *= (1 - fsb.f_FD_k(kf, T));

            Ei_t[iki] = isb.E_total(ki) * 1000/e;

            /* calculate Fermi-Dirac weighted mean of scattering rates over the 
               initial carrier states, note that the integral step length 
               dE=2*sqr(hBar)*ki*dki/(2m)					*/
            Wbar_integrand_ki[iki] = Wif[iki]*ki*isb.f_FD_k(ki, T);
        } /* end ki	*/

        /* output scattering rate versus carrier energy=subband minima+in-plane
           kinetic energy						*/
        char	filename[9];	/* character string for output filename		*/
        sprintf(filename,"ado%i%i.r",i,f);
        write_table(filename, Ei_t, Wif);

        const double Wbar = integral(Wbar_integrand_ki, dki)/(pi*isb.get_pop());

        fprintf(Favg,"%i %i %20.17le\n", i,f,Wbar);
} /* end while over states */

fclose(Favg);	/* close weighted mean output file	*/

return EXIT_SUCCESS;
} /* end main */
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
