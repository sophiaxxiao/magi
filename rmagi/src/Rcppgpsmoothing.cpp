#define NDEBUG
#define BOOST_DISABLE_ASSERTS
#define EIGEN_NO_DEBUG

#include "RcppArmadillo.h"

// [[Rcpp::depends(roptim)]]
#include <roptim.h>

#include "tgtdistr.h"
#include "fullloglikelihood.h"


// [[Rcpp::export]]
arma::vec calcFrequencyBasedPrior(const arma::vec & x){
    arma::cx_mat z = arma::fft(x);
    arma::vec zmod(z.n_rows);
    for(unsigned int i = 0; i < z.n_rows; i++){
        zmod(i) = std::sqrt(std::norm(z(i, 0)));
    }
    const arma::vec & zmodEffective = zmod(arma::span(1, (zmod.size() - 1) / 2));
    double freq = 1;
    const arma::vec & zmodEffectiveSq = arma::square(zmodEffective);
    freq = arma::sum(arma::linspace<arma::vec>(1, zmodEffective.size(), zmodEffective.size()) % zmodEffectiveSq) / arma::sum(zmodEffectiveSq);

    double meanFactor = 0.5 / freq;
    double sdFactor = (1 - meanFactor) / 3;

    return arma::vec({meanFactor, sdFactor});
}


class PhiGaussianProcessSmoothing : public roptim::Functor{
public:
    std::string kernel;
    const arma::mat & yobs;
    const arma::mat & dist;
    const unsigned int numparam;
    const double sigmaExogenScalar;
    const bool useFrequencyBasedPrior;
    arma::vec priorFactor;
    double maxDist;
    arma::vec lb;
    arma::vec ub;

    double operator()(const arma::vec & phisigInput, arma::vec & grad)  {
        if (arma::any(phisigInput < lb)){
            grad.fill(0);
            for(unsigned i = 0; i < numparam; i++){
                if(phisigInput(i) < lb(i)){
                    grad(i) = -1;
                }
            }
            return 1E16;
        }
        if (arma::any(phisigInput > ub)){
            grad.fill(0);
            for(unsigned i = 0; i < numparam; i++){
                if(phisigInput(i) > ub(i)){
                    grad(i) = 1;
                }
            }
            return 1E16;
        }
        arma::vec phisig = phisigInput;
        if(sigmaExogenScalar > 0){
            phisig = arma::join_vert(phisig, arma::vec({sigmaExogenScalar}));
        }
        const lp & out = phisigllik(phisig, yobs, dist, kernel);
        for(unsigned i = 0; i < numparam; i++){
            grad(i) = -out.gradient(i);
        }
        double penalty = 0;
        if (useFrequencyBasedPrior) {
            for (unsigned j = 0; j < yobs.n_cols; j++){
                penalty = (phisig(2*j+1) - maxDist * priorFactor(0)) / std::pow((maxDist * priorFactor(1)), 2);
                grad(2*j+1) += penalty;
            }
        }

        penalty = 0;
        if (useFrequencyBasedPrior) {
            for (unsigned j = 0; j < yobs.n_cols; j++){
                penalty += -0.5 * std::pow((phisig(2*j+1) - maxDist * priorFactor(0)) / (maxDist * priorFactor(1)), 2);
            }
        }
        return -(out.value + penalty);
    }

    double operator()(const arma::vec & phisigInput) override {
        arma::vec grad = arma::zeros(phisigInput.size());
        double value = this->operator()(phisigInput, grad);
        return value;
    }

    void Gradient(const arma::vec & phisigInput, arma::vec & grad) override {
        if (grad.size() != phisigInput.size()){
            grad = arma::zeros(phisigInput.size());
        }
        this->operator()(phisigInput, grad);
    }

