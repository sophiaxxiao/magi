#include "hmc.h"
#include "band.h"
#include "paralleltempering.h"
#include "tgtdistr.h"
#include "dynamicalSystemModels.h"

using namespace arma;

// [[Rcpp::export]]
int hmcTest(){
  arma::vec initial = arma::zeros<arma::vec>(4);
  arma::vec step(4);
  step.fill(0.05);
  int nsteps = 20;
  bool traj = true;
  hmcstate post = basic_hmcC(lpnormal, initial, step, 
                             {-arma::datum::inf}, 
                             {arma::datum::inf}, 
                             nsteps, traj);
  // for(int i; i < post.final.size(); i++)
    //   cout << post.final(i) << endl;
  // cout << post.final << endl;
  return 0;
}

// [[Rcpp::export]]
double bandTest(std::string filename="data_band.txt"){
  const int datasize = 201, bandsize = 20;
  
  double xtheta[datasize * 2 + 3];
  double Vmphi[datasize * (bandsize * 2 + 1)];
  double VKinv[datasize * (bandsize * 2 + 1)];
  double VCinv[datasize * (bandsize * 2 + 1)];
  double Rmphi[datasize * (bandsize * 2 + 1)];
  double RKinv[datasize * (bandsize * 2 + 1)];
  double RCinv[datasize * (bandsize * 2 + 1)];
  int bandsizeInput, datasizeInput;
  double cursigma;
  double fnsim[datasize * 2];
  
  ifstream fin(filename);
  for(int i = 0; i < datasize * 2 + 3; i++){
    fin >> xtheta[i];
  }
  for(int i = 0; i < datasize * (bandsize * 2 + 1); i++){
    fin >> Vmphi[i];
  }
  for(int i = 0; i < datasize * (bandsize * 2 + 1); i++){
    fin >> VKinv[i];
  }
  for(int i = 0; i < datasize * (bandsize * 2 + 1); i++){
    fin >> VCinv[i];
  }
  for(int i = 0; i < datasize * (bandsize * 2 + 1); i++){
    fin >> Rmphi[i];
  }
  for(int i = 0; i < datasize * (bandsize * 2 + 1); i++){
    fin >> RKinv[i];
  }
  for(int i = 0; i < datasize * (bandsize * 2 + 1); i++){
    fin >> RCinv[i];
  }
  fin >> bandsizeInput;
  fin >> datasizeInput;
  if(bandsizeInput != bandsize || datasizeInput != datasize){
    throw "size not matched";
  }
  fin >> cursigma;
  for(int i = 0; i < datasize * 2; i++){
    fin >> fnsim[i];
    if(fnsim[i] < -99998){
      fnsim[i] = arma::datum::nan;
    }
  }
  
  double ret=0;
  double grad[datasize * 2 + 3];
  
  xthetallikBandC(xtheta, Vmphi, VKinv, VCinv,
                  Rmphi, RKinv, RCinv, &bandsizeInput, &datasizeInput,
                  &cursigma, fnsim, &ret, grad, fnmodelODE);
  return std::accumulate(grad, grad + datasize * 2 + 3, ret);
};



// [[Rcpp::export]]
arma::cube paralleltemperingTest1() {
  std::ofstream out("testout.txt");
  std::streambuf *coutbuf = std::cout.rdbuf(); //save old buf
  std::cout.rdbuf(out.rdbuf()); //redirect std::cout to out.txt!
  
  function<lp(vec)> lpnormal = [](vec x) {return lp(-arma::sum(arma::square(x))/2.0);};
  vec temperature = arma::linspace<vec>(8, 1, 8);
  std::function<mcmcstate(function<lp(vec)>, mcmcstate)> metropolis_tuned =
    std::bind(metropolis, std::placeholders::_1, std::placeholders::_2, 1.0);
  
  cube samples = parallel_termperingC(lpnormal, 
                                      metropolis_tuned, 
                                      temperature, 
                                      arma::zeros<vec>(4), 
                                      0.05, 
                                      1e4,
                                      true);
  std::cout.rdbuf(coutbuf); //reset to standard output again
  return samples;
}

