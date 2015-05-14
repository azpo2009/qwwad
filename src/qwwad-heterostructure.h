/**
 * \file   qwwad-heterostructure.h
 * \author Alex Valavanis <a.valavanis@leeds.ac.uk>
 * \brief  Declarations for heterostructure array generation
 */

#ifndef HETEROSTRUCTURE_H
#define HETEROSTRUCTURE_H

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <string>
#include <valarray>
#include <vector>

namespace QWWAD
{
/// Convenience wrapper for a list of vector components in each layer
typedef std::vector< std::valarray<double> > alloy_vector;

/**
 * \brief Unit of measurement for layer thickness
 */
enum Unit {
    UNIT_NM,       ///< Nanometres
    UNIT_ANGSTROM  ///< angstroms
};

/**
 * \brief A stack of layers making up a quantum heterostructure
 */
class Heterostructure
{
    private:
        static void read_layers_from_file(const std::string     &filename,
                                          alloy_vector          &x_layer,
                                          std::valarray<double> &W_layer,
                                          std::valarray<double> &n3D_layer,
                                          const Unit             thickness_unit = UNIT_NM);

        // Parameters for each individual layer of the structure
        size_t                _n_alloy;   ///< Number of alloy components
        alloy_vector          _x_layer;   ///< Alloy fractions in each layer
        std::valarray<double> _W_layer;   ///< Width of each layer [m]
        std::valarray<double> _n3D_layer; ///< Donor density in each layer [m^{-3}]

        size_t                _n_periods; ///< Number of periods in the structure
        size_t                _nz_1per;   ///< Number of points in each period

        std::valarray<unsigned int> _layer_top_index;

        // Parameters for each point in the entire, expanded structure
        std::valarray<double> _z;   ///< Spatial position at each point [m]
        alloy_vector          _x;   ///< Alloy fractions at each spatial point
        std::valarray<double> _n3D; ///< Volume doping at each point [m^{-3}]
        double                _dz;  ///< Spatial separation between points [m]

    public:
        Heterostructure(const alloy_vector          &x_layer,
                        const std::valarray<double> &W_layer,
                        const std::valarray<double> &n3D_layer,
                        const size_t                 nz_1per,
                        const size_t                 n_periods = 1);

        
        static Heterostructure* create_from_file_auto_nz(const std::string &layer_filename,
                                                       const Unit         thickness_unit,
                                                       const size_t       n_periods,
                                                       const double       dz_max = 1e-10);

        static Heterostructure* create_from_file(const std::string &layer_filename,
                                                 const Unit         thickness_unit,
                                                 const size_t       nz_1per,
                                                 const size_t       n_periods);

        /** Return the number of sampling points in one period of the structure */
        size_t get_nz_1per() const {return _nz_1per;}

        /** Return the total number of sampling points in the entire structure */
        size_t get_nz() const {return _z.size();}

        std::valarray<double> get_z() const {return _z;}
        double                get_z(unsigned int iz) const {return _z[iz];}
        double                get_dz() const {return _dz;}

        std::valarray<double> get_layer_widths() const {return _W_layer;}

        double get_x_in_layer(const unsigned int iL, const unsigned int ialloy) const;
        alloy_vector get_x_array() const {return _x;}

        double get_n3D_in_layer(const unsigned int iL) const;
        double get_n3D_at_point(const unsigned int iz) const;

        /**
         * Return the entire array of doping at each point
         */
         std::valarray<double> get_n3D_array() const {return _n3D;}

        /**
         * \brief Return the number of layers in a single period
         *
         * \returns Number of layers in the structure
         */
        size_t       get_n_layers_per_period() const {return _W_layer.size();}

        /// Return the number of layers in the entire structure
        size_t       get_n_layers_total() const {return _W_layer.size()*_n_periods;}

        unsigned int get_layer_from_height(const double z) const;

        bool         point_is_in_layer(const double z,
                                       const unsigned int iL) const;

        double       get_height_at_top_of_layer(const unsigned int iL) const;

        unsigned int get_layer_top_index(const unsigned int iL) const;
        std::valarray<unsigned int> get_layer_top_indices() const {return _layer_top_index;}

        /// Return the length of a single period of the structure
        double       get_period_length() const {return _W_layer.sum();}

        /// Return the entire length of the structure
        double       get_total_length() const {return _W_layer.sum()*_n_periods;}
};
} // namespace
#endif // HETEROSTRUCTURE_H
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