    PhiGaussianProcessSmoothing(const arma::mat & yobsInput,
                                const arma::mat & distInput,
                                std::string kernelInput,
                                const unsigned int numparamInput,
                                const double sigmaExogenScalarInput,
                                const bool useFrequencyBasedPriorInput) :
            kernel(std::move(kernelInput)),
            yobs(yobsInput),
            dist(distInput),
            numparam(numparamInput),
            sigmaExogenScalar(sigmaExogenScalarInput),
            useFrequencyBasedPrior(useFrequencyBasedPriorInput) {
        unsigned int phiDim;
        if(kernel == "generalMatern") {
            phiDim = 2;
        }else if(kernel == "matern") {
            phiDim = 2;
        }else if(kernel == "compact1") {
            phiDim = 2;
        }else if(kernel == "periodicMatern"){
            phiDim = 3;
        }else{
            throw std::invalid_argument("kernelInput invalid");
        }

        lb = arma::ones(numparam);
        lb.fill(1e-4);

        maxDist = dist.max();
        double maxScale = arma::max(arma::abs(yobs(arma::find_finite(yobs))));
        maxScale = std::max(maxScale, maxDist);

        ub = arma::ones(numparam);
        ub.fill(10 * maxScale);
        for(unsigned i = 0; i < yobsInput.n_cols; i++) {
            const arma::uvec finite_elem = arma::find_finite(yobs.col(i));
            if (finite_elem.size() > 0){
                ub(phiDim * i) = 100 * arma::max(arma::abs((yobs.col(i).eval().elem(finite_elem))));
            }
            ub(phiDim * i + 1) = maxDist;
        }

        priorFactor = arma::zeros(2);
        if(useFrequencyBasedPrior){
            for (unsigned j = 0; j < yobs.n_cols; j++){
                priorFactor += calcFrequencyBasedPrior(yobs.col(j));
            }
            priorFactor /= yobs.n_cols;
//            Rcpp::Rcout << "priorFactor =\n" << priorFactor << "\n";
        }
    }
};


// [[Rcpp::export]]
arma::vec gpsmooth(const arma::mat & yobsInput,
                   const arma::mat & distInput,
                   std::string kernelInput,
                   const double sigmaExogenScalar = -1,
                   bool useFrequencyBasedPrior = false) {
    unsigned int phiDim;
    if(kernelInput == "generalMatern") {
        phiDim = 2;
    }else if(kernelInput == "matern") {
        phiDim = 2;
    }else if(kernelInput == "compact1") {
        phiDim = 2;
    }else if(kernelInput == "periodicMatern"){
        phiDim = 3;
    }else{
        throw std::invalid_argument("kernelInput invalid");
    }
    unsigned int numparam;

    if(sigmaExogenScalar > 0){
        numparam = phiDim * yobsInput.n_cols;
    }else{
        numparam = phiDim * yobsInput.n_cols + 1;
    }

    PhiGaussianProcessSmoothing objective(yobsInput, distInput, std::move(kernelInput), numparam, sigmaExogenScalar, useFrequencyBasedPrior);

    roptim::Roptim<PhiGaussianProcessSmoothing> opt("L-BFGS-B");
    opt.set_lower(objective.lb);
    opt.set_upper(objective.ub);

    // phi sigma 1st initial value for optimization
    arma::vec phisigAttempt1(numparam);
    phisigAttempt1.fill(1);
    double maxDist = distInput.max();
    double sdOverall = 0;
    for(unsigned i = 0; i < yobsInput.n_cols; i++) {
        phisigAttempt1[phiDim * i] = 0.5 * arma::stddev(yobsInput.col(i));
        phisigAttempt1[phiDim * i + 1] = 0.5 * maxDist;
        sdOverall += phisigAttempt1[phiDim * i];
    }
    if(sigmaExogenScalar <= 0){
        phisigAttempt1[phiDim * yobsInput.n_cols] = sdOverall / yobsInput.n_cols;
    }

    opt.minimize(objective, phisigAttempt1);
    double fx1 = opt.value();
    phisigAttempt1 = opt.par();

    // phi sigma 2nd initial value for optimization
    arma::vec phisigAttempt2(numparam);
    phisigAttempt2.fill(1);
    for(unsigned i = 0; i < yobsInput.n_cols; i++) {
        phisigAttempt2[phiDim * i] = arma::stddev(yobsInput.col(i));
        if (useFrequencyBasedPrior){
            phisigAttempt2[phiDim * i + 1] = maxDist * objective.priorFactor(0);
        }else{
            arma::vec distVec = arma::vectorise(distInput);
            phisigAttempt2[phiDim * i + 1] = distVec(arma::find(distVec > 1e-8)).eval().min();
//            Rcpp::Rcout << "phisigAttempt2[phiDim * i + 1] init = " << phisigAttempt2[phiDim * i + 1] << "\n";
        }
    }
    if(sigmaExogenScalar <= 0){
        phisigAttempt2[phiDim * yobsInput.n_cols] = sdOverall / yobsInput.n_cols * 0.2;
    }

    opt.minimize(objective, phisigAttempt2);
    double fx2 = opt.value();
    phisigAttempt2 = opt.par();

    arma::vec phisig;
    if (fx1 < fx2){
        phisig = phisigAttempt1;
    }else{
        phisig = phisigAttempt2;
    }

    return phisig;
}