// [[Rcpp::export]]
arma::cube paralleltemperingTest2() {
  function<lp(vec)> lpnormalvalue = [](vec x) {
    return lp(log(exp(-arma::sum(arma::square(x+4))/2.0) + exp(-arma::sum(arma::square(x-4))/2.0)));
  };
  vec temperature = {1, 1.3, 1.8, 2.5, 3.8, 5.7, 8};
  function<mcmcstate(function<lp(vec)>, mcmcstate)> metropolis_tuned =
    std::bind(metropolis, std::placeholders::_1, std::placeholders::_2, 1.0);
  
  cube samples = parallel_termperingC(lpnormalvalue, 
                                      metropolis_tuned, 
                                      temperature, 
                                      arma::zeros<vec>(4), 
                                      0.125, 
                                      1e4,
                                      false);
  // cout << "parallel_termperingC finished, before returning from paralleltemperingTest2\n";
  return samples;
}

//' log likelihood for latent states and ODE theta conditional on phi sigma
//'   latent states have a mean mu
//' @param phisig      the parameter phi and sigma
//' @param yobs        observed data
lp xthetallik_withmu( const vec & xtheta, 
                      const std::vector<gpcov> & CovAllDimensions, 
                      const double & sigma, 
                      const mat & yobs, 
                      const OdeSystem & fOdeModel) {
  const gpcov & CovV = CovAllDimensions[0];
  const gpcov & CovR = CovAllDimensions[1];
  
  int n = (xtheta.size() - 3)/2;
  const vec & theta = xtheta.subvec(xtheta.size() - 3, xtheta.size() - 1);
  lp ret;
  
  const vec & Vsm = xtheta.subvec(0, n - 1);
  const vec & Rsm = xtheta.subvec(n, 2*n - 1);
  const vec & Vsmminusmu = Vsm - CovV.mu;
  const vec & Rsmminusmu = Rsm - CovR.mu;
  
  
  const mat & fderiv = fOdeModel.fOde(theta, join_horiz(Vsm, Rsm));
  const cube & fderivDx = fOdeModel.fOdeDx(theta, join_horiz(Vsm, Rsm));
  const cube & fderivDtheta = fOdeModel.fOdeDtheta(theta, join_horiz(Vsm, Rsm));
  
  mat res(2,3);
  
  // V 
  vec frVminusdotmu = (fderiv.col(0) - CovV.dotmu - CovV.mphi * Vsmminusmu);
  vec fitLevelErrorV = Vsm - yobs.col(0);
  fitLevelErrorV(find_nonfinite(fitLevelErrorV)).fill(0.0);
  res(0,0) = -0.5 * sum(square( fitLevelErrorV )) / pow(sigma,2);
  res(0,1) = -0.5 * as_scalar( frVminusdotmu.t() * CovV.Kinv * frVminusdotmu);
  res(0,2) = -0.5 * as_scalar( Vsmminusmu.t() * CovV.Cinv * Vsmminusmu);
  
  // R
  vec frRminusdotmu = (fderiv.col(1) - CovR.dotmu - CovR.mphi * Rsmminusmu);
  vec fitLevelErrorR = Rsm - yobs.col(1);
  fitLevelErrorR(find_nonfinite(fitLevelErrorR)).fill(0.0);
  
  res(1,0) = -0.5 * sum(square( fitLevelErrorR )) / pow(sigma,2);
  res(1,1) = -0.5 * as_scalar( frRminusdotmu.t() * CovR.Kinv * frRminusdotmu);
  res(1,2) = -0.5 * as_scalar( Rsmminusmu.t() * CovR.Cinv * Rsmminusmu);
  
  //cout << "lglik component = \n" << res << endl;
  
  ret.value = accu(res);
  
  // cout << "lglik = " << ret.value << endl;
  
  // gradient 
  // V contrib
  mat Vtemp = -CovV.mphi;
  Vtemp.diag() += fderivDx.slice(0).col(0);
  
  vec KinvFrVminusdotmu = (CovV.Kinv * frVminusdotmu);
  
  vec VC2 =  2.0 * join_vert(join_vert( Vtemp.t() * KinvFrVminusdotmu, // n^2 operation
                                        fderivDx.slice(0).col(1) % KinvFrVminusdotmu ),
                                        fderivDtheta.slice(0).t() * KinvFrVminusdotmu );
  
  
  // R contrib
  mat Rtemp = -CovR.mphi;
  Rtemp.diag() += fderivDx.slice(1).col(1);
  
  vec KinvFrRminusdotmu = (CovR.Kinv * frRminusdotmu);
  vec RC2 = 2.0 * join_vert(join_vert( fderivDx.slice(1).col(0) % KinvFrRminusdotmu,
                                       Rtemp.t() * KinvFrRminusdotmu), // n^2 operation
                                       fderivDtheta.slice(1).t() * KinvFrRminusdotmu );
  // 
  // vec C3 = join_vert(join_vert( 2.0 * CovV.CeigenVec * (VsmCTrans % CovV.Ceigen1over),  
  //                               2.0 * CovR.CeigenVec * (RsmCTrans % CovR.Ceigen1over) ), 
  //                               zeros<vec>(theta.size()));
  vec C3 = join_vert(join_vert( 2.0 * CovV.Cinv * Vsmminusmu,  
                                2.0 * CovR.Cinv * Rsmminusmu ), 
                                zeros<vec>(theta.size()));  
  vec C1 = join_vert(join_vert( 2.0 * fitLevelErrorV / pow(sigma,2) ,  
                                2.0 * fitLevelErrorR / pow(sigma,2) ),
                                zeros<vec>(theta.size()));
  
  ret.gradient = ((VC2 + RC2)  + C3 + C1 ) * -0.5;
  
  return ret;
}

