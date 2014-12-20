/*===================================================================
	efiwire Envelope Function Infinite Wire
===================================================================*/

/* This program calculates the eigenfunctions and eigenenergies of
   an infinitely deep rectangular cross-section quantum wire. The 
   relevant parameters are passed via command line arguments.

   Paul Harrison, November 1998				 */

#include <cstdio>
#include <cstdlib>
#include <cmath>

#include <gsl/gsl_math.h>

#include "qclsim-fileio.h"
#include "qclsim-constants.h"
#include "qwwad-options.h"
using namespace Leeds;
using namespace constants;

int main(int argc, char **argv)
{
    Options opt;

    std::string doc("Find eigenstates of an infinite rectangular quantum wire.");

    opt.add_numeric_option("ywidth,y",   100,   "Width of quantum wire in y-direction [angstrom].");
    opt.add_numeric_option("zwidth,z",   100,   "Width of quantum wire in z-direction [angstrom].");
    opt.add_numeric_option("mass,m",     0.067, "Effective mass (relative to free electron).");
    opt.add_size_option   ("nz,N",       100,   "Number of spatial points for output file.");
    opt.add_size_option   ("nst,s",      1,     "Number of states to find.");
    opt.add_char_option   ("particle,p", 'e',   "ID of particle to be used: 'e', 'h' or 'l', for "
                                                "electrons, heavy holes or light holes respectively.");

    opt.add_prog_specific_options_and_parse(argc, argv, doc);
char	filename[9];	/* wavefunction filename	*/

    const double Ly = opt.get_numeric_option("ywidth") * 1e-10; // wire width in y-direction [m]
    const double Lz = opt.get_numeric_option("zwidth") * 1e-10; // wire width in z-direction [m]
    const char   p  = opt.get_char_option("particle");          // particle ID (e, h or l)
    const double m  = opt.get_numeric_option("mass") * me;      // effective mass [kg]
    const size_t N  = opt.get_size_option("nz");                // number of spatial steps
    const size_t s  = opt.get_size_option("nst");               // number of states

    std::valarray<unsigned int> y_index(s*s); // y-index of each state
    std::valarray<unsigned int> z_index(s*s); // z-index of each statee
    std::valarray<double>       E(s*s);       // Energy of each state [meV]

// Loop over all y and z state indices
unsigned int ist = 0;

for(unsigned int in_y=1;in_y<=s;in_y++)
    for(unsigned int in_z=1;in_z<=s;in_z++)
    {
        y_index[ist] = in_y;
        z_index[ist] = in_z;
        E[ist]       = gsl_pow_2(pi*hBar)/(2*m)*(gsl_pow_2(in_y/Ly)+gsl_pow_2(in_z/Lz)) /(1e-3*e);

        std::valarray<double> y(N*N);
        std::valarray<double> z(N*N);
        std::valarray<double> psi(N*N);

        const double dy = Ly/(N-1);
        const double dz = Ly/(N-1);

        unsigned int izy = 0;

        // Loop over 2D space
        for(unsigned int iy=0;iy<N;iy++)
        {
            y[izy] = (float)iy*dy;
            const double psi_y=sqrt(2/Ly)*sin(in_y*pi*y[izy]/Ly);

            for(unsigned int iz=0;iz<N;iz++)
            {
                z[izy] = (float)iz*dz;
                const double psi_z=sqrt(2/Lz)*sin(in_z*pi*z[izy]/Lz);
                psi[izy] = psi_y * psi_z;

                ++izy;
            }
        }

        sprintf(filename,"cd%i%i.r",in_y,in_z);
        write_table_xyz(filename, y, z, psi);

        ++ist; // Increment the overall state index
    }/* end in_z */

sprintf(filename,"E%c.r",p);
write_table_xyz(filename, y_index, z_index, E);

return EXIT_SUCCESS;
}