// [[Rcpp::export]]
arma::cube calcMeanCurve(const arma::vec & xInput,
                         const arma::vec & yInput,
                         const arma::vec & xOutput,
                         const arma::mat & phiCandidates,
                         const arma::vec & sigmaCandidates,
                         const std::string kerneltype = "generalMatern",
                         const bool useDeriv = false) {
    if(kerneltype != "generalMatern") Rcpp::Rcerr << "kerneltype other than generalMatern is not supported\n";

    const arma::vec & tvec = arma::join_vert(xOutput, xInput);
    arma::mat distSigned(tvec.size(), tvec.size());
    for(unsigned int i = 0; i < distSigned.n_cols; i++){
        distSigned.col(i) = tvec - tvec(i);
    }
    arma::cube ydyOutput(xOutput.size(), phiCandidates.n_cols, 2, arma::fill::zeros);
    arma::mat & yOutput = ydyOutput.slice(0);
    arma::mat & dyOutput = ydyOutput.slice(1);

    int complexity = 0;
    if(useDeriv){
        complexity = 3;
    }

    for(unsigned it = 0; it < phiCandidates.n_cols; it++){
        const double & sigma = sigmaCandidates(it);
        const arma::vec & phi = phiCandidates.col(it);

        gpcov covObj = generalMaternCov(phi, distSigned, complexity);
        arma::mat C = std::move(covObj.C);

        arma::vec Cdiag = C.diag();
        Cdiag(arma::span(xOutput.size(), Cdiag.size() - 1)) += std::pow(sigma, 2);
        C.diag() = Cdiag;
        yOutput.col(it) = C(arma::span(0, xOutput.size() - 1),
                            arma::span(xOutput.size(), Cdiag.size() - 1)) *
                          arma::solve(C(arma::span(xOutput.size(), Cdiag.size() - 1),
                                        arma::span(xOutput.size(), Cdiag.size() - 1)),
                                      yInput);
        if(useDeriv){
            dyOutput.col(it) = covObj.Cprime(arma::span(0, xOutput.size() - 1),
                                             arma::span(0, xOutput.size() - 1)) *
                               arma::solve(C(arma::span(0, xOutput.size() - 1),
                                             arma::span(0, xOutput.size() - 1)),
                                           yOutput.col(it));
        }
    }
    return ydyOutput;
}


class ThetaOptim : public roptim::Functor {
public:
    const arma::mat & yobs;
    const OdeSystem & fOdeModel;
    const std::vector<gpcov> & covAllDimensions;
    const arma::vec & sigmaAllDimensions;
    const arma::vec & priorTemperature;
    const arma::mat & xInit;
    const bool useBand;
    arma::vec lb;
    arma::vec ub;

    double operator()(const arma::vec & thetaInput, arma::vec & grad) {
        if (arma::any(thetaInput < lb)){
            grad.fill(0);
            for(unsigned i = 0; i < fOdeModel.thetaSize; i++){
                if(thetaInput[i] < lb[i]){
                    grad[i] = -1;
                }
            }
            return 1E16;
        }
        if (arma::any(thetaInput > ub)){
            grad.fill(0);
            for(unsigned i = 0; i < fOdeModel.thetaSize; i++){
                if(thetaInput[i] > ub[i]){
                    grad[i] = 1;
                }
            }
            return 1E16;
        }
        const arma::vec & xtheta = arma::join_vert(
                arma::vectorise(xInit),
                thetaInput
        );
        const lp & out = xthetallik(
                xtheta,
                covAllDimensions,
                sigmaAllDimensions,
                yobs,
                fOdeModel,
                useBand,
                priorTemperature);
        for(unsigned i = 0; i < fOdeModel.thetaSize; i++){
            grad[i] = -out.gradient(xInit.size() + i);
        }
        return -out.value;
    }