//' log likelihood for latent states and ODE theta conditional on phi sigma
//' 
//' the scale is in fact taken out and it is the legacy version of xthetallik
//' 
//' @param phisig      the parameter phi and sigma
//' @param yobs        observed data
lp xthetallik_rescaled( const vec & xtheta, 
                        const gpcov & CovV, 
                        const gpcov & CovR, 
                        const double & sigma, 
                        const mat & yobs, 
                        const std::function<mat (vec, mat)> & fODE) {
  vec xlatent = xtheta.subvec(0, xtheta.size() - 4);
  vec theta = xtheta.subvec(xtheta.size() - 3, xtheta.size() - 1);
  lp ret;
  
  if (min(theta) < 0) {
    ret.value = -1e+9;
    ret.gradient = zeros<vec>(xlatent.size());
    ret.gradient.subvec(xtheta.size() - 3, xtheta.size() - 1).fill(1e9);
    return ret;
  }
  
  vec Vsm = xlatent.subvec(0, xlatent.size()/2 - 1);
  vec Rsm = xlatent.subvec(xlatent.size()/2, xlatent.size() - 1);
  int n = xlatent.size()/2;
  int nobs = 0;
  
  mat fderiv = fODE(theta, join_horiz(Vsm, Rsm));
  mat res(2,3);
  
  // V 
  vec frV = (fderiv.col(0) - CovV.mphi * Vsm);
  //res(0,0) = -0.5 * sum(square( Vsm - yobs.col(0) )) / pow(sigma,2);
  res(0,0) = 0.0;
  for (int i=0; i < n; i++) {
    if (!std::isnan(yobs(i,0))) {
      res(0,0) += pow(Vsm[i] - yobs(i,0),2);
      nobs++;
    }
  }
  res(0,0) = -0.5 * res(0,0) / pow(sigma,2);
  
  res(0,1) = -0.5 * as_scalar( frV.t() * CovV.Kinv * frV) * (double)nobs/(double)n;
  res(0,2) = -0.5 * as_scalar( Vsm.t() * CovV.Cinv * Vsm) * (double)nobs/(double)n;
  // R
  vec frR = (fderiv.col(1) - CovR.mphi * Rsm);
  res(1,0) = 0.0;
  for (int i=0; i < n; i++) {
    if (!std::isnan(yobs(i,1)))
      res(1,0) += pow(Rsm[i] - yobs(i,1),2);
  }
  res(1,0) = -0.5 * res(1,0) / pow(sigma,2);
  
  //res(1,0) = -0.5 * sum(square( Rsm - yobs.col(1) )) / pow(sigma,2);
  res(1,1) = -0.5 * as_scalar( frR.t() * CovR.Kinv * frR) * (double)nobs/(double)n;
  res(1,2) = -0.5 * as_scalar( Rsm.t() * CovR.Cinv * Rsm) * (double)nobs/(double)n;
  
  //cout << "lglik component = \n" << res << endl;
  
  ret.value = accu(res);
  
  // cout << "lglik = " << ret.value << endl;
  
  // gradient 
  // V contrib
  mat Vtemp = eye<mat>(n, n);
  Vtemp.diag() = theta(2)*(1 - square(Vsm));
  Vtemp = Vtemp - CovV.mphi;
  mat Rtemp = eye<mat>(n, n)*theta(2);
  vec aTemp = zeros<vec>(n); 
  vec bTemp = zeros<vec>(n); 
  vec cTemp = fderiv.col(0) / theta(2);
  mat VC2 = join_horiz(join_horiz(join_horiz(join_horiz(Vtemp,Rtemp),aTemp),bTemp),cTemp);
  VC2 = 2.0 * VC2.t() * CovV.Kinv * frV * (double)nobs/(double)n;
  
  // cout << "VC2 = \n" << VC2 << endl;
  
  // R contrib
  Vtemp = eye<mat>(n, n) * -1.0/theta(2);
  Rtemp = eye<mat>(n, n) * -1.0*theta(1)/theta(2) - CovR.mphi;
  aTemp = ones<vec>(n) / theta(2);
  bTemp = -Rsm/theta(2);
  cTemp = -fderiv.col(1) / theta(2);
  mat RC2 = join_horiz(join_horiz(join_horiz(join_horiz(Vtemp,Rtemp),aTemp),bTemp),cTemp);
  RC2 = 2.0 * RC2.t() * CovR.Kinv * frR * (double)nobs/(double)n;
  
  // cout << "RC2 = \n" << RC2 << endl;
  
  vec C3 = join_vert(join_vert( 2.0 * CovV.Cinv * Vsm,  
                                2.0 * CovR.Cinv * Rsm ), 
                                zeros<vec>(theta.size()));
  C3 = C3 * (double)nobs/(double)n;
  vec C1 = join_vert(join_vert( 2.0 * (Vsm - yobs.col(0)) / pow(sigma,2) ,  
                                2.0 * (Rsm - yobs.col(1)) / pow(sigma,2) ),
                                zeros<vec>(theta.size()));
  for (unsigned int i=0; i < C1.size(); i++) {
    if (std::isnan(C1(i)))
      C1(i) = 0.0;
  }
  
  ret.gradient = ((VC2 + RC2)  + C3 + C1 ) * -0.5;
  
  return ret;
}

//' approximate log likelihood for latent states and ODE theta conditional on phi sigma
//' 
//' band matrix approximation
//' 
//' @param phisig      the parameter phi and sigma
//' @param yobs        observed data
//' @noRd
//' FIXME xtheta currently passed by value for Fortran code
lp xthetallikBandApprox( const vec & xtheta, 
                         const std::vector<gpcov> & CovAllDimensions, 
                         const double & sigma, 
                         const mat & yobs,
                         const OdeSystem & fOdeModel) {
  const gpcov & CovV = CovAllDimensions[0];
  const gpcov & CovR = CovAllDimensions[1]; 
  
  int n = (xtheta.size() - 3)/2;
  lp ret;
  const vec & theta = xtheta.subvec(xtheta.size() - 3, xtheta.size() - 1);
  
  if (min(theta) < 0) {
    ret.value = -1e+9;
    ret.gradient = zeros<vec>(2*n);
    ret.gradient.subvec(xtheta.size() - 3, xtheta.size() - 1).fill(1e9);
    return ret;
  }
  
  const vec & Vsm = xtheta.subvec(0, n - 1);
  const vec & Rsm = xtheta.subvec(n, 2*n - 1);
  
  const mat & fderiv = fOdeModel.fOde(theta, join_horiz(Vsm, Rsm));
  const cube & fderivDx = fOdeModel.fOdeDx(theta, join_horiz(Vsm, Rsm));
  const cube & fderivDtheta = fOdeModel.fOdeDtheta(theta, join_horiz(Vsm, Rsm));
  
  mat res(2,3);
  
  // V 
  vec frV(n);
  bmatvecmult(CovV.mphiBand.memptr(), Vsm.memptr(), &(CovV.bandsize), &n, frV.memptr());
  frV = fderiv.col(0) - frV;
  
  vec KinvFrV(n);
  bmatvecmult(CovV.KinvBand.memptr(), frV.memptr(), &(CovV.bandsize), &n, KinvFrV.memptr());
  
  vec CinvVsm(n);
  bmatvecmult(CovV.CinvBand.memptr(), Vsm.memptr(), &(CovV.bandsize), &n, CinvVsm.memptr());
  
  vec fitLevelErrorV = Vsm - yobs.col(0);
  fitLevelErrorV(find_nonfinite(fitLevelErrorV)).fill(0.0);
  res(0,0) = -0.5 * sum(square( fitLevelErrorV )) / pow(sigma,2);
  res(0,1) = -0.5 * sum( frV % KinvFrV);
  res(0,2) = -0.5 * sum( Vsm % CinvVsm);
  
  // R
  vec frR(n);
  bmatvecmult(CovR.mphiBand.memptr(), Rsm.memptr(), &(CovR.bandsize), &n, frR.memptr());
  frR = fderiv.col(1) - frR;
  
  vec KinvFrR(n);
  bmatvecmult(CovR.KinvBand.memptr(), frR.memptr(), &(CovR.bandsize), &n, KinvFrR.memptr());
  
  vec CinvRsm(n);
  bmatvecmult(CovR.CinvBand.memptr(), Rsm.memptr(), &(CovR.bandsize), &n, CinvRsm.memptr());
  
  vec fitLevelErrorR = Rsm - yobs.col(1);
  fitLevelErrorR(find_nonfinite(fitLevelErrorR)).fill(0.0);
  
  res(1,0) = -0.5 * sum(square( fitLevelErrorR )) / pow(sigma,2);
  res(1,1) = -0.5 * sum( frR % KinvFrR);
  res(1,2) = -0.5 * sum( Rsm % CinvRsm);
  
  ret.value = accu(res);
  
  // gradient 
  // V contrib
  mat Vtemp = -CovV.mphiBand;
  Vtemp.row(Vtemp.n_rows/2) += fderivDx.slice(0).col(0).t();
  
  vec VC2part1(n);
  bmatvecmultT(Vtemp.memptr(), KinvFrV.memptr(), &(CovV.bandsize), &n, VC2part1.memptr());
  
  vec VC2 =  2.0 * join_vert(join_vert( VC2part1, 
                                        fderivDx.slice(0).col(1) % KinvFrV ),
                                        fderivDtheta.slice(0).t() * KinvFrV );
  
  
  // R contrib
  mat Rtemp = -CovR.mphiBand;
  Rtemp.row(Rtemp.n_rows/2) += fderivDx.slice(1).col(1).t();
  
  vec RC2part1(n);
  bmatvecmultT(Rtemp.memptr(), KinvFrR.memptr(), &(CovR.bandsize), &n, RC2part1.memptr());
  
  vec RC2 = 2.0 * join_vert(join_vert( fderivDx.slice(1).col(0) % KinvFrR,
                                       RC2part1 ),
                                       fderivDtheta.slice(1).t() * KinvFrR );
  
  vec C3 = join_vert(join_vert( 2.0 * CinvVsm,  
                                2.0 * CinvRsm ), 
                                zeros<vec>(theta.size()));  
  vec C1 = join_vert(join_vert( 2.0 * fitLevelErrorV / pow(sigma,2) ,  
                                2.0 * fitLevelErrorR / pow(sigma,2) ),
                                zeros<vec>(theta.size()));
  
  ret.gradient = ((VC2 + RC2)  + C3 + C1 ) * -0.5;
  
  return ret;
}

