#include "Sampler.h"
#include "xthetasigma.h"
#include "hmc.h"


hmcstate Sampler::sampleSingle(const arma::vec &xthetasigmaInit, const arma::vec &step) {
    hmcstate post = basic_hmcC(tgt, xthetasigmaInit, step, lb, ub, nsteps, traj);
    return post;
}

void Sampler::sampleChian(const arma::vec &xthetasigmaInit, const arma::vec &stepLowInit, bool verbose=false) {
    lliklist.fill(arma::datum::nan);
    xth.fill(arma::datum::nan);
    stepLow = stepLowInit;
    arma::vec accepts = arma::vec(niter).fill(arma::datum::nan);
    accepts(0) = 0;
    xth.col(0) = xthetasigmaInit;
    const unsigned int burnin = static_cast<unsigned int>(niter * burninRatio);
    for (int t = 1; t < niter; t++){
        arma::vec stepRandom = arma::randu(stepLow.size());
        const arma::vec & rstep = stepRandom % stepLow + stepLow;
        hmcstate hmcpostsample = sampleSingle(xth.col(t-1), rstep);
        xth.col(t) = hmcpostsample.final;
        accepts(t) = static_cast<double>(hmcpostsample.acc);
        double acceptRate = arma::mean(accepts(arma::span(std::max(0, t - 99), t)));
        if (t < burnin && t > 10){
            if (acceptRate > 0.9){
                stepLow *= 1.005;
            }else if (acceptRate < 0.6){
                stepLow *= 0.995;
            }
            if (t % 100 == 0){
                arma::vec xthsd = arma::stddev(xth.cols(std::max(0, t - 99), t), 0, 1);
                if (arma::mean(xthsd) > 0){
                    stepLow = 0.05 * xthsd / arma::mean(xthsd) * arma::mean(stepLow) + 0.95 * stepLow;
                }
            }
        }
        lliklist(t) = hmcpostsample.lprvalue;

        if (verbose && (t % 100 == 1)){
            std::cout << "t = " << t << "; acceptance rate = " << acceptRate << "; theta = "
                      << xth.submat(arma::span(yobs.size(), yobs.size() + model.thetaSize - 1), arma::span(t, t)).t();
            std::cout << "t = " << t << "; stepRandom = " << stepRandom.subvec(0, 4).t();  // confirmed with seed setting
            std::cout << "t = " << t << "; hmcpostsample.lprvalue = " << hmcpostsample.lprvalue << "\n";
        }
    }
}

Sampler::Sampler(const arma::mat & yobsInput,
        const std::vector<gpcov> & covAllDimensionsInput,
        const int nstepsInput,
        const std::string loglikflagInput,
        const arma::vec priorTemperatureInput,
        const unsigned int sigmaSizeInput,
        const OdeSystem & modelInput,
        const unsigned int niterInput,
        const double burninRatioInput) :

        yobs(yobsInput),
        covAllDimensions(covAllDimensionsInput),
        nsteps(nstepsInput),
        traj(false),
        loglikflag(loglikflagInput),
        priorTemperature(priorTemperatureInput),
        model(modelInput),
        sigmaSize(sigmaSizeInput),
        burninRatio(burninRatioInput),
        niter(niterInput),
        lb(yobsInput.size() + modelInput.thetaSize + sigmaSizeInput),
        ub(yobsInput.size() + modelInput.thetaSize + sigmaSizeInput),
        lliklist(arma::vec(niterInput)),
        xth(arma::mat(yobsInput.size() + modelInput.thetaSize + sigmaSizeInput, niterInput).fill(arma::datum::nan))
{
    useBand = false;
    if(loglikflag == "band" || loglikflag == "withmeanBand"){
        useBand = true;
    }
    useMean = false;
    if(loglikflag == "withmean" || loglikflag == "withmeanBand"){
        useMean = true;
    }
    tgt = [&](const arma::vec & xthetasigma) -> lp{
        const arma::mat & xlatent = arma::mat(const_cast<double*>( xthetasigma.memptr()), yobs.n_rows, yobs.n_cols, false, false);
        const arma::vec & theta = arma::vec(const_cast<double*>( xthetasigma.memptr() + yobs.size()), model.thetaSize, false, false);
        const arma::vec & sigma = arma::vec(const_cast<double*>( xthetasigma.memptr() + yobs.size() + theta.size()), sigmaSize, false, false);
        return xthetasigmallik( xlatent,
                                theta,
                                sigma,
                                yobs,
                                covAllDimensions,
                                model,
                                priorTemperature,
                                useBand,
                                useMean);
    };
    lb.subvec(0, yobs.size()-1).fill(-arma::datum::inf);
    lb.subvec(yobs.size(), yobs.size() + model.thetaSize - 1) = model.thetaLowerBound;
    lb.subvec(yobs.size() + model.thetaSize, yobs.size() + model.thetaSize + sigmaSize - 1).fill(1e-7);
    ub.fill(arma::datum::inf);
    ub.subvec(yobs.size(), yobs.size() + model.thetaSize - 1) = model.thetaUpperBound;
}