    double operator()(const arma::vec & x) override {
        arma::vec grad = arma::zeros(x.size());
        double value = this->operator()(x, grad);
        return value;
    }

    void Gradient(const arma::vec & x, arma::vec & grad) override {
        if (grad.size() != x.size()){
            grad = arma::zeros(x.size());
        }
        this->operator()(x, grad);
    }


    ThetaOptim(const arma::mat & yobsInput,
               const OdeSystem & fOdeModelInput,
               const std::vector<gpcov> & covAllDimensionsInput,
               const arma::vec & sigmaAllDimensionsInput,
               const arma::vec & priorTemperatureInput,
               const arma::mat & xInitInput,
               const bool useBandInput) :
            yobs(yobsInput),
            fOdeModel(fOdeModelInput),
            covAllDimensions(covAllDimensionsInput),
            sigmaAllDimensions(sigmaAllDimensionsInput),
            priorTemperature(priorTemperatureInput),
            xInit(xInitInput),
            useBand(useBandInput) {
        lb = fOdeModel.thetaLowerBound;
        ub = fOdeModel.thetaUpperBound;
    }
};

// [[Rcpp::export]]
arma::vec optimizeThetaInit(const arma::mat & yobsInput,
                            const OdeSystem & fOdeModelInput,
                            const std::vector<gpcov> & covAllDimensionsInput,
                            const arma::vec & sigmaAllDimensionsInput,
                            const arma::vec & priorTemperatureInput,
                            const arma::mat & xInitInput,
                            const bool useBandInput) {
    ThetaOptim objective(yobsInput, fOdeModelInput, covAllDimensionsInput, sigmaAllDimensionsInput, priorTemperatureInput, xInitInput, useBandInput);
    roptim::Roptim<ThetaOptim> opt("L-BFGS-B");
    opt.set_lower(objective.lb);
    opt.set_upper(objective.ub);

    arma::vec theta(fOdeModelInput.thetaSize);
    theta.fill(1);

    opt.minimize(objective, theta);

    return opt.par();
}


class PhiOptim : public roptim::Functor{
public:
    const arma::mat & yobs;
    const arma::vec & tvec;
    const OdeSystem & fOdeModel;
    const arma::vec & sigmaAllDimensions;
    const arma::vec & priorTemperature;
    const arma::mat & xInit;
    const arma::vec & thetaInit;
    const arma::mat & phiFull;
    const arma::uvec & missingComponentDim;
    arma::vec lb;
    arma::vec ub;

    double operator()(const arma::vec & phiInput, arma::vec & grad) {
        if (arma::any(phiInput < lb)){
            grad.fill(0);
            for(unsigned i = 0; i < phiInput.size(); i++){
                if(phiInput[i] < lb[i]){
                    grad[i] = -1;
                }
            }
            return 1E16;
        }
        const arma::mat phiMissingDimensions(
                const_cast<double*>(phiInput.begin()),
                2,
                missingComponentDim.size(),
                false,
                false);
        arma::mat phiAllDimensions = phiFull;
        phiAllDimensions.cols(missingComponentDim) = phiMissingDimensions;

        const lp & out = xthetaphisigmallik( xInit,
                                             thetaInit,
                                             phiAllDimensions,
                                             sigmaAllDimensions,
                                             yobs,
                                             tvec,
                                             fOdeModel);

        for(unsigned i = 0; i < missingComponentDim.size(); i++){
            unsigned currentDim = missingComponentDim[i];
            grad[2*i] = -out.gradient(xInit.size() + thetaInit.size() + 2*currentDim);
            grad[2*i+1] = -out.gradient(xInit.size() + thetaInit.size() + 2*currentDim + 1);
        }
        return -out.value;
    }