// old log likelihood for latent states and ODE theta conditional on phi sigma
// 
// use for benchmarking about speed
lp xthetallikHardCode( const vec & xtheta, 
                       const gpcov & CovV, 
                       const gpcov & CovR, 
                       const double & sigma, 
                       const mat & yobs, 
                       const std::function<mat (vec, mat)> & fODE) {
  int n = (xtheta.size() - 3)/2;
  const vec & theta = xtheta.subvec(xtheta.size() - 3, xtheta.size() - 1);
  lp ret;
  
  if (min(theta) < 0) {
    ret.value = -1e+9;
    ret.gradient = zeros<vec>(2*n);
    ret.gradient.subvec(xtheta.size() - 3, xtheta.size() - 1).fill(1e9);
    return ret;
  }
  
  const vec & Vsm = xtheta.subvec(0, n - 1);
  const vec & Rsm = xtheta.subvec(n, 2*n - 1);
  
  const mat & fderiv = fODE(theta, join_horiz(Vsm, Rsm));
  mat res(2,3);
  
  // V 
  vec frV = (fderiv.col(0) - CovV.mphi * Vsm); // n^2 operation
  vec VsmCTrans = CovV.CeigenVec.t() * Vsm;
  // vec frV = fderiv.col(0) - CovV.mphiLeftHalf * (VsmCTrans % CovV.Ceigen1over);
  vec frVKTrans = CovV.KeigenVec.t() * frV;
  vec fitLevelErrorV = Vsm - yobs.col(0);
  fitLevelErrorV(find_nonfinite(fitLevelErrorV)).fill(0.0);
  res(0,0) = -0.5 * sum(square( fitLevelErrorV )) / pow(sigma,2);
  res(0,1) = -0.5 * sum( square(frVKTrans) % CovV.Keigen1over);
  res(0,2) = -0.5 * sum( square(VsmCTrans) % CovV.Ceigen1over);
  
  // R
  vec frR = (fderiv.col(1) - CovR.mphi * Rsm); // n^2 operation
  vec RsmCTrans = CovR.CeigenVec.t() * Rsm;
  // vec frR = fderiv.col(1) - CovR.mphiLeftHalf * (RsmCTrans % CovR.Ceigen1over);
  vec frRKTrans = CovR.KeigenVec.t() * frR;
  vec fitLevelErrorR = Rsm - yobs.col(1);
  fitLevelErrorR(find_nonfinite(fitLevelErrorR)).fill(0.0);
  
  res(1,0) = -0.5 * sum(square( fitLevelErrorR )) / pow(sigma,2);
  res(1,1) = -0.5 * sum( square(frRKTrans) % CovR.Keigen1over);
  res(1,2) = -0.5 * sum( square(RsmCTrans) % CovR.Ceigen1over);
  
  //cout << "lglik component = \n" << res << endl;
  
  ret.value = accu(res);
  
  // cout << "lglik = " << ret.value << endl;
  
  // gradient 
  // V contrib
  mat Vtemp = -CovV.mphi;
  Vtemp.diag() += theta(2)*(1 - square(Vsm));
  
  vec KinvFrV = (CovV.KeigenVec * (frVKTrans % CovV.Keigen1over));
  vec abcTemp = zeros<vec>(3);
  abcTemp(2) = sum(KinvFrV % fderiv.col(0)) / theta(2);
  vec VC2 =  2.0 * join_vert(join_vert( Vtemp.t()*KinvFrV, // n^2 operation
                                        theta(2) * KinvFrV ),
                                        abcTemp );
  
  
  // R contrib
  mat Rtemp = -CovR.mphi;
  Rtemp.diag() -= theta(1)/theta(2);
  
  vec KinvFrR = (CovR.KeigenVec * (frRKTrans % CovR.Keigen1over));
  abcTemp.fill(0);
  abcTemp(0) = sum(KinvFrR) / theta(2);
  abcTemp(1) = -sum(Rsm % KinvFrR) / theta(2);
  abcTemp(2) = -sum(fderiv.col(1) % KinvFrR) / theta(2);
  vec RC2 = 2.0 * join_vert(join_vert( -KinvFrR / theta(2),
                                       Rtemp.t() * KinvFrR), // n^2 operation
                                       abcTemp );
  
  vec C3 = join_vert(join_vert( 2.0 * CovV.CeigenVec * (VsmCTrans % CovV.Ceigen1over),  
                                2.0 * CovR.CeigenVec * (RsmCTrans % CovR.Ceigen1over) ), 
                                zeros<vec>(theta.size()));
  vec C1 = join_vert(join_vert( 2.0 * fitLevelErrorV / pow(sigma,2) ,  
                                2.0 * fitLevelErrorR / pow(sigma,2) ),
                                zeros<vec>(theta.size()));
  
  ret.gradient = ((VC2 + RC2)  + C3 + C1 ) * -0.5;
  
  return ret;
}

