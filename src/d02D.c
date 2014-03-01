/*==================================================================
                                d02D
  ==================================================================*/

/* This program implements a variational technique to calculate the
   uncorrelated one particle energies of an electron attatched to a 
   single donor at any position, in any user supplied potential.  
   The potential is read from the file v.r

   This version is a single variational parameter calculation---the 
   two-dimensional (2D) approximation trial wavefunction:

		Psi=chi(z) exp(-r"/lambda)

   where	r"=sqrt(x^2+y^2)

		Input files:
		r_d.r		donor (or acceptor positions)
		v.r		one-dimensional potential

		Output files:
		e.r		total energies for each r_d
		l.r		Bohr radii (lambda) for each r_d
		wfn.r		wave functions, both Psi and chi, n=0,1,2..

   Paul Harrison, February 1993 

   Substantially revised,

   Paul Harrison, February 1998					*/

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <signal.h>
#include <malloc.h>
#include <gsl/gsl_math.h>
#include "qclsim-constants.h"
#include "d0-helpers.h"
#include "struct.h"
#include "maths.h"

static double I_1(const double  lambda);
static double I_2(const double  lambda);
static double I_3(const double  lambda);
static double I_4(const double  lambda,
                  const double  z_dash,
                  const size_t  N_w);

int main(int argc,char *argv[])
{
double psi_at_inf();
void   wavefunctions();
bool    repeat_lambda();    

double d_E;                 /* infinitesmal energy               */
double delta_E;             /* small but finite energy           */
double delta_z;             /* z separation of input potentials  */
double dy;                  /* derivative of function            */
double E;                   /* electron (or hole) energies       */
double epsilon;             /* permitivity of material           */
double lambda;              /* Bohr radius (variational)         */
double lambda_0;            /* Bohr radius of electron (or hole) */
double lambda_start;        /* initial Bohr radius               */
double lambda_step;         /* Bohr radius increment             */
double lambda_stop;         /* final lambda                      */
double mstar;               /* electron mass  	                 */
double r_d;                 /* donor (or acceptor) position      */
double x;                   /* independent variable (energy)     */
double x_min;               /* variational calculation minimum x */
double y;                   /* function (psi at infinity)        */
double y1;                  /* temporary y value                 */
double y2;                  /* temporary y value                 */
int    i_d;                 /* donor (or acceptor) index         */
int    N_w;                 /* number of strips in w integration */
size_t n;		    /* number of lines of potential file */
bool   repeat_flag_lambda; /* variational flag=>new lambda      */
data11  *Vstart;             /* start address of potential        */
FILE   *fe;                 /* file pointer for energies         */
FILE   *fl;                 /* file pointer for lambda_0         */
FILE   *fr_d;               /* file pointer to donor positions   */
 

/* default values */

delta_E=1e-3*e;
epsilon=13.18*eps0;
lambda_start=50.0e-10;
lambda_step=1.0e-10;
lambda_stop=-1.0e-10;
mstar=0.067*me;

/* computational default values */

i_d=0;          
d_E=delta_E/1e+6;
N_w=100;

while((argc>1)&&(argv[1][0]=='-'))
{
 switch(argv[1][1])
 {
  case 'd':
	   delta_E=atof(argv[2])*1e-3*e;
	   break;
  case 'e':
	   epsilon=atof(argv[2])*eps0;
	   break;
  case 'm':
	   mstar=atof(argv[2])*me;
	   break;
  case 's':
	   lambda_start=atof(argv[2])*1e-10;
	   break;
  case 't':
	   lambda_step=atof(argv[2])*1e-10;
	   break;
  case 'u':
	   lambda_stop=atof(argv[2])*1e-10;
	   break;
  default :
           printf("Usage:  d02D [-d energy step (\033[1m1\033[0mmeV)][-e relative permittivity \033[1m13.18\033[0m]\n");
	   printf("             [-m mass (\033[1m0.067\033[0mm0)]\n");
	   printf("             [-s starting lambda (\033[1m50\033[0mA)][-t lambda increment (\033[1m1\033[0mA)]\n");
	   printf("             [-u final lambda (\033[1m-1\033[0mA)]\n");
	   exit(0);
 }
 argv++;
 argv++;
 argc--;
 argc--;
}

  Vstart = read_v(&n);                  /* reads potential file */

  delta_z=read_delta_z(Vstart);

  /* Open files for output of data */

  fe=fopen("e.r","w");           /* E versus r_d	*/
  fl=fopen("l.r","w");           /* lambda versus r_d	*/

  /* Different donor (or acceptor) positions */

  if((fr_d=fopen("r_d.r","r"))==0)    /* open r_d data for reading	*/
  {
   fprintf(stderr,"Error: Cannot open input file 'r_d.r'!\n");
   exit(0);
  }

  /* read in each donor position from r_d.r and perform variational
     calculation for each one						*/

  while(fscanf(fr_d,"%lf\n",&r_d)!=EOF)
  {
   lambda=lambda_start;    /* initial lambda value               */
   x_min=e;              /* minimum energy of single donor 1eV */

   /* Variational calculation */

    do
    {

    /* initial energy estimate=minimum potential-binding energy
                               of particle to free ionised dopant */

    x=V_min(Vstart,n)-gsl_pow_2(e)/(4*pi*epsilon*lambda);   

    /* increment energy-search for f(x)=0 */

    y2=psi_at_inf(x,delta_z,epsilon,lambda,mstar,r_d,Vstart,n,N_w);

    do
    {
     y1=y2;
     x+=delta_E;
     y2=psi_at_inf(x,delta_z,epsilon,lambda,mstar,r_d,Vstart,n,N_w);
    }while(y1*y2>0);

   /* improve estimate using midpoint rule */

    x-=fabs(y2)/(fabs(y1)+fabs(y2))*delta_E;

   /* implement Newton-Raphson method */

    do
    {
     y=psi_at_inf(x,delta_z,epsilon,lambda,mstar,r_d,Vstart,n,N_w);
     dy=(psi_at_inf(x+d_E,delta_z,epsilon,lambda,mstar,r_d,Vstart,n,N_w)-
         psi_at_inf(x-d_E,delta_z,epsilon,lambda,mstar,r_d,Vstart,n,N_w))/
        (2.0*d_E);
     x-=y/dy;

    }while(fabs(y/dy)>1e-9*e);

    printf("r_d %le lambda %le energy %le meV\n",r_d,lambda,x/(1e-3*e));       

    repeat_flag_lambda=repeat_lambda(&lambda,&lambda_0,x,&x_min);

    lambda+=lambda_step;     /* increments Bohr radius */

   }while((repeat_flag_lambda&&(lambda_stop<0))||(lambda<lambda_stop));

   E=x_min;   /* assign the energy E to the minimum x_min */

   /* Output neutral dopant binding energies (E) and 
      Bohr radii (lambda) in meV and Angstrom respectively */

   fprintf(fe,"%le %le\n",r_d/1e-10,E/(1e-3*e));
   fprintf(fl,"%le %le\n",r_d/1e-10,lambda_0/1e-10);

   wavefunctions(delta_z,E,epsilon,lambda_0,mstar,r_d,i_d,Vstart,n,N_w);

   i_d++;            /* donor index */

  }/* end while r_d */

  fclose(fr_d);
  fclose(fe);
  fclose(fl);
  free(Vstart);

  return EXIT_SUCCESS;
} /* end main */

bool
repeat_lambda(lambda,lambda_0,x,x_min)

/* This function compares minimum value of energy for this lambda,
   if it is a new true minima, then repeat for new lambda	*/

double *lambda;
double *lambda_0;
double x;
double *x_min;
{
 bool flag;

 if(x<*x_min)    
 {      
  *x_min=x;                           /* set new minimum        */
  *lambda_0=*lambda;
  flag=true;                          /* repeat for new lambda  */
 }
 else 
 {
  flag=false;
 }    
 return(flag);
}



double
psi_at_inf(E,delta_z,epsilon,lambda,mstar,r_d,Vp,n,N_w)     

/* This function returns the value of the wavefunction (psi)
   at +infinity for a given value of the energy.  The solution
   to the energy occurs for psi(+infinity)=0.                      */

double E;
double delta_z;
double epsilon;
double lambda;
double mstar;
double r_d;
int n;
data11 *Vp;
size_t N_w;
{
 double alpha;		     /* coefficient of second derivative, see notes */
 double beta;                /* coefficient of first derivative            */
 double gamma;               /* coefficient of function                    */
 double delta_psi;           /* initial wavefunction value                 */
 double I1,I2,I3,I4;         /* particular values of the functions         */
 double kappa;     
 double psi[3];              /* wavefunction at z-delta_z, z, z+delta_z    */
 int i;			     /* index			                   */


 /* ignore potential V corresponding to psi[0] and psi[1] */

 Vp++;

 /* boundary conditions */

 kappa=sqrt(2*mstar/hBar*(Vp->b-E)/hBar);

 delta_psi=1.0e-10;

 psi[0]=delta_psi;                 /* arbitrary number close to zero */
 psi[1]=psi[0]*exp(kappa*delta_z); /* exponential growth produce by  */

 for(i=1;i<n;i++)
 {
  I1=I_1(lambda);
  I2=I_2(lambda);
  I3=I_3(lambda);
  I4=I_4(lambda,(Vp->a)-r_d,N_w);

  alpha=I1;
  beta=2*I2;
  gamma=I3+(2*mstar*gsl_pow_2(e/hBar)/(4*pi*epsilon))*I4
          -(2*mstar/hBar)*((Vp->b)-E)*I1/hBar;

  psi[2]=((-1+beta*delta_z/(2*alpha))*psi[0]
          +(2-gsl_pow_2(delta_z)*gamma/alpha)*psi[1]
         )/(1+beta*delta_z/(2*alpha));

  psi[0]=psi[1];
  psi[1]=psi[2];
  Vp++;
 } /* end for */

 return(psi[0]-delta_psi);
}

void
wavefunctions(delta_z,E,epsilon,lambda,mstar,r_d,i_d,Vp,n,N_w)

/* This function calculates and writes the wavefunctions
   both psi(z) and chi(z) to the external file wf(n).r.		*/

double delta_z;
double E;
double epsilon;
double lambda;
double mstar;
double r_d;
int    i_d;
size_t n;
data11  *Vp;
size_t  N_w;
{
 double alpha;		 /* coefficient of second derivative, see notes */
 double beta;            /* coefficient of first derivative             */
 double gamma;           /* coefficient of function                     */
 double delta_psi;       /* initial wavefunction value                  */
 double I1,I2,I3,I4;     /* particular values of the functions	        */
 double kappa;
 double Npsi=0;          /* normalisation integral for psi              */
 double Nchi=0;          /* normalisation integral for chi              */
 double psi[3];          /* wavefunctions at z-d_z,z,z+d_z              */
 int    i;               /* index                                       */
 char   filename[9];     /* character string for wavefunction filename  */
 FILE   *fw;             /* file wf.r                                   */
 data12  *wf_start;      /* pointer to start of w.f                     */
 data12  *wf;		 /* wavefunction pointer, note b[0]=psi, 
                            b[1]=chi, see notes                         */

 wf_start=(data12 *)calloc(n,sizeof(data12)); /* allocates memory for wavefunctions */
 if (wf_start==0)  {
  fprintf(stderr,"Cannot allocate memory!\n");
  exit(0);
 }
 wf=wf_start;

 /* boundary conditions */

 kappa=sqrt(2*mstar/hBar*((Vp->b)-E)/hBar);

 delta_psi=1.0e-10;

 psi[0]=delta_psi;                 /* arbitrary number close to zero */
 psi[1]=psi[0]*exp(kappa*delta_z); /* exponential growth produce by  */

 /* write first value of wavefunction */ 

 wf->a=Vp->a;
 wf->b[0]=psi[0]; 
 wf->b[1]=psi[0]; 

 wf++;
 Vp++;
 wf->a=Vp->a;
 wf->b[0]=psi[1]; 
 wf->b[1]=psi[1]; 
 
 /* calculate unnormalised wavefunction */

 for(i=2;i<n;i++) /* points 0 and 1 defined	*/
 {
  I1=I_1(lambda);
  I2=I_2(lambda);
  I3=I_3(lambda);
  I4=I_4(lambda,(Vp->a)-r_d,N_w);

  alpha=I1;
  beta=2*I2;
  gamma=I3+(2*mstar*gsl_pow_2(e/hBar)/(4*pi*epsilon))*I4
          -(2*mstar/hBar)*((Vp->b)-E)*I1/hBar;

  psi[2]=((-1+beta*delta_z/(2*alpha))*psi[0]
          +(2-gsl_pow_2(delta_z)*gamma/alpha)*psi[1]
         )/(1+beta*delta_z/(2*alpha));


  /* write wavefunction point corresponding to current z and V */

  wf++;		/* increment pointer ready for writing */
  Vp++;		/* increment pointer for next loop, note increment first
                   as wf derives z from Vp                       */

  wf->a=Vp->a;
  wf->b[0]=psi[1];
  wf->b[1]=psi[1];


  psi[0]=psi[1];       
  psi[1]=psi[2];

 }

 /* calculate normalisation integral	*/

 wf=wf_start;
 for(i=0;i<n;i++)
 {
  Npsi+=gsl_pow_2(wf->b[0])*delta_z;
  Nchi+=gsl_pow_2(wf->b[1])*delta_z;
  wf++;
 }

 /* divide unnormalised wavefunction by square root
    of normalisation integral                       */
 
 wf=wf_start;
 for(i=0;i<n;i++)
 {
  wf->b[0]=wf->b[0]/sqrt(Npsi);
  wf->b[1]=wf->b[1]/sqrt(Nchi);
  wf++;
 }

 /* generate output filename (and open file for writing) using 
    the basis wf%i.r where the integer %i is the donor index i_d  */

 sprintf(filename,"wf%i.r",i_d);
 fw=fopen(filename,"w");

 /* write wavefunction wf(n).r	*/

 wf=wf_start;

 for(i=0;i<n;i++)
 {
  fprintf(fw,"%20.17le %le %le\n",wf->a,wf->b[0],wf->b[1]);
  wf++;
 }
 fclose(fw);
}

/**
 * Computes the binding energy integral \f$I_1\f$ for a 2D trial wavefunction
 *
 * \param[in] lambda variational parameter [m]
 *
 * \returns Binding energy integral [m^2]
 *
 * \details See Eq. 5.39, QWWAD3. The integral is solved analytically as
 *          \f[
 *            I_1 = 2\pi\frac{\lambda^2}{4}
 *          \f]
 */
static double I_1(const double lambda)
{
 return 2*pi*gsl_pow_2(lambda)/4;
}

/**
 * Computes the binding energy integral \f$I_2\f$ for a 2D trial wavefunction
 *
 * \param[in] lambda variational parameter [m]
 *
 * \returns Binding energy integral [m]
 *
 * \details See Eq. 5.42, QWWAD3. The integral evaluates to zero
 */
static double I_2(const double lambda)
{
 (void)lambda; /* Silence compiler warning about unused param */
 return 0;
}

/**
 * Computes the binding energy integral \f$I_3\f$ for a 2D trial wavefunction
 *
 * \param[in] lambda variational parameter [m]
 *
 * \returns Binding energy integral [dimensionless]
 *
 * \details See Eq. 5.52, QWWAD3. The integral is solved analytically as
 *          \f[
 *            I_3 = 2\pi \left(-\frac{1}{4}\right)
 *          \f]
 */
static double I_3(const double lambda)
{
 (void)lambda; /* Silence compiler warning about unused param */
 return 2*pi*(-0.25);
}

/**
 * Computes the binding energy integral \f$I_4\f$ for a 2D trial wavefunction
 *
 * \param[in] lambda variational parameter [m]
 * \param[in] z_dash displacement between electron and donor in z-direction [m]
 * \param[in] N_w    number of samples to take in integration
 *
 * \returns Binding energy integral [m]
 *
 * \details See Eq. 5.54, QWWAD3. The integral is given by
 *          \f[
 *            I_4 = 2\pi \int_{|z'|}^{\infty}\frac{\exp{-\frac{2\sqrt{r'^2-z'^2}}{\lambda}}}{r'}r'\mathrm{d}r'
 *          \f]
 */
static double I_4(const double lambda,
                  const double z_dash,
                  const size_t N_w)
{
 double I_40=0.0; /* Integral term */
 double w;
 double delta_w=(1.0-0.0)/(float)N_w;

 for (w=delta_w/2;w<1;w+=delta_w)
 {
  I_40+=exp(-fabs(z_dash)*(1/w-w)/lambda)*fabs(z_dash)
        *(1-gsl_pow_2(w))/(2*gsl_pow_2(w))*delta_w;
 }

 return 2*pi*I_40;
}
