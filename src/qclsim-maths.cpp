/**
 * \file   qclsim-maths.cpp
 * \brief  Mathematical utility functions for QCLsim
 * \author Alex Valavanis <a.valavanis@leeds.ac.uk>
 * \date   2012-03-14
 */

#include "qclsim-maths.h"
#include <stdexcept>
#include <gsl/gsl_math.h>
#include <gsl/gsl_sf_gamma.h>
#include <sstream>

namespace Leeds
{

/**
 * \brief Compute a numerical integral using a sensible solver
 *
 * \param[in] y  Samples of the function to be integrated
 * \param[in] dx Spatial step between samples
 *
 * \details If the number of samples is odd, and >=3, a Simpson's rule solver
 *          will be used. This gives higher precision than the fallback
 *          trapezium rule solver. The method is selected automatically, based
 *          on the number of samples in the function. This should only result
 *          in a small time overhead, so it should generally be fine to use
 *          this function instead of directly calling trapz or simps.
 */
double integral(const std::valarray<double>& y, const double dx)
{
    const size_t n = y.size();

    if(n < 2)
        throw std::runtime_error("Need at least two points for numerical integration.");

    if(GSL_IS_ODD(n) && n >= 3)
        return simps(y, dx);
    else
        return trapz(y, dx);
}

/**
 * \brief Integrate using Simpson's rule
 *
 * \param[in] y  Samples of the function to be integrated
 * \param[in] dx Spatial step between samples
 *
 * \details The number of samples must be odd, and >= 3
 *
 * \returns The integral
 */
double simps(const std::valarray<double>& y, const double dx)
{
    const size_t n = y.size();

    if(n < 3)
        throw std::runtime_error("Not enough points for Simpson's rule");

    if(GSL_IS_EVEN(n))
    {
        std::ostringstream oss;
        oss << "Simpson's rule needs odd number of points: " << n << " received.";
        throw std::length_error(oss.str());
    }

    double ans=0;
    for(unsigned int i=0; i<n-2; i+=2)
        ans += y[i] + 4.0*y[i+1] + y[i+2];

    ans *= dx/3.0;
    return ans;
}


/**
 * \brief Integrate using the trapezium rule
 *
 * \param[in] y  Samples of the function to be integrated
 * \param[in] dx Spatial step between samples
 *
 * \details The number of samples must be >= 2
 *
 * \returns The integral
 */
double trapz(const std::valarray<double>& y, const double dx)
{
    const size_t n = y.size();

    if(n < 2)
        throw std::runtime_error("Need at least two points for trapezium rule");

    double ans=0;

    for(unsigned int i=0; i<n-1; i++)
        ans += (y[i] + y[i+1])/2.0;

    ans *= dx;

    return ans;
}

/**
 * \brief      Interpolates y=f(x) between f(0) and f(1)
 *
 * \param[in]  y0 y-value for f(x=0)
 * \param[in]  y1 y-value for f(x=1)
 * \param[in]  x  x-value for which to find y=f(x)
 * \param[in]  b  An optional "bowing factor"
 *
 * \details  x must lie in the range 0 < x < 1.
 *           The interpolation is computed using
 *
 *           f(x) = (1-x)f(0) + xf(1) + x(1-x)b
 *
 * \return y = f(x)
 */
double lin_interp(const double y0,
                  const double y1,
                  const double x,
                  const double b){
    if(x < 0 or x > 1)
        throw std::domain_error("x value out of range");

    return y0*(1.0-x) + y1*x + b*x*(1.0-x);
}

/**
 * Looks up a y value in a table of the form y=f(x)
 *
 * \param[in] x_values  The x values, in ascending order
 * \param[in] y_values  The y values, in ascending order
 * \param[in] x0        The desired x value for which to find y
 * 
 * \returns The y-value that corresponds to x0
 *
 * \details Linear interpolation is used to find the value more accurately
 *
 * \todo This is a very inefficient way of doing it.  Use splines!
 */
double lookup_y_from_x(const std::valarray<double>& x_values,
        const std::valarray<double>& y_values,
        const double x0)
{
    if (x0 > x_values.max() or x0 < x_values.min())
    {
        std::ostringstream oss;
        oss << "Desired x value: " << x0 << " is out of range (" << x_values.min() << ", " << x_values.max() << ").";
        throw std::domain_error(oss.str());
    }

    unsigned int ix=0;
    while (x0 >= x_values[ix])
        ix++;

    if (ix==0)
        return y_values[0];
    else
        return y_values[ix-1] + (y_values[ix] - y_values[ix-1]) * (x0 - x_values[ix-1])/(x_values[ix] - x_values[ix-1]);
}

/**
 * Initialise the Laplace solver
 *
 * \details Generates the Stehfest coefficients
 */
Laplace::Laplace() :
    V(N)
{
    if(N % 2 != 0)
        throw std::domain_error("Laplace inversion algorithm must have even number of samples");

    const size_t N2 = N / 2;
    
    for (unsigned int i = 0; i < N; i++) 
    { 
        const int kmin = (i + 2) / 2; 
        const int kmax = GSL_MIN(i + 1, N2); 
        
        for (int k = kmin; k <= kmax; k++) 
        { 
            V[i] += pow(k, N2) * gsl_sf_fact(2 * k) / (  gsl_sf_fact(k) 
                                                       * gsl_sf_fact(2 * k - i - 1)
                                                       * gsl_sf_fact(N2 - k)
                                                       * gsl_sf_fact(k - 1)
                                                       * gsl_sf_fact(i + 1 - k)); 
        } 
    
        V[i] *= pow(-1, static_cast<double>(N2 + i + 1));
    } 
}

/**
 * Find the inverse Laplace transform of a function at a given time
 *
 * \details Beware... this only gives decent results if the time-domain
 *          representation of the function is smoothly varying.  i.e. things
 *          that you expect to come out of the transform looking like square
 *          time pulses will actually turn into oscillatory weirdness!
 *
 * \param[in] F A function, F(s), in the Laplace domain
 * \param[in] t The time at which to evaluate the transform.  Only times
 *              greater than zero are permitted.
 *
 * \return The inverse Laplace transform of F(s) evaluated at time t.
 */
double Laplace::inverse_transform(double (*F)(const double), const double t) const
{
    if(t <= 0.0)
    {
        std::ostringstream oss;
        oss << "Inverse Laplace transform algorithm only works for t > 0. Cannot solve at t = " << t;
        throw std::domain_error(oss.str());
    }

    const double ln2t = M_LN2/t;
    double f_t = 0.0; // Result

    // Sum over Stehfest coefficients
    for (unsigned int i = 0; i < V.size(); i++)
        f_t += V[i] * F(ln2t*(i+1));

    return ln2t * f_t;
}

/**
 * \brief The cotangent of a number
 *
 * \param[in] x The number for which to find the cotangent (radians)
 *
 * \return The cotangent
 */
double cot(const double x)
{
    return 1.0/tan(x);
}

/**
 * \brief The hyperbolic cotangent of a number
 *
 * \param[in] x The number for which to find the hyperbolic cotangent
 *
 * \return The hyperbolic cotangent
 */
double coth(const double x)
{
    return 1.0/tanh(x);
}
} // namespace Leeds
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
