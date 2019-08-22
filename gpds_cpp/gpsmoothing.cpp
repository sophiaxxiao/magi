#include <armadillo>
#include <cppoptlib/boundedproblem.h>
#include <cppoptlib/solver/lbfgsbsolver.h>
#include <cppoptlib/solver/bfgssolver.h>

#include "tgtdistr.h"


class PhiGaussianProcessSmoothing : public cppoptlib::BoundedProblem<double> {
public:
    std::string kernel;
    const arma::mat & yobs;
    const arma::mat & dist;
    const unsigned int numparam;

    double value(const Eigen::VectorXd & phisigInput) override {
        if ((phisigInput.array() < this->lowerBound().array()).any()){
            return INFINITY;
        }
        const arma::vec & phisig = arma::vec(const_cast<double*>(phisigInput.data()), numparam, false, false);
        const lp & out = phisigllik(phisig, yobs, dist, kernel);
        return -out.value;
    }

    void gradient(const Eigen::VectorXd & phisigInput, Eigen::VectorXd & grad) override {
        if ((phisigInput.array() < this->lowerBound().array()).any()){
            grad.fill(0);
            for(unsigned i = 0; i < numparam; i++){
                if(phisigInput[i] < this->lowerBound()[i]){
                    grad[i] = -1;
                }
            }
            return;
        }
        const arma::vec & phisig = arma::vec(const_cast<double*>(phisigInput.data()), numparam, false, false);
        const lp & out = phisigllik(phisig, yobs, dist, kernel);
        for(unsigned i = 0; i < numparam; i++){
            grad[i] = -out.gradient(i);
        }
    }

    PhiGaussianProcessSmoothing(const arma::mat & yobsInput,
                                const arma::mat & distInput,
                                std::string kernelInput,
                                const unsigned int numparamInput) :
            BoundedProblem(numparamInput),
            kernel(std::move(kernelInput)),
            yobs(yobsInput),
            dist(distInput),
            numparam(numparamInput) {
        Eigen::VectorXd lb(numparam);
        lb.fill(1e-4);
        this->setLowerBound(lb);
    }
};


// [[Rcpp::export]]
arma::vec gpsmooth(const arma::mat & yobsInput,
                   const arma::mat & distInput,
                   std::string kernelInput,
                   const unsigned int numparamInput) {
    PhiGaussianProcessSmoothing objective(yobsInput, distInput, std::move(kernelInput), numparamInput);
    cppoptlib::LbfgsbSolver<PhiGaussianProcessSmoothing> solver;
    Eigen::VectorXd phisig(2 * yobsInput.n_cols + 1);
    double maxDist = distInput.max();
    double sdOverall = 0;
    for(unsigned i = 0; i < yobsInput.n_cols; i++) {
        phisig[2 * i] = 0.5 * arma::stddev(yobsInput.col(i));
        phisig[2 * i + 1] = 0.5 * maxDist;
        sdOverall += phisig[2 * i];
    }
    phisig[2 * yobsInput.n_cols] = sdOverall / yobsInput.n_cols;
    solver.minimize(objective, phisig);
    const arma::vec & phisigArgmin = arma::vec(phisig.data(), 2 * yobsInput.n_cols + 1, false, false);
    return phisigArgmin;
}
