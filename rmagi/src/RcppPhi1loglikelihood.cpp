#include "classDefinition.h"
#include "fullloglikelihood.h"
#include "tgtdistr.h"
#include "hmc.h"
#include "dynamicalSystemModels.h"
#include "band.h"
#include "phi1loglikelihood.h"
#include "RcppWrapper.h"

using namespace arma;


//' R wrapper for xthetaphi1sigmallik
//' the phi1 can be sampled together without hurting computational speed because the kernel matrix is scaled with phi1,
//'     so we don't need to re-calculate the inverse. This is the function to calculate the log posterior including phi1.
//' not used in the final method because phi1 sample is not stable.
//' @noRd
// [[Rcpp::export]]
Rcpp::List xthetaphi1sigmallikRcpp( const arma::mat & xlatent, 
                                    const arma::vec & theta, 
                                    const arma::vec & phi1, 
                                    const arma::vec & sigma, 
                                    const arma::mat & yobs, 
                                    const Rcpp::List & covAllDimInput,
                                    const Rcpp::NumericVector & priorTemperatureInput = 1.0,
                                    const bool useBand = false,
                                    const bool useMean = false,
                                    const std::string modelName = "FN"){
  
  OdeSystem model;
  if(modelName == "FN"){
    model = OdeSystem(fnmodelODE, fnmodelDx, fnmodelDtheta, zeros(3), ones(3)*datum::inf);
  }else if(modelName == "Hes1"){
    model = OdeSystem(hes1modelODE, hes1modelDx, hes1modelDtheta, zeros(7), ones(7)*datum::inf); 
  }else if(modelName == "Hes1-log"){
    model = OdeSystem(hes1logmodelODE, hes1logmodelDx, hes1logmodelDtheta, zeros(7), ones(7)*datum::inf); 
  }else if(modelName == "HIV"){
    model = OdeSystem(HIVmodelODE, HIVmodelDx, HIVmodelDtheta, {-datum::inf, 0,0,0,0,0, -datum::inf,-datum::inf,-datum::inf}, ones(9)*datum::inf);   
  }else{
    throw std::runtime_error("modelName must be one of 'FN', 'Hes1', 'Hes1-log', 'HIV'");
  }
  
  vector<gpcov> covAllDimensions(yobs.n_cols);
  for(unsigned j = 0; j < yobs.n_cols; j++){
    covAllDimensions[j] = cov_r2cpp(covAllDimInput[j]);
  }
  
  const arma::vec priorTemperature = Rcpp::as<arma::vec>(priorTemperatureInput);
  
  lp ret = xthetaphi1sigmallik(xlatent, 
                               theta, 
                               phi1,
                               sigma, 
                               yobs, 
                               covAllDimensions,
                               model,
                               priorTemperature,
                               useBand,
                               useMean);
  
  return Rcpp::List::create(Rcpp::Named("value")=ret.value,
                            Rcpp::Named("grad")=ret.gradient);
}


