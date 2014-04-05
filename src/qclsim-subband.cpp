/**
 * \file   qclsim-subband.cpp
 * \brief  A subband in a 2D system
 * \author Alex Valavanis <a.valavanis@leeds.ac.uk>
 * \date   2013-01-10
 */
#include "qclsim-subband.h"
#include "qclsim-fileio.h"
#include "qclsim-maths.h"
#include "qclsim-constants.h"

namespace Leeds {
using namespace constants;

Subband::Subband(State ground_state,
                 double Ef,
                 double population,
                 double md_0,
                 std::valarray<double> z) :
    ground_state(ground_state),
    _z(z),
    Ef(Ef),
    population(population),
    md_0(md_0),
    alphad(0.0),
    condband_edge(0.0)
{
}

Subband::Subband(State ground_state,
                 double Ef,
                 double population,
                 double md_0,
                 std::valarray<double> z,
                 double alphad,
                 double condband_edge) :
    ground_state(ground_state),
    _z(z),
    Ef(Ef),
    population(population),
    md_0(md_0),
    alphad(alphad),
    condband_edge(condband_edge)
{
}

/** Find Fermi wave-vector [1/m] */
double Subband::get_k_fermi() const
{
    return sqrt(2.0*pi*population);
}

/**
 * Reads a set of subbands from data files (not including nonparabolic dispersion)
 *
 * \param[in] energy_input_path     Path for energy and wavefunction files
 * \param[in] wf_input_prefix       Prefix for wavefunction filenames
 * \param[in] wf_input_ext          Extension for wavefunction filenames
 * \param[in] populations_filename  Name of data file containing populations
 * \param[in] fermienergy_filename  Name of data file containing Fermi energies
 * \param[in] m_d_filename          Name of data file containing density-of-states
 *                                  effective mass.
 */
std::vector<Subband> Subband::read_from_file(const std::string& energy_input_path,
                                             const std::string& wf_input_prefix,
                                             const std::string& wf_input_ext,
                                             const std::string& populations_filename,
                                             const std::string& fermienergy_filename,
                                             const std::string& m_d_filename)
{
    // Read subband data
    std::valarray<double> ist_temp;
    std::valarray<double> Ef;
    std::valarray<double> P;
    std::valarray<double> z;
    std::valarray<double> m_d_z;
    
    std::vector<State> ground_state = State::read_from_file(energy_input_path,
                                                            wf_input_prefix,
                                                            wf_input_ext);

    Leeds::read_table_x(populations_filename.c_str(), P);
    Leeds::read_table_xy(fermienergy_filename.c_str(), ist_temp, Ef);
    Leeds::read_table_xy(m_d_filename.c_str(), z, m_d_z);
    
    const size_t nst = ground_state.size();

    // Check that all state input files contain same amount of data
    if(P.size() != nst or Ef.size() != nst)
    {
        std::ostringstream oss;
        oss << "Incorrect amount of data in " << populations_filename
            << " (" << P.size()  << " lines) or " << fermienergy_filename
            << " (" << Ef.size() << " lines). Expected " << nst << " lines.";
        throw std::runtime_error(oss.str());
    }

    std::valarray<unsigned int> psi_max_iz(nst);

    // Check that populations look sensible
    for(unsigned int ist = 0; ist < nst; ist++){
        Leeds::check_positive(&P[ist]);

        // Use the value of in-plane mass where wavefunction has max. magnitude
        // (I.e. in the well!)
        // TODO Is there a better way way of doing this?
        double psi_max = 0.0;
        for(unsigned int iz = 0; iz<ground_state[ist].size(); iz++){
            if(fabs(ground_state[ist].psi(iz)) > psi_max){
                psi_max = fabs(ground_state[ist].psi(iz));
                psi_max_iz[ist] = iz;
            }
        }
    }
    
    // Copy subband data to vector
    std::vector<Subband> subbands;

    for (unsigned int ist = 0; ist < nst; ist++)
        subbands.push_back(Subband(ground_state[ist], Ef[ist], P[ist], m_d_z[psi_max_iz[ist]], z));

    return subbands;
}

/**
 * Reads a set of subbands from data files (including nonparabolic dispersion)
 *
 * \param[in] energy_input_path     Path for energy and wavefunction files
 * \param[in] wf_input_prefix       Prefix for wavefunction filenames
 * \param[in] wf_input_ext          Extension for wavefunction filenames
 * \param[in] populations_filename  Name of data file containing populations
 * \param[in] fermienergy_filename  Name of data file containing Fermi energies
 * \param[in] m_d_filename          Name of data file containing density-of-states effective mass.
 * \param[in] alphad_filename       Name of data file containing dispersion nonparabolicity profile
 * \param[in] potential_filename    Name of data file containing band edge potential.
 */
std::vector<Subband> Subband::read_from_file(const std::string& energy_input_path,
                                             const std::string& wf_input_prefix,
                                             const std::string& wf_input_ext,
                                             const std::string& populations_filename,
                                             const std::string& fermienergy_filename,
                                             const std::string& m_d_filename,
                                             const std::string& alphad_filename,
                                             const std::string& potential_filename)
{
    std::vector<Subband> subbands; // Output structure
    
    // Read subband data
    std::valarray<double> ist_temp;
    std::valarray<double> Ef;
    std::valarray<double> P;
    std::valarray<double> z;
    std::valarray<double> m_d_z;
    std::valarray<double> alphad;
    std::valarray<double> V;
    
    std::vector<State> ground_state = State::read_from_file(energy_input_path,
                                                            wf_input_prefix,
                                                            wf_input_ext);
    Leeds::read_table_x(populations_filename.c_str(), P);
    Leeds::read_table_xy(fermienergy_filename.c_str(), ist_temp, Ef);
    Leeds::read_table_xy(m_d_filename.c_str(), z, m_d_z);
    Leeds::read_table_xy(alphad_filename.c_str(), z, alphad);
    Leeds::read_table_xy(potential_filename.c_str(), z, V);

    const size_t nst = ground_state.size();
    std::valarray<unsigned int> psi_max_iz(nst);    
    // Check that populations look sensible
    for(unsigned int ist = 0; ist < nst; ist++){
        Leeds::check_positive(&P[ist]);

        // Use the value of in-plane mass where wavefunction has max. magnitude
        // (I.e. in the well!)
        // TODO Is there a better way way of doding this?
        double psi_max = 0.0;
        for(unsigned int iz = 0; iz<ground_state[ist].size(); iz++){
            if(fabs(ground_state[ist].psi(iz)) > psi_max){
                psi_max = fabs(ground_state[ist].psi(iz));
                psi_max_iz[ist] = iz;
            }
        }
    }
    
    // Check that all state input files contain same amount of data
    if(P.size() != nst or Ef.size() != nst)
    {
        std::ostringstream oss;
        oss << "Incorrect amount of data in " << populations_filename
            << " (" << P.size()  << " lines) or " << fermienergy_filename
            << " (" << Ef.size() << " lines). Expected " << nst << " lines.";
        throw std::runtime_error(oss.str());
    }
    
    // Copy subband data to vector
    for (unsigned int ist = 0; ist < nst; ist++)
        subbands.push_back(Subband(ground_state[ist], Ef[ist], P[ist], m_d_z[psi_max_iz[ist]], z,
                                alphad[psi_max_iz[ist]], V[psi_max_iz[ist]]));

    return subbands;
}

/** Return the energy above subband minima at some wavevector */
double Subband::Ek(double k) const
{
    //TODO Make this configurable?
    // Numerical error allowed in calcultion
    double numerical_error = 1e-9;
    // First check if wavevector is zero. I.e. at subband minima
    if(fabs(k) < numerical_error)
        return 0.0;

    double Ek;
    // Check if subband is initialised as being nonparabolic
    if(alphad == 0.0)
        Ek = gsl_pow_2(k*hBar)/(2.0*md_0);
    else
    {
        // b
        const double b = 1+alphad*(get_E() - condband_edge);
        // 4*a*c
        const double four_ac = 4*alphad*(-gsl_pow_2(hBar*k)/(2*md_0));

        // Check solveable
        if(four_ac > b*b)
        {
            std::ostringstream oss;
            oss << "No real energy solution exists at wavevector k = " << k*1.0e-9 << " nm^{-1}.";
            throw std::domain_error(oss.str());
        }

        const double root = sqrt(b*b - four_ac);
        if(root > b)
            Ek = (-b + root)/(2*alphad);
        else
        {
            std::ostringstream oss;
            oss << "Negative energy found at wavevector k = " << k*1.0e-9 << " nm^{-1}.";
            throw std::domain_error(oss.str());
        }
    }

    return Ek;
}

/**
 * Return the wavevector at some energy above subband minima
 *
 * \param[in] Ek Kinetic energy [J]
 * 
 * \return Wavevector [m^{-1}]
 */
double Subband::k(const double Ek) const
{
    if(Ek < 0.0)
    {
        std::ostringstream oss;
        oss << "Cannot find wavevector at negative kinetic energy, Ek = " << Ek/e*1000 << " meV.";
        throw std::domain_error(oss.str());
    }

    // Check if at subband minima
    // TODO Make numerical error allowed for configurable?
    double numerical_error = 1e-9*e;
    if(fabs(Ek) < numerical_error)
        return 0.0;

    double k;
    // Check if subband is initialised as being nonparabolic
    if(alphad == 0.0)
        k = sqrt(Ek*2.0*md_0)/hBar;
    else
        k = sqrt(Ek*2.0*md_0*(1 + alphad*(get_E() + Ek - condband_edge)))/hBar;
    
    return k;
} 

/// Return 2D density of states for subband
double Subband::rho(const double E) const
{
    return get_m_d(E)/(pi*hBar*hBar);
}
        
/// Fermi-dirac statistics at some energy
double Subband::f_FD(const double E, const double Te) const
{
    return 1.0/(exp((E-Ef)/(kB*Te)) + 1.0);
}
} // namespace Leeds
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :