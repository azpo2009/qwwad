/*=================================================================
        ppcd      PseudoPotential Charge Density
  =================================================================

   This program sums the charge densities over a selected number
   of bands, for a user defined cuboid at a specified resolution 
   from the eigenvectors generated by pplb.c.  Written only for the 
   zone center (k=0) at present.

   Input files:
               a_nk.r       expansion coefficients of eigenvectors
                  G.r       reciprocal lattice vectors

   Output files:
                cd.r		charge density grid


   Paul Harrison, March 1995  

   Modifications, July 1998

   Modifications, September 1998                              */

#include <complex.h>
#include <error.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <gsl/gsl_math.h>
#include "struct.h"
#include "maths.h"
#include "qclsim-constants.h"

static complex double * read_ank(const int  N,
                                 int       *Nn);

int main(int argc,char *argv[])
{
extern double atof();
extern int atoi();
vector *read_rlv();	/* function to read reciprocal lattice vectors	*/
complex double *ank;	/* real coefficients of eigenvectors		*/
double complex psi;    	/* the wave function psi_nk(r)			*/
double A0;		/* Lattice constant			       	*/
double Gdotr;		/* G.r						*/
double Omega;           /* normalisation constant                       */
double psi_sqr;         /* charge density                               */
double x_min;           /*             -+                               */
double x_max;           /*              |                               */
double y_min;           /*               \    spatial extent            */
double y_max;           /*               /    of cuboid                 */
double z_min;           /*              |                               */
double z_max;           /*             -+                               */
int	iG;		/* index over G vectors                         */
int	in;		/* index over bands                             */
int	ix;		/* index along x-axis                           */
int	iy;		/* index along y-axis                           */
int	iz;		/* index along z-axis                           */
int	N;		/* number of reciprocal lattice vectors		*/
int	Nn;		/* number of bands in eigenvector file		*/
int	n_min;          /* lowest band in summation			*/
int	n_max;		/* highest band in summation		 	*/
int	n_xyz;          /* number of points per lattice constant        */
FILE	*Fcd;           /* pointer to charge density file, cd.r         */
FILE	*Fcdx;          /* pointer to x coordinates file, cdx.r         */
FILE	*Fcdy;          /* pointer to y coordinates file, cdy.r         */
FILE	*Fcdz;		/* pointer to z coordinates file, cdz.r         */
vector	*G;		/* reciprocal lattice vectors                   */
vector	r;		/* position                                     */

/* default values	*/

A0=5.65e-10;
n_xyz=20;
n_min=0;
n_max=3;
x_min=0;
x_max=0;
y_min=0;
y_max=0;
z_min=0;
z_max=1;

while((argc>1)&&(argv[1][0]=='-'))
{
 switch(argv[1][1])
 {
  case 'A':
           A0=atof(argv[2])*1e-10;
           break;
  case 'N':
	   n_xyz=atoi(argv[2]);
	   break;
  case 'n':
           n_min=atoi(argv[2])-1;         /* Note -1=>top VB=4, CB=5 */
           break;
  case 'm':
           n_max=atoi(argv[2])-1;         /* Note -1=>top VB=4, CB=5 */
           break;
  case 'x':
	   x_min=atof(argv[2]);
	   break;
  case 'X':
	   x_max=atof(argv[2]);
	   break;
  case 'y':
	   y_min=atof(argv[2]);
	   break;
  case 'Y':
	   y_max=atof(argv[2]);
	   break;
  case 'z':
	   z_min=atof(argv[2]);
	   break;
  case 'Z':
	   z_max=atof(argv[2]);
	   break;
  default :
	   printf("Usage: ppcd [-x # (\033[1m0\033[0mA0)][-X # (\033[1m0\033[0mA0)]     minimum and maximum\n");
	   printf("            [-y # (\033[1m0\033[0mA0)][-Y # (\033[1m0\033[0mA0)]     extent of charge\n");
	   printf("            [-z # (\033[1m0\033[0mA0)][-Z # (\033[1m1\033[0mA0)]     density cuboid\n");
	   printf("            [-N # points per A0 \033[1m20\033[0m]\n");
	   printf("            [-n # lowest band \033[1m1\033[0m][-m highest band \033[1m4\033[0m], lowest band in ank file=1\n");
	   printf("            [-A Lattice constant (\033[1m5.65\033[0mAngstrom)]\n");
	   exit(0);
 }
 argv++;
 argv++;
 argc--;
 argc--;
}

Omega=1.0;

G=read_rlv(A0,&N);
ank=read_ank(N,&Nn);

/* Check number of eigenvectors in file is greater than or equal to number
   required for summation						*/

if((n_max-n_min+1)>Nn)
{fprintf(stderr,"Error: Incorrect number of states in `ank.r'!\n");exit(0);}

/* Open file for charge density */

Fcd=fopen("cd.r","w");

for(ix=0;ix<=(x_max-x_min)*n_xyz;ix++)		/* index along x-axis */
{
 r.x=(x_min+(float)ix/(float)n_xyz)*A0;
 for(iy=0;iy<=(y_max-y_min)*n_xyz;iy++)		/* index along y-axis */
 {
  r.y=(y_min+(float)iy/(float)n_xyz)*A0;
  for(iz=0;iz<=(z_max-z_min)*n_xyz;iz++)	/* index along z-axis */
  {
   r.z=(z_min+(float)iz/(float)n_xyz)*A0;
   psi_sqr=0;
   for(in=n_min;in<=n_max;in++)			/* sum over bands */
   {
    /* Calculate psi_nk(r)	*/
    psi=0;
    for(iG=0;iG<N;iG++)				/* sum over G */
    {
     Gdotr=vsprod(*(G+iG),r);
     psi += ank[iG*Nn+in] * cexp(Gdotr);
    }
    psi_sqr+=gsl_pow_2(cabs(psi))/Omega;
   }
   fprintf(Fcd,"%le\n",psi_sqr);
  }
 }
}

fclose(Fcd);	/* Close charge density file	*/

/* now regenerate positions r for writing to file */

/* Open files for spatial co-ordinates */

Fcdx=fopen("cd-x.r","w");Fcdy=fopen("cd-y.r","w");Fcdz=fopen("cd-z.r","w");

for(ix=0;ix<=(x_max-x_min)*n_xyz;ix++)      /* index along x-axis */
{
 r.x=(x_min+(float)ix/(float)n_xyz);        /* this time in units of A0 */
 fprintf(Fcdx,"%6.3f\n",r.x);
}
for(iy=0;iy<=(y_max-y_min)*n_xyz;iy++)      /* index along x-axis */
{
 r.y=(y_min+(float)iy/(float)n_xyz);        /* this time in units of A0 */
 fprintf(Fcdy,"%6.3f\n",r.y);
}
for(iz=0;iz<=(z_max-z_min)*n_xyz;iz++)      /* index along x-axis */
{
 r.z=(z_min+(float)iz/(float)n_xyz);        /* this time in units of A0 */
 fprintf(Fcdz,"%6.3f\n",r.z);
}

fclose(Fcdx);fclose(Fcdy);fclose(Fcdz);

return EXIT_SUCCESS;
}/* end main */