lp xthetallikBandApproxHardCode( const vec & xtheta, 
                                 const gpcov & CovV, 
                                 const gpcov & CovR, 
                                 const double & sigma, 
                                 const mat & yobs,
                                 const std::function<mat (vec, mat)> & fODE) {
  int n = (xtheta.size() - 3)/2;
  lp ret;
  ret.gradient.set_size(xtheta.size());
  
  const double *xthetaPtr = xtheta.memptr();
  const double *VmphiPtr = CovV.mphiBand.memptr();
  const double *VKinvPtr = CovV.KinvBand.memptr();
  const double *VCinvPtr = CovV.CinvBand.memptr();
  const double *RmphiPtr = CovR.mphiBand.memptr(); 
  const double *RKinvPtr = CovR.KinvBand.memptr(); 
  const double *RCinvPtr = CovR.CinvBand.memptr();
  const double *sigmaPtr = &sigma; 
  const double *yobsPtr = yobs.memptr();
  double *retPtr = &ret.value;
  double *retgradPtr = ret.gradient.memptr();
  
  xthetallikBandC( xthetaPtr, VmphiPtr, VKinvPtr, VCinvPtr,
                   RmphiPtr, RKinvPtr, RCinvPtr, &CovV.bandsize, &n,
                   sigmaPtr, yobsPtr, retPtr, retgradPtr, fODE);
  
  return ret;
}