    double operator()(const arma::vec & x) override {
        arma::vec grad = arma::zeros(x.size());
        double value = this->operator()(x, grad);
        return value;
    }

    void Gradient(const arma::vec & x, arma::vec & grad) override {
        if (grad.size() != x.size()){
            grad = arma::zeros(x.size());
        }
        this->operator()(x, grad);
    }

    PhiOptim(const arma::mat & yobsInput,
             const arma::vec & tvecInput,
             const OdeSystem & fOdeModelInput,
             const arma::vec & sigmaAllDimensionsInput,
             const arma::vec & priorTemperatureInput,
             const arma::mat & xInitInput,
             const arma::vec & thetaInitInput,
             const arma::mat & phiFullInput,
             const arma::uvec & missingComponentDimInput) :
            yobs(yobsInput),
            tvec(tvecInput),
            fOdeModel(fOdeModelInput),
            sigmaAllDimensions(sigmaAllDimensionsInput),
            priorTemperature(priorTemperatureInput),
            xInit(xInitInput),
            thetaInit(thetaInitInput),
            phiFull(phiFullInput),
            missingComponentDim(missingComponentDimInput) {
        lb = arma::vec(missingComponentDim.size() * 2);
        ub = arma::vec(missingComponentDim.size() * 2);

        const double maxDist = (arma::max(tvecInput) - arma::min(tvecInput));
        const double minDist = arma::min(arma::abs(arma::diff(tvecInput)));
        const double maxScale = arma::max(arma::abs(yobs(arma::find_finite(yobs))));

        arma::vec priorFactor = arma::zeros(2);
        for (unsigned j = 0; j < yobs.n_cols; j++){
            if (arma::any(missingComponentDim == j)){
                continue;
            }
            const arma::vec & yobsThisDim = yobs.col(j);
            priorFactor += calcFrequencyBasedPrior(yobsThisDim(arma::find_finite(yobsThisDim)));
        }
        priorFactor /= (yobs.n_cols - missingComponentDim.size());
//        Rcpp::Rcout << "average priorFactor in PhiOptim =\n" << priorFactor << "\n";

        for(unsigned i = 0; i < missingComponentDim.size(); i++){
            ub[2*i] = maxScale * 5;
            lb[2*i] = maxScale * 1e-3;
            ub[2*i+1] = maxDist * 5;
            lb[2*i+1] = std::min(maxDist * priorFactor(0) * 0.5, minDist);
        }
    }
};


