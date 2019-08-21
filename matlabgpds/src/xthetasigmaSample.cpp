#include "mex.h"
#include "armaMex.hpp"
#include "../gpds_cpp/tgtdistr.h"
#include "../gpds_cpp/dynamicalSystemModels.h"
#include "../gpds_cpp/xthetasigma.h"
#include "../gpds_cpp/hmc.h"

using namespace arma;

void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{

// Rcpp::List xthetasigmaSample( const arma::mat & yobs, 
//                               const Rcpp::List & covList, 
//                               const arma::vec & sigmaInit,
//                               const arma::vec & xthetaInit, 
//                               const arma::vec & step,
//                               const int nsteps = 1, 
//                               const bool traj = false, 
//                               const std::string loglikflag = "usual",
//                               const Rcpp::NumericVector & priorTemperatureInput = 1.0, 
//                               const std::string modelName = "FN"){
  
//  const arma::vec priorTemperature = Rcpp::as<arma::vec>(priorTemperatureInput);
  mat yobs = armaGetPr(prhs[0]);
  int NStructElems = mxGetNumberOfElements(prhs[1]);
  vec sigmaInit = armaGetPrVec(prhs[2]);
  vec xthetaInit = armaGetPrVec(prhs[3]);
  vec step = armaGetPrVec(prhs[4]);;
  int nsteps = mxGetScalar(prhs[5]);
  const bool traj = mxGetLogicals(prhs[6])[0];
  
  char *str1, *str2;
  str1 = mxArrayToString(prhs[7]);
  string loglikflag = str1;
  
  vec priorTemperature = armaGetPrVec(prhs[8]);
  
  str2 = mxArrayToString(prhs[9]);
  string modelName = str2;
  
  vec sigma( yobs.n_cols);
  if(sigmaInit.size() == 1){
    sigma.fill( as_scalar( sigmaInit));
  }else if(sigmaInit.size() == yobs.n_cols){
    sigma = sigmaInit;
  }else{
    throw std::runtime_error("sigmaInit size not right");
  }
  
  //cout << NStructElems << "\n";
  
  vector<gpcov> covAllDimensions(NStructElems);
  for(unsigned int j = 0; j < NStructElems; j++){
    //covAllDimensions[i] = cov_r2cpp(covList[i]);
    covAllDimensions[j].Cinv = armaGetPr(mxGetField(prhs[1], j, "Cinv"));
    covAllDimensions[j].mphi = armaGetPr(mxGetField(prhs[1], j, "mphi"));
    covAllDimensions[j].Kinv = armaGetPr(mxGetField(prhs[1], j, "Kinv"));
    covAllDimensions[j].CinvBand = armaGetPr(mxGetField(prhs[1], j, "CinvBand"));
    covAllDimensions[j].mphiBand = armaGetPr(mxGetField(prhs[1], j, "mphiBand"));
    covAllDimensions[j].KinvBand = armaGetPr(mxGetField(prhs[1], j, "KinvBand"));
    covAllDimensions[j].mu = armaGetPrVec(mxGetField(prhs[1], j, "mu"));
    covAllDimensions[j].dotmu = armaGetPrVec(mxGetField(prhs[1], j, "dotmu"));      
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
  
  
//   vec initial = join_vert(xthetaInit, sigmaInit);
//       const mat & xlatent = mat(const_cast<double*>( initial.memptr()), yobs.n_rows, yobs.n_cols, false, false);
//       const vec & theta = vec(const_cast<double*>( initial.memptr() + yobs.size()), xthetaInit.size() - yobs.size(), false, false);
//       const vec & sigmat = vec(const_cast<double*>( initial.memptr() + yobs.size() + theta.size()), sigmaInit.size(), false, false);
//       xthetasigmallik( xlatent, 
//                               theta, 
//                               sigmat, 
//                               yobs, 
//                               covAllDimensions,
//                               model,
//                               priorTemperature,
//                               false,
//                               useMean);  
//       
//       cout << xlatent << "\n\n";
//       cout << theta << "\n\n";
//       cout << sigmat << "\n\n";
      
  
  
  std::function<lp(vec)> tgt = [&](const vec & xthetasigma) -> lp{
      const mat & xlatent = mat(const_cast<double*>( xthetasigma.memptr()), yobs.n_rows, yobs.n_cols, false, false);
      const vec & theta = vec(const_cast<double*>( xthetasigma.memptr() + yobs.size()), xthetaInit.size() - yobs.size(), false, false);
      const vec & sigma = vec(const_cast<double*>( xthetasigma.memptr() + yobs.size() + theta.size()), sigmaInit.size(), false, false);
      return xthetasigmallik( xlatent, 
                              theta, 
                              sigma, 
                              yobs, 
                              covAllDimensions,
                              model,
                              priorTemperature,
                              false,  //band not working in Matlab
                              useMean);
    };
  
  vec lb(xthetaInit.size() + sigmaInit.size());
  lb.subvec(0, yobs.size()-1).fill(-datum::inf);
  lb.subvec(yobs.size(), xthetaInit.size()-1) = model.thetaLowerBound;
  lb.subvec(xthetaInit.size(), xthetaInit.size() + sigmaInit.size() - 1).fill(1e-3);
  
  vec initial = join_vert(xthetaInit, sigmaInit);
  
  //initial.print();
  //lb.print();
  
  //step = {1, 2};
  
  //cout << step << " " << traj << "\n";
  //return;
  
  const hmcstate & post = basic_hmcC(tgt, initial, step, lb, {datum::inf}, nsteps, traj);
  
  const char* field_names[] = {"final", "final_p", "lpr", "step", "apr", "acc", "delta"};
  //const char* field_names[] = {"final"};
  mwSize dims[1] = {1};
  plhs[0] = mxCreateStructArray(1, dims, 7, field_names);
  
  mxArray* tmp1 = mxCreateDoubleMatrix(post.final.n_rows, post.final.n_cols, mxREAL); 
  armaSetPr(tmp1, post.final);
  mxSetField( plhs[0], 0, "final", tmp1 ); 
  
  mxArray* tmp2 = mxCreateDoubleMatrix(post.finalp.n_rows, post.finalp.n_cols, mxREAL); 
  armaSetPr(tmp2, post.finalp);
  mxSetField( plhs[0], 0, "final_p", tmp2 ); 
  
  mxArray* tmp3 = mxCreateDoubleMatrix(post.step.n_rows, post.step.n_cols, mxREAL); 
  armaSetPr(tmp3, post.step);
  mxSetField( plhs[0], 0, "step", tmp3 ); 
  
  
  mxSetField( plhs[0], 0, "lpr", mxCreateDoubleScalar(post.lprvalue));     
  mxSetField( plhs[0], 0, "apr", mxCreateDoubleScalar(post.apr));
  mxSetField( plhs[0], 0, "acc", mxCreateDoubleScalar(post.acc));
  mxSetField( plhs[0], 0, "delta", mxCreateDoubleScalar(post.delta));
  
  //post.final.print();
  //mxCreateDoubleScalar(post.final);
  
//   Rcpp::List ret = Rcpp::List::create(Rcpp::Named("final")=post.final,
//                                       Rcpp::Named("final.p")=post.finalp,
//                                       Rcpp::Named("lpr")=post.lprvalue,
//                                       Rcpp::Named("step")=post.step,
//                                       Rcpp::Named("apr")=post.apr,
//                                       Rcpp::Named("acc")=post.acc,
//                                       Rcpp::Named("delta")=post.delta);
//   if(traj){
//     ret.push_back(post.trajp, "traj.p");
//     ret.push_back(post.trajq, "traj.q");
//     ret.push_back(post.trajH, "traj.H");
//   }
//   return ret;
}
