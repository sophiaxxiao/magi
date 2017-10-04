#include <cmath>
#include <random>
#include <vector>
#include <iostream>
#include <stdio.h>
// #include <armadillo>
// [[Rcpp::depends(RcppArmadillo)]]
#include <RcppArmadillo.h>

#include "classDefinition.h"

using namespace std;
using namespace arma;

struct gpcov {
  mat C, Cinv, mphi, Kphi, Kinv, dCdphi1, dCdphi2, CeigenVec, KeigenVec, mphiLeftHalf;
  vec Ceigen1over, Keigen1over;
};

gpcov maternCov( vec, mat, int);
gpcov rbfCov( vec, mat, int);
gpcov compact1Cov( vec, mat, int);
lp phisigllik( vec, mat, mat, string kernel = "matern");
lp xthetallik( const vec & xtheta,
               const gpcov & CovV,
               const gpcov & CovR,
               const double & sigma,
               const mat & yobs,
               const std::function<mat (vec, mat)> & fODE);
lp xthetallik_rescaled( const vec & xtheta,
                        const gpcov & CovV,
                        const gpcov & CovR,
                        const double & sigma,
                        const mat & yobs,
                        const std::function<mat (vec, mat)> & fODE);
mat fnmodelODE(const vec &, const mat &);