lp xthetallikTwoDimension( const vec & xtheta, 
                           const gpcov & CovV, 
                           const gpcov & CovR, 
                           const double & sigma, 
                           const mat & yobs, 
                           const OdeSystem & fOdeModel) {
  int n = (xtheta.size() - 3)/2;
  const vec & theta = xtheta.subvec(xtheta.size() - 3, xtheta.size() - 1);
  lp ret;
  
  if (min(theta) < 0) {
    ret.value = -1e+9;
    ret.gradient = zeros<vec>(2*n);
    ret.gradient.subvec(xtheta.size() - 3, xtheta.size() - 1).fill(1e9);
    return ret;
  }
  
  const vec & Vsm = xtheta.subvec(0, n - 1);
  const vec & Rsm = xtheta.subvec(n, 2*n - 1);
  
  const mat & fderiv = fOdeModel.fOde(theta, join_horiz(Vsm, Rsm));
  const cube & fderivDx = fOdeModel.fOdeDx(theta, join_horiz(Vsm, Rsm));
  const cube & fderivDtheta = fOdeModel.fOdeDtheta(theta, join_horiz(Vsm, Rsm));
  
  mat res(2,3);
  
  // V 
  vec frV = (fderiv.col(0) - CovV.mphi * Vsm); // n^2 operation
  vec fitLevelErrorV = Vsm - yobs.col(0);
  fitLevelErrorV(find_nonfinite(fitLevelErrorV)).fill(0.0);
  res(0,0) = -0.5 * sum(square( fitLevelErrorV )) / pow(sigma,2);
  res(0,1) = -0.5 * as_scalar( frV.t() * CovV.Kinv * frV);
  res(0,2) = -0.5 * as_scalar( Vsm.t() * CovV.Cinv * Vsm);
  
  // R
  vec frR = (fderiv.col(1) - CovR.mphi * Rsm); // n^2 operation
  vec fitLevelErrorR = Rsm - yobs.col(1);
  fitLevelErrorR(find_nonfinite(fitLevelErrorR)).fill(0.0);
  
  res(1,0) = -0.5 * sum(square( fitLevelErrorR )) / pow(sigma,2);
  res(1,1) = -0.5 * as_scalar( frR.t() * CovR.Kinv * frR);
  res(1,2) = -0.5 * as_scalar( Rsm.t() * CovR.Cinv * Rsm);
  
  //cout << "lglik component = \n" << res << endl;
  
  ret.value = accu(res);
  
  // cout << "lglik = " << ret.value << endl;
  
  // gradient 
  // V contrib
  mat Vtemp = -CovV.mphi;
  Vtemp.diag() += fderivDx.slice(0).col(0);
  
  vec KinvFrV = (CovV.Kinv * frV);
  
  vec VC2 =  2.0 * join_vert(join_vert( Vtemp.t() * KinvFrV, // n^2 operation
                                        fderivDx.slice(0).col(1) % KinvFrV ),
                                        fderivDtheta.slice(0).t() * KinvFrV );
  
  
  // R contrib
  mat Rtemp = -CovR.mphi;
  Rtemp.diag() += fderivDx.slice(1).col(1);
  
  vec KinvFrR = (CovR.Kinv * frR);
  vec RC2 = 2.0 * join_vert(join_vert( fderivDx.slice(1).col(0) % KinvFrR,
                                       Rtemp.t() * KinvFrR), // n^2 operation
                                       fderivDtheta.slice(1).t() * KinvFrR );
  
  vec C3 = join_vert(join_vert( 2.0 * CovV.Cinv * Vsm,  
                                2.0 * CovR.Cinv * Rsm ), 
                                zeros<vec>(theta.size()));  
  vec C1 = join_vert(join_vert( 2.0 * fitLevelErrorV / pow(sigma,2) ,  
                                2.0 * fitLevelErrorR / pow(sigma,2) ),
                                zeros<vec>(theta.size()));
  
  ret.gradient = ((VC2 + RC2)  + C3 + C1 ) * -0.5;
  
  return ret;
}

