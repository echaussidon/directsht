#include	<stdlib.h>
#include	<math.h>
#include	"omp.h"



int	indx(int ell, int m, int Nl) {
int	ii;
  ii = (m*(2*Nl-1-m))/2 + ell;
  return(ii);
}




int	make_table(int Nl, int Nx, double xmax, double Yv[], double Yd[]) {
/* Makes the function and derivative tables. */
int	ell,m,ix,ii,i0,i1,i2;
double	xx,sx,omx2,dx,fact1,fact2;
  dx = xmax/(Nx-1.0);
  /* First do Legendre polynomials. */
#pragma omp parallel for private(ix), shared(dx,Yv)
  for (ix=0; ix<Nx; ix++) {
    Yv[Nx*0+ix] = 1.0;
    Yv[Nx*1+ix] = ix*dx;
  }
  for (ell=2; ell<Nl; ell++) {
    i0 = Nx*indx(ell-0,0,Nl);
    i1 = Nx*indx(ell-1,0,Nl);
    i2 = Nx*indx(ell-2,0,Nl);
#pragma omp parallel for private(ix,xx), shared(dx,Yv,i0,i1,i2,ell), schedule(static)
    for (ix=0; ix<Nx; ix++) {
      xx        = ix * dx;
      Yv[i0+ix] = (2-1./ell)*xx*Yv[i1+ix] - (1.0-1./ell)*Yv[i2+ix];
    }
  }
  /* Now fill in m=ell function values. */
  for (m=1; m<Nl; m++) {
    i0 = Nx*indx(m-0,m-0,Nl);
    i1 = Nx*indx(m-1,m-1,Nl);
#pragma omp parallel for private(ix,xx), shared(dx,Yv,i0,i1), schedule(static)
    for (ix=0; ix<Nx; ix++) {
      xx        = ix * dx;
      Yv[i0+ix] = -sqrt( (1.0-0.5/m)*(1.0-xx*xx) )*Yv[i1+ix];
    }
  }
  /* and the m=ell-1 function values. */
  for (m=1; m<Nl-1; m++) {
    i0 = Nx*indx(m+0,m,Nl);
    i1 = Nx*indx(m+1,m,Nl);
#pragma omp parallel for private(ix,xx), shared(dx,Yv,i0,i1), schedule(static)
    for (ix=0; ix<Nx; ix++) {
      xx        = ix * dx;
      Yv[i1+ix] = sqrt(2*m+1.)*xx*Yv[i0+ix];
    }
  }
  /* Finally fill in the other m values. */
  for (m=0; m<Nl-1; m++)
    for (ell=m+2; ell<Nl; ell++) {
      i0    = Nx*indx(ell-0,m,Nl);
      i1    = Nx*indx(ell-1,m,Nl);
      i2    = Nx*indx(ell-2,m,Nl);
      fact1 = sqrt( (double)(ell-m)/(double)(ell+m) );
      fact2 = sqrt( (ell-m-1.)/(ell+m-1.) );
#pragma omp parallel for private(ix,xx), shared(dx,Yv,i0,i1,i2,fact1,fact2), schedule(static)
      for (ix=0; ix<Nx; ix++) {
        xx        = ix * dx;
        Yv[i0+ix] = (2*ell-1)*xx*Yv[i1+ix] - (ell+m-1)*Yv[i2+ix]*fact2;
        Yv[i0+ix]*= fact1/(ell-m);
      }
    }
  /* Then we do the derivatives -- again do m=0 separately. */
  for (ix=0; ix<Nx; ix++) {
    Yd[Nx*0+ix] = 0.0;
    Yd[Nx*1+ix] = 1.0;
  }
  for (ell=2; ell<Nl; ell++) {
    i0 = Nx*indx(ell-0,0,Nl);
    i1 = Nx*indx(ell-1,0,Nl);
#pragma omp parallel for private(ix,xx,omx2), shared(dx,Yv,Yd,i0,i1), schedule(static)
    for (ix=0; ix<Nx; ix++) {
      xx        = ix * dx;
      omx2      = 1.0-xx*xx;
      Yd[i0+ix] = ell*(Yv[i1+ix]-xx*Yv[i0+ix])/omx2;
    }
  }
  /* Then the higher m's. */
  for (ell=0; ell<Nl; ell++)
    for (m=1; m<=ell; m++) {
      i0    = Nx*indx(ell-0,m,Nl);
      i1    = Nx*indx(ell-1,m,Nl);
      fact1 = sqrt( (double)(ell-m)/(double)(ell+m) );
#pragma omp parallel for private(ix,xx,omx2), shared(dx,Yv,Yd,i0,i1,fact1), schedule(static)
      for (ix=0; ix<Nx; ix++) {
        xx        = ix * dx;
        omx2      = 1.0-xx*xx;
        Yd[i0+ix] = ((ell+m)*fact1*Yv[i1+ix]-ell*xx*Yv[i0+ix])/omx2;
      }
    }
  /* Normalize by Sqrt[ (2ell+1)/4Pi ] */
  for (ell=0; ell<Nl; ell++) {
    fact1 = sqrt( (2*ell+1.0)/4./M_PI );
    for (m=0; m<=ell; m++) {
      ii = Nx*indx(ell,m,Nl);
#pragma omp parallel for private(ix), shared(fact1,ii,Yv,Yd), schedule(static)
      for (ix=0; ix<Nx; ix++) {
        Yv[ii+ix] *= fact1;
        Yd[ii+ix] *= fact1;
      }
    }
  }
  return(0);
}



int	do_transform(int Nl, int Nx, double xmax, double Yv[], double Yd[],
                     int Np, double theta[], double phi[], double wt[],
                     double carr[], double sarr[]) {
int	ell,m,ii,ix,offset,i0,i1;
double	xx,ax,dx,hh,sc,ss,yv;
double	tt,t1,t2,s0,s1,s2,s3;
  dx = xmax/(Nx-1.0);
  for (ell=0; ell<Nl; ell++)
    for (m=0; m<=ell; m++) {
      offset  = Nx*indx(ell,m,Nl);
      ss = sc = 0.0; /* Assumulate the sine and cosine sums here.*/
#pragma omp parallel for private(ii,ix,i0,i1,xx,ax,tt,t1,t2,s0,s1,s2,s3,yv), shared(ell,m,dx,offset,Yv,Yd,theta,phi,wt), reduction(+:sc,ss), schedule(static)
      for (ii=0; ii<Np; ii++) {
        /* Use Hermite spline to get Ylm(x,0). */
        xx = cos(theta[ii]);
        ax = fabs(xx);
        ix = ax/dx;
        tt = ax/dx-ix;
        t1 = (tt-1.0)*(tt-1.0);
        t2 = tt*tt;
        s0 = (1+2*tt)*t1;
        s1 = tt*t1;
        s2 = t2*(3-2*tt);
        s3 = t2*(tt-1.0);
        i0 = offset+ix;
        i1 = i0+1;
        yv = Yv[i0]*s0+Yd[i0]*s1*dx+Yv[i1]*s2+Yd[i1]*s3*dx;
        /* Flip sign if x<0 and ell-m is odd. */
        if (xx<0 && (ell-m)%2==1) yv = -yv;
        /* Multiply through by sin(m.phi) and cos(m.phi) */
        sc+= wt[ii] * yv * cos(m*phi[ii]);
        ss+= wt[ii] * yv * sin(m*phi[ii]);
      }
      carr[indx(ell,m,Nl)] = sc;
      sarr[indx(ell,m,Nl)] = ss;
    }
  return(0);
}