/******************************************************************************/



/**
 * Reads the eigenvectors (a_nk(G)) from the file a_nk.r
 * created by the code pplb.c into the array a_nk[N][Nn]
 *
 * \param[in] N  The number of terms in each eigenvector
 * \param     Nn The number of bands in file
 */
static complex double * read_ank(const int  N,
                                 int       *Nn)
{
 int	in;		/* index across bands				*/
 int	iG;		/* index across G vectors			*/
 int	n;		/* counter for number of elements in file	*/
 complex double *ank;
 FILE   *Fank;		/* file pointer to eigenvectors file		*/

if((Fank=fopen("ank.r","r"))==0)
{
 fprintf(stderr,"Error: Cannot open input file 'ank.r'!\n");
 exit(0);
}

/* Deduce number of complexes in file and hence number of bands	*/

n=0;
while(fscanf(Fank,"%*f %*f")!=EOF)
 n++;
rewind(Fank);

/* The number of bands Nn is therefore the total number of elements divided
   by the number of terms in each eigenvector				*/

*Nn=n/N;

/* Allocate memory for eigenvectors	*/

ank=(complex double *)calloc(n,sizeof(complex double));
if(ank==0){fprintf(stderr,"Cannot allocate memory!\n");exit(0);}

/* Finally read eigenvectors into structure	*/

for(iG=0;iG<N;iG++)
 for(in=0;in<*Nn;in++)
 {
  double re, im;
  int n_read = fscanf(Fank,"%lf %lf",&re,&im);
  if(n_read != 2)
    error(EXIT_FAILURE, 0, "Data missing in ank.r");
  ank[iG*(*Nn)+in] = re + I*im;
 }

fclose(Fank);

return(ank);
}



vector
*read_rlv(A0,N)

/* This function reads the reciprocal lattice vectors (defined in
   the file G.r) into the array G[] and then converts into SI units */

double	A0;
int     *N;
{
 int    i=0;
 vector *G;
 FILE   *FG;           /* file pointer to wavefunction file */

 if((FG=fopen("G.r","r"))==0)
 {
  fprintf(stderr,"Error: Cannot open input file 'G.r'!\n");
  exit(0);
 }

 *N=0;
 while(fscanf(FG,"%*f %*f %*f")!=EOF)
  (*N)++;
 rewind(FG);

 G=(vector *)calloc(*N,sizeof(vector));
 if (G==0)  {
  fprintf(stderr,"Cannot allocate memory!\n");
  exit(0);
 }

 while(fscanf(FG,"%lf %lf %lf",&(G+i)->x,&(G+i)->y,&(G+i)->z)!=EOF)
  {
   (G+i)->x*=(2*pi/A0);(G+i)->y*=(2*pi/A0);(G+i)->z*=(2*pi/A0);
   i++;
  }

 fclose(FG);

 return(G);
}