//' log likelihood for Gaussian Process marginal likelihood with Matern kernel
//' 
//' @param phisig      the parameter phi and sigma
//' @param yobs        observed data
lp phisigllikHard2D( const vec & phisig, 
                     const mat & yobs, 
                     const mat & dist, 
                     string kernel){
  int n = yobs.n_rows;
  int p = phisig.size();
  double sigma = phisig(p-1);
  vec res(2);
  
  // likelihood value part
  std::function<gpcov(vec, mat, int)> kernelCov;
  if(kernel == "matern"){
    kernelCov = maternCov;
  }else if(kernel == "rbf"){
    kernelCov = rbfCov;
  }else if(kernel == "compact1"){
    kernelCov = compact1Cov;
  }else{
    throw "kernel is not specified correctly";
  }
  
  // V 
  gpcov CovV = kernelCov(phisig.subvec(0,(p-1)/2-1), dist, 1);
  mat Kv = CovV.C+ eye<mat>(n,n)*pow(sigma, 2);
  mat Kvl = chol(Kv, "lower");
  mat Kvlinv = inv(Kvl);
  vec veta = Kvlinv * yobs.col(0);
  res(0) = -n/2.0*log(2.0*datum::pi) - sum(log(Kvl.diag())) - 0.5*sum(square(veta));
  
  // R
  gpcov CovR = kernelCov(phisig.subvec((p-1)/2,p-2), dist, 1);
  mat Kr = CovR.C+ eye<mat>(n,n)*pow(sigma, 2);
  mat Krl = chol(Kr, "lower");
  mat Krlinv = inv(Krl);
  vec reta = Krlinv * yobs.col(1);
  res(1) = -n/2.0*log(2.0*datum::pi) - sum(log(Krl.diag())) - 0.5*sum(square(reta));
  
  lp ret;  
  ret.value = sum(res);
  
  // gradient part
  // V contrib
  mat Kvinv = Kvlinv.t() * Kvlinv;
  mat alphaV = Kvlinv.t() * veta;
  mat facVtemp = alphaV * alphaV.t() - Kvinv;
  double dVdsig = sigma * sum(facVtemp.diag());
  
  vec dVdphi(CovV.dCdphiCube.n_slices);
  for(unsigned int i=0; i<dVdphi.size(); i++){
    dVdphi(i) = accu(facVtemp % CovV.dCdphiCube.slice(i))/2.0;
  }
  
  // R contrib
  mat Krinv = Krlinv.t() * Krlinv;
  mat alphaR = Krlinv.t() * reta;
  mat facRtemp = alphaR * alphaR.t() - Krinv;
  double dRdsig = sigma * sum(facRtemp.diag());
  
  vec dRdphi(CovR.dCdphiCube.n_slices);
  for(unsigned int i=0; i<dRdphi.size(); i++){
    dRdphi(i) = accu(facRtemp % CovR.dCdphiCube.slice(i))/2.0;
  }
  
  ret.gradient.set_size(p);
  ret.gradient.subvec(0, dVdphi.size()-1) = dVdphi;
  ret.gradient.subvec(dVdphi.size(), p-2) = dRdphi;
  ret.gradient(p-1) = dVdsig+dRdsig;
  
  // cout << ret.value << endl << ret.gradient;
  
  return ret;
}