// [[Rcpp::export]]
arma::mat optimizePhi(const arma::mat & yobsInput,
                      const arma::vec & tvecInput,
                      const OdeSystem & fOdeModelInput,
                      const arma::vec & sigmaAllDimensionsInput,
                      const arma::vec & priorTemperatureInput,
                      const arma::mat & xInitInput,
                      const arma::vec & thetaInitInput,
                      const arma::mat & phiInitInput,
                      const arma::uvec & missingComponentDim) {
    PhiOptim objective(yobsInput, tvecInput, fOdeModelInput, sigmaAllDimensionsInput, priorTemperatureInput, xInitInput, thetaInitInput, phiInitInput, missingComponentDim);

    roptim::Roptim<PhiOptim> opt("L-BFGS-B");
    //opt.control.maxit = 1000;
    //opt.control.lmm = 100;
    //opt.control.fnscale = 0.1;
    opt.set_lower(objective.lb);
    opt.set_upper(objective.ub);

    arma::vec phi(2 * missingComponentDim.size());
    for(unsigned i = 0; i < missingComponentDim.size(); i++){
        unsigned currentDim = missingComponentDim[i];
        phi[2*i] = phiInitInput(0, currentDim);
        phi[2*i+1] = phiInitInput(1, currentDim);
    }

//    Rcpp::Rcout << "starting from phi = " << phi.t();
    opt.minimize(objective, phi);
//    Rcpp::Rcout << "; opt.value() = " << opt.value() << "; opt.par() = " << opt.par().t() << "\n";
    //Rcpp::Rcout << "Diagnostics: opt.fncount() = " << opt.fncount() << "; opt.grcount() = " << opt.grcount() << "; opt.convergence() = " << opt.convergence() << "\n";
    double fx_best = opt.value();
    arma::vec phi_argmin_best = opt.par();

    for (unsigned obs_component_each = 0; obs_component_each < yobsInput.n_cols; obs_component_each++){
        if (arma::any(missingComponentDim == obs_component_each)){
            continue;
        }

        for(unsigned i = 0; i < missingComponentDim.size(); i++){
            phi[2*i] = phiInitInput(0, obs_component_each);
            phi[2*i+1] = phiInitInput(1, obs_component_each);
        }

//        Rcpp::Rcout << "starting from phi = " << phi.t();
        opt.minimize(objective, phi);
//        Rcpp::Rcout << "; opt.value() = " << opt.value() << "; opt.par() = " << opt.par().t() << "\n";
        //Rcpp::Rcout << "Diagnostics: opt.fncount() = " << opt.fncount() << "; opt.grcount() = " << opt.grcount() << "; opt.convergence() = " << opt.convergence() << "\n";
        if (opt.value() < fx_best){
            fx_best = opt.value();
            phi_argmin_best = opt.par();
        }
    }

    for(unsigned i = 0; i < missingComponentDim.size(); i++){
        phi[2*i] = 0.24;
        phi[2*i+1] = 20;
    }

//    Rcpp::Rcout << "starting from phi = " << phi.t();
    opt.minimize(objective, phi);
//    Rcpp::Rcout << "; opt.value() = " << opt.value() << "; opt.par() = " << opt.par().t() << "\n";
    //Rcpp::Rcout << "Diagnostics: opt.fncount() = " << opt.fncount() << "; opt.grcount() = " << opt.grcount() << "; opt.convergence() = " << opt.convergence() << "\n";

    if (opt.value() < fx_best){
        fx_best = opt.value();
        phi_argmin_best = opt.par();
    }

    const arma::mat & phiArgmin = arma::reshape(phi_argmin_best, 2, missingComponentDim.size());
    return phiArgmin;
}


class XmissingThetaPhiOptim : public roptim::Functor{
public:
    const arma::mat & yobs;
    const arma::vec & tvec;
    const OdeSystem & fOdeModel;
    const arma::vec & sigmaAllDimensions;
    const arma::vec & priorTemperature;
    arma::mat xInit;
    arma::vec thetaInit;
    arma::mat phiAllDimensions;
    const arma::uvec & missingComponentDim;
    const double SCALE = 1;
    arma::vec lb;
    arma::vec ub;

    double operator()(const arma::vec & xthetaphiInput, arma::vec & grad) {
        if (arma::any(xthetaphiInput < lb)){
            grad.fill(0);
            for(unsigned i = 0; i < xthetaphiInput.size(); i++){
                if(xthetaphiInput[i] < lb[i]){
                    grad[i] = -1;
                }
            }
            return 1E16;
        }
        if (arma::any(xthetaphiInput > ub)){
            grad.fill(0);
            for(unsigned i = 0; i < xthetaphiInput.size(); i++){
                if(xthetaphiInput[i] < ub[i]){
                    grad[i] = 1;
                }
            }
            return 1E16;
        }
        if (xthetaphiInput.has_nan()){
            return 1E16;
        }

        const arma::vec & xthetaphi = xthetaphiInput;

        for (unsigned id = 0; id < missingComponentDim.size(); id++){
            xInit.col(missingComponentDim(id)) = xthetaphi.subvec(
                    xInit.n_rows * (id), xInit.n_rows * (id + 1) - 1);
        }

        thetaInit = xthetaphi.subvec(
                xInit.n_rows * missingComponentDim.size(), xInit.n_rows * missingComponentDim.size() + thetaInit.size() - 1);

        for (unsigned id = 0; id < missingComponentDim.size(); id++){
            phiAllDimensions.col(missingComponentDim(id)) = xthetaphi.subvec(
                    xInit.n_rows * missingComponentDim.size() + thetaInit.size() + phiAllDimensions.n_rows * id,
                    xInit.n_rows * missingComponentDim.size() + thetaInit.size() + phiAllDimensions.n_rows * (id + 1) - 1);
//            Rcpp::Rcout << "gradient: phiAllDimensions.col(missingComponentDim(id)) = " << phiAllDimensions.col(missingComponentDim(id)) << "\n";
        }

        lp out = xthetaphisigmallik( xInit,
                                     thetaInit,
                                     phiAllDimensions,
                                     sigmaAllDimensions,
                                     yobs,
                                     tvec,
                                     fOdeModel);
        if (out.gradient.has_nan() || isnan(out.value)){
            return 1E16;
        }
        out.gradient *= SCALE;
        out.value *= SCALE;

        for (unsigned id = 0; id < missingComponentDim.size(); id++){
            for (unsigned j = 0; j < xInit.n_rows; j++){
                grad[xInit.n_rows * (id) + j] = -out.gradient(xInit.n_rows * missingComponentDim(id) + j);
            }
        }
        for (unsigned j = 0; j < thetaInit.size(); j++){
            grad[xInit.n_rows * missingComponentDim.size() + j] = -out.gradient(xInit.size() + j);
        }
        for (unsigned id = 0; id < missingComponentDim.size(); id++){
            for (unsigned j = 0; j < phiAllDimensions.n_rows; j++){
                grad[xInit.n_rows * missingComponentDim.size() + thetaInit.size() + phiAllDimensions.n_rows * id + j] =
                        -out.gradient(xInit.size() + thetaInit.size() + phiAllDimensions.n_rows * missingComponentDim(id) + j);
            }
        }
//        Rcpp::Rcout << "after gradient assignment =\n" << grad.transpose();
        return -out.value;
    }