//' sample from GP ODE for latent x, theta, sigma, and phi1
//' the phi1 can be sampled together without hurting computational speed because the kernel matrix is scaled with phi1,
//'     so we don't need to re-calculate the inverse.
//' not used in the final method because phi1 sample is not stable.
//' @noRd
// [[Rcpp::export]]
Rcpp::List xthetaphi1sigmaSample( const arma::mat & yobs, 
                                  const Rcpp::List & covList,
                                  const arma::vec & phi1Init,
                                  const arma::vec & sigmaInit,
                                  const arma::vec & xthetaInit, 
                                  const arma::vec & step,
                                  const int nsteps = 1, 
                                  const bool traj = false, 
                                  const std::string loglikflag = "usual",
                                  const Rcpp::NumericVector & priorTemperatureInput = 1.0, 
                                  const std::string modelName = "FN"){
  
  const arma::vec priorTemperature = Rcpp::as<arma::vec>(priorTemperatureInput);
  
  vec sigma( yobs.n_cols);
  if(sigmaInit.size() == 1){
    sigma.fill( as_scalar( sigmaInit));
  }else if(sigmaInit.size() == yobs.n_cols){
    sigma = sigmaInit;
  }else{
    throw std::runtime_error("sigmaInit size not right");
  }
  
  vector<gpcov> covAllDimensions(covList.size());
  for(unsigned int i = 0; i < covList.size(); i++){
    covAllDimensions[i] = cov_r2cpp(covList[i]);
  }
  
  OdeSystem model;
  if(modelName == "FN"){
    model = OdeSystem(fnmodelODE, fnmodelDx, fnmodelDtheta, zeros(3), ones(3)*datum::inf);
  }else if(modelName == "Hes1"){
    model = OdeSystem(hes1modelODE, hes1modelDx, hes1modelDtheta, zeros(7), ones(7)*datum::inf); 
  }else if(modelName == "Hes1-log"){
    model = OdeSystem(hes1logmodelODE, hes1logmodelDx, hes1logmodelDtheta, zeros(7), ones(7)*datum::inf); 
  }else if(modelName == "HIV"){
    model = OdeSystem(HIVmodelODE, HIVmodelDx, HIVmodelDtheta, {-datum::inf, 0,0,0,0,0, -datum::inf,-datum::inf,-datum::inf}, ones(9)*datum::inf);   
  }else{
    throw std::runtime_error("modelName must be one of 'FN', 'Hes1', 'Hes1-log', 'HIV'");
  }
  
  bool useBand = false;
  if(loglikflag == "band" || loglikflag == "withmeanBand"){
    useBand = true;
  }
  bool useMean = false;
  if(loglikflag == "withmean" || loglikflag == "withmeanBand"){
    useMean = true;
  }
  
  std::function<lp(vec)> tgt = [&](const vec & xthetaphi1sigma) -> lp{
    const mat & xlatent = mat(const_cast<double*>( xthetaphi1sigma.memptr()), yobs.n_rows, yobs.n_cols, false, false);
    const vec & theta = vec(const_cast<double*>( xthetaphi1sigma.memptr() + yobs.size()), xthetaInit.size() - yobs.size(), false, false);
    const vec & phi1 = vec(const_cast<double*>( xthetaphi1sigma.memptr() + yobs.size() + theta.size()), phi1Init.size(), false, false);
    const vec & sigma = vec(const_cast<double*>( xthetaphi1sigma.memptr() + yobs.size() + theta.size() + phi1.size()), sigmaInit.size(), false, false);
    return xthetaphi1sigmallik( xlatent, 
                                theta, 
                                phi1,
                                sigma, 
                                yobs, 
                                covAllDimensions,
                                model,
                                priorTemperature,
                                useBand,
                                useMean);
  };
  
  vec lb(xthetaInit.size() + phi1Init.size() + sigmaInit.size());
  lb.subvec(0, yobs.size()-1).fill(-datum::inf);
  lb.subvec(yobs.size(), xthetaInit.size()-1) = model.thetaLowerBound;
  lb.subvec(xthetaInit.size(), lb.size() - 1).fill(1e-3);
  
  vec initial = join_vert(join_vert(xthetaInit, phi1Init), sigmaInit);
  hmcstate post = basic_hmcC(tgt, initial, step, lb, {datum::inf}, nsteps, traj);
  
  Rcpp::List ret = Rcpp::List::create(Rcpp::Named("final")=post.final,
                                      Rcpp::Named("final.p")=post.finalp,
                                      Rcpp::Named("lpr")=post.lprvalue,
                                      Rcpp::Named("step")=post.step,
                                      Rcpp::Named("apr")=post.apr,
                                      Rcpp::Named("acc")=post.acc,
                                      Rcpp::Named("delta")=post.delta);
  if(traj){
    ret.push_back(post.trajp, "traj.p");
    ret.push_back(post.trajq, "traj.q");
    ret.push_back(post.trajH, "traj.H");
  }
  return ret;
}
