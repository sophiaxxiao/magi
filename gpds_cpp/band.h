#ifndef BAND_H
#define BAND_H

#include <stdio.h>
#include <iostream>
#include <fstream>
#include <math.h>
#include <numeric>
#include <armadillo>

using namespace std;



extern "C" {

  void xthetallikBandC( const double *xtheta, const double *Vmphi, const double *VKinv, const double *VCinv,
                        const double *Rmphi, const double *RKinv, const double *RCinv, const int *bandsize, const int *nn,
                        const double *sigma, const double *yobs, double *ret, double *retgrad,
                        const std::function<arma::mat (arma::vec, arma::mat)> & fODE);
  // previous .C wrapper doesn't work with Rcpp auto-generated wrapper
  void bmatvecmult(const double *a, const double *b, const int *bandsize, const int *matdim, double *result);
  void bmatvecmultT(const double *a, const double *b, const int *bandsize, const int *matdim, double *result);
}

// g++ band.cpp -o band.o -lopenblas -llapack -lm -Wall -L/opt/OpenBLAS/lib -I/opt/OpenBLAS/include
#endif
//g++ -fPIC -c band.cpp -o band.o -lopenblas -llapack -lm -Wall -L/usr/local/opt/openblas/lib -I/usr/local/opt/openblas/include -larmadillo -I../include -L../lib