    double operator()(const arma::vec & x) override {
        arma::vec grad = arma::zeros(x.size());
        double value = this->operator()(x, grad);
        return value;
    }

    void Gradient(const arma::vec & x, arma::vec & grad) override {
        if (grad.size() != x.size()){
            grad = arma::zeros(x.size());
        }
        this->operator()(x, grad);
    }

    XmissingThetaPhiOptim(const arma::mat & yobsInput,
                          const arma::vec & tvecInput,
                          const OdeSystem & fOdeModelInput,
                          const arma::vec & sigmaAllDimensionsInput,
                          const arma::vec & priorTemperatureInput,
                          const arma::mat & xInitInput,
                          const arma::vec & thetaInitInput,
                          const arma::mat & phiFullInput,
                          const arma::uvec & missingComponentDimInput) :
            yobs(yobsInput),
            tvec(tvecInput),
            fOdeModel(fOdeModelInput),
            sigmaAllDimensions(sigmaAllDimensionsInput),
            priorTemperature(priorTemperatureInput),
            xInit(xInitInput),
            thetaInit(thetaInitInput),
            phiAllDimensions(phiFullInput),
            missingComponentDim(missingComponentDimInput) {
        lb = arma::vec(xInit.n_rows * missingComponentDim.size() + thetaInit.size() + phiAllDimensions.n_rows * missingComponentDim.size());
        lb.fill(-1E16);
        ub = arma::vec(xInit.n_rows * missingComponentDim.size() + thetaInit.size() + phiAllDimensions.n_rows * missingComponentDim.size());
        ub.fill(1E16);

        for (unsigned j = 0; j < thetaInit.size(); j++){
            lb[xInit.n_rows * missingComponentDim.size() + j] = fOdeModel.thetaLowerBound(j) + 1e-6;
            ub[xInit.n_rows * missingComponentDim.size() + j] = fOdeModel.thetaUpperBound(j) - 1e-6;
        }

        const double maxDist = (arma::max(tvecInput) - arma::min(tvecInput));
        const double minDist = arma::min(arma::abs(arma::diff(tvecInput)));
        const double maxScale = arma::max(arma::abs(yobs(arma::find_finite(yobs))));

        arma::vec priorFactor = arma::zeros(2);
        for (unsigned j = 0; j < yobs.n_cols; j++){
            if (arma::any(missingComponentDim == j)){
                continue;
            }
            const arma::vec & yobsThisDim = yobs.col(j);
            priorFactor += calcFrequencyBasedPrior(yobsThisDim(arma::find_finite(yobsThisDim)));
        }
        priorFactor /= (yobs.n_cols - missingComponentDim.size());
//        Rcpp::Rcout << "average priorFactor in PhiOptim =\n" << priorFactor << "\n";

        for(unsigned i = 0; i < missingComponentDim.size(); i++){
            ub[xInit.n_rows * missingComponentDim.size() + thetaInit.size() + 2*i] = maxScale * 5;
            lb[xInit.n_rows * missingComponentDim.size() + thetaInit.size() + 2*i] = maxScale * 1e-3;
            ub[xInit.n_rows * missingComponentDim.size() + thetaInit.size() + 2*i+1] = maxDist * 5;
            lb[xInit.n_rows * missingComponentDim.size() + thetaInit.size() + 2*i+1] = std::min(maxDist * priorFactor(0) * 0.5, minDist);
        }
//        Rcpp::Rcout << "finish set up of the problem\n";
//        Rcpp::Rcout << "ub = \n" << ub << "\nlb = \n" << lb << "\n";
    }
};

