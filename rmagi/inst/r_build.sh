#! /bin/bash
# if the following commands error, cause the script to error
set -e

if [[ -z "${R_LIBS_USER}" ]]; then
    export R_LIBS_USER=$HOME/R/library
fi

if [[ -z "${PROJECT}" ]]; then
    PROJECT="$(git rev-parse --show-toplevel)"
    export PROJECT
fi

if [[ -z "${CPU}" ]]; then
    export CPU=$(python3 -c 'import multiprocessing as mp; print(mp.cpu_count())')
fi

cd "$PROJECT"/rmagi || exit 1

# only select the mininum sufficient third party library for compile

echo "
PKG_CXX=clang++
CXX_STD = CXX11
PKG_CXXFLAGS = -DNDEBUG -I'../inst/include'
PKG_LIBS = \$(LAPACK_LIBS) \$(BLAS_LIBS) \$(FLIBS)
" > src/Makevars

MAKE="make -j$CPU" Rscript -e 'if (!require("devtools")) install.packages("devtools", repos="http://cran.us.r-project.org")'
MAKE="make -j$CPU" Rscript -e 'if (!require("roxygen2")) install.packages("roxygen2", repos="http://cran.us.r-project.org")'
MAKE="make -j$CPU" Rscript -e 'devtools::install_deps(dependencies = TRUE, repos="http://cran.us.r-project.org", upgrade=FALSE)'

find src/ -lname '*' -delete
mkdir src/rcppmagi

cd ../cmagi/
rsync -az band.* classDefinition.* dynamicalSystemModels.* fullloglikelihood.* gpcov.* hmc.* MagiMain.* MagiSolver.* Sampler.* xthetasigma.* ../rmagi/src/rcppmagi/

# no need ot parallel tempering, testing utilities, and other types of GP kernels
#keep only testing utilities, remove parallel tempering, other types of GP kernels, and phi1
rsync -az testingUtilities.* ../rmagi/src/rcppmagi/
rsync -az tgtdistr.* ../rmagi/src/rcppmagi/
rsync -az paralleltempering.* ../rmagi/src/rcppmagi/
rsync -az phi1loglikelihood.* ../rmagi/src/rcppmagi/
rsync -az gpsmoothing.h ../rmagi/src/rcppmagi/

cd ../rmagi/

perl -pi -e 's/\#include <armadillo>/\#include \"RcppArmadillo.h\"/g' src/rcppmagi/*.h
perl -pi -e 's/\#include <armadillo>/\#include \"RcppArmadillo.h\"/g' src/rcppmagi/*.cpp
perl -pi -e 's/\#include <Eigen.*>/\#include \"RcppEigen.h\"/g' src/rcppmagi/*.h
perl -pi -e 's/\#include <Eigen.*>/\#include \"RcppEigen.h\"/g' src/rcppmagi/*.cpp
perl -pi -e 's/std::cerr/Rcpp::Rcerr/g' src/rcppmagi/*.cpp
perl -pi -e 's/std::cout/Rcpp::Rcout/g' src/rcppmagi/*.cpp

git checkout -- R/zzz.R
COMPILING_INFORMATION=""
if [[ "$1" != "--cran" ]]; then
  COMPILING_INFORMATION="Build Date - $(date); GIT branch - $(git rev-parse --abbrev-ref HEAD); GIT commit number - $(git log -1 --oneline)"
fi

export COMPILING_INFORMATION
perl -pi -e 's/COMPILING_INFORMATION_HERE/$ENV{COMPILING_INFORMATION}/g' R/zzz.R

rsync -az src/rcppmagi/*.cpp src/
rsync -az src/rcppmagi/*.h src/

rm -r src/rcppmagi/

if [[ "$1" != "--cran" ]]; then
  echo "MAKEFLAGS = -j$CPU" >> src/Makevars
fi

rm NAMESPACE
echo "# Generated by roxygen2: do not edit by hand" >> NAMESPACE
MAKE="make -j$CPU" Rscript -e "Rcpp::compileAttributes(); devtools::document(); devtools::install(upgrade=FALSE);"

export NOT_CRAN=TRUE
mv examples inst/examples
if [[ "$1" == "--cran" ]]; then
  R -e 'Sys.getenv("NOT_CRAN"); devtools::build(vignettes=TRUE)'
else
  R -e 'Sys.getenv("NOT_CRAN"); devtools::build(vignettes=FALSE)'
fi
mv inst/examples examples

LIB_PYMAGI_SOURCE=$(cd "$PROJECT"/cmagi && ls -- *.cpp | grep -v "gpsmoothing\.cpp")
LIB_PYMAGI_HEADERS=$(cd "$PROJECT"/cmagi && ls -- *.h)
cd "$PROJECT"/rmagi/src && rm $LIB_PYMAGI_SOURCE $LIB_PYMAGI_HEADERS
cd "$PROJECT"/rmagi || return
rm -r inst/include/
ln -s "$(pwd)"/../cmagi/*.cpp src/
ln -s "$(pwd)"/../cmagi/*.h src/
git checkout -- R/zzz.R
git checkout -- inst/include/magi.h