arma::mat optimizeXmissingThetaPhi(const arma::mat & yobsInput,
                                   const arma::vec & tvecInput,
                                   const OdeSystem & fOdeModelInput,
                                   const arma::vec & sigmaAllDimensionsInput,
                                   const arma::vec & priorTemperatureInput,
                                   const arma::mat & xInitInput,
                                   const arma::vec & thetaInitInput,
                                   const arma::mat & phiInitInput,
                                   const arma::uvec & missingComponentDim) {
    XmissingThetaPhiOptim objective(yobsInput, tvecInput, fOdeModelInput, sigmaAllDimensionsInput, priorTemperatureInput, xInitInput, thetaInitInput, phiInitInput, missingComponentDim);

    roptim::Roptim<XmissingThetaPhiOptim> opt("L-BFGS-B");
    opt.set_lower(objective.lb);
    opt.set_upper(objective.ub);

    arma::vec xThetaPhi(xInitInput.n_rows * missingComponentDim.size() + thetaInitInput.size() + phiInitInput.n_rows * missingComponentDim.size());

    for (unsigned id = 0; id < missingComponentDim.size(); id++){
        for (unsigned j = 0; j < xInitInput.n_rows; j++){
            xThetaPhi[xInitInput.n_rows * (id) + j] = xInitInput(j, missingComponentDim(id));
        }
    }
    for (unsigned j = 0; j < thetaInitInput.size(); j++){
        xThetaPhi[xInitInput.n_rows * missingComponentDim.size() + j] = thetaInitInput(j);
    }
    for (unsigned id = 0; id < missingComponentDim.size(); id++){
        for (unsigned j = 0; j < phiInitInput.n_rows; j++){
            xThetaPhi[xInitInput.n_rows * missingComponentDim.size() + thetaInitInput.size() + phiInitInput.n_rows * id + j] =
                    phiInitInput(j, missingComponentDim(id));
        }
    }

//    Rcpp::Rcout << "inside optimizeXmissingThetaPhi\n"
//    << "init xThetaPhi = " << xThetaPhi;
    arma::vec xThetaPhiInit = xThetaPhi;
    arma::vec grad = arma::vec(xThetaPhi.size());

    opt.minimize(objective, xThetaPhiInit);
    xThetaPhi = opt.par();

    if (! isfinite(objective(xThetaPhi, grad))){
        const arma::vec & xThetaPhiArgmin = arma::vec(xThetaPhiInit.begin(), xInitInput.n_rows * missingComponentDim.size() + thetaInitInput.size() + phiInitInput.n_rows * missingComponentDim.size(), true, false);
        return xThetaPhiArgmin;
    }

    if (objective(xThetaPhi, grad) < objective(xThetaPhiInit, grad)){
        const arma::vec & xThetaPhiArgmin = arma::vec(xThetaPhi.begin(), xInitInput.n_rows * missingComponentDim.size() + thetaInitInput.size() + phiInitInput.n_rows * missingComponentDim.size(), true, false);
        return xThetaPhiArgmin;
    }else{
        const arma::vec & xThetaPhiArgmin = arma::vec(xThetaPhiInit.begin(), xInitInput.n_rows * missingComponentDim.size() + thetaInitInput.size() + phiInitInput.n_rows * missingComponentDim.size(), true, false);
        return xThetaPhiArgmin;
    }
}
