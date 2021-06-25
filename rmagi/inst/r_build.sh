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
mkdir -p inst/include/cppoptlib/solver
mkdir -p inst/include/cppoptlib/linesearch
rsync -az $PROJECT/include/cppoptlib/boundedproblem.h inst/include/cppoptlib/
rsync -az $PROJECT/include/cppoptlib/meta.h inst/include/cppoptlib/
rsync -az $PROJECT/include/cppoptlib/problem.h inst/include/cppoptlib/
rsync -az $PROJECT/include/cppoptlib/solver/isolver.h inst/include/cppoptlib/solver/
rsync -az $PROJECT/include/cppoptlib/solver/lbfgsbsolver.h inst/include/cppoptlib/solver/
rsync -az $PROJECT/include/cppoptlib/linesearch/morethuente.h inst/include/cppoptlib/linesearch/

perl -pi -e 's/\#define META_H/\#define META_H\n\#include <Rcpp.h>/g' inst/include/cppoptlib/meta.h
perl -pi -e 's/std::cout/Rcpp::Rcout/g' inst/include/cppoptlib/solver/*.h
perl -pi -e 's/std::cerr/Rcpp::Rcerr/g' inst/include/cppoptlib/solver/*.h
perl -pi -e 's/assert\((.*)\);/if(!(\1)) Rcpp::Rcerr << "!(\1\)\\n\";/g' inst/include/cppoptlib/solver/*.h
perl -pi -e 's/\#pragma GCC diagnostic ignored \"-Wunused-parameter\"//g' inst/include/cppoptlib/*.h

echo "
PKG_CXX=clang++
CXX_STD = CXX11
PKG_CXXFLAGS = -DNDEBUG -I'../inst/include'
PKG_LIBS = \$(LAPACK_LIBS) \$(BLAS_LIBS) \$(FLIBS)
MAKEFLAGS = -j$CPU
" > src/Makevars

MAKE="make -j$CPU" Rscript -e 'if (!require("devtools")) install.packages("devtools", repos="http://cran.us.r-project.org")'
MAKE="make -j$CPU" Rscript -e 'if (!require("roxygen2")) install.packages("roxygen2", repos="http://cran.us.r-project.org")'
MAKE="make -j$CPU" Rscript -e 'devtools::install_deps(dependencies = TRUE, repos="http://cran.us.r-project.org")'

find src/ -lname '*' -delete
mkdir src/rcppmagi
rsync -az ../cmagi/*.cpp src/rcppmagi/
rsync -az ../cmagi/*.h src/rcppmagi/
perl -pi -e 's/\#include <armadillo>/\#include \"RcppArmadillo.h\"/g' src/rcppmagi/*.h
perl -pi -e 's/\#include <armadillo>/\#include \"RcppArmadillo.h\"/g' src/rcppmagi/*.cpp
perl -pi -e 's/std::cerr/Rcpp::Rcerr/g' src/rcppmagi/*.cpp
perl -pi -e 's/std::cout/Rcpp::Rcout/g' src/rcppmagi/*.cpp

git checkout -- R/zzz.R
COMPILING_INFORMATION="Build Date - $(date); GIT branch - $(git rev-parse --abbrev-ref HEAD); GIT commit number - $(git log -1 --oneline)"
export COMPILING_INFORMATION
perl -pi -e 's/COMPILING_INFORMATION_HERE/$ENV{COMPILING_INFORMATION}/g' R/zzz.R

rsync -az src/rcppmagi/*.cpp src/
rsync -az src/rcppmagi/*.h src/

rm -r src/rcppmagi/

if [[ "$1" == "--cran" ]]; then
  rm src/RcppTestingUtilities.cpp
  rm src/testingUtilities.cpp
fi

rm NAMESPACE
echo "# Generated by roxygen2: do not edit by hand" >> NAMESPACE
MAKE="make -j$CPU" Rscript -e "Rcpp::compileAttributes(); devtools::document(); devtools::install();"

if [[ "$1" == "--cran" ]]; then
  mv examples inst/examples
else
  LIB_PYMAGI_SOURCE=$(cd "$PROJECT"/cmagi && ls -- *.cpp)
  LIB_PYMAGI_HEADERS=$(cd "$PROJECT"/cmagi && ls -- *.h)
  cd "$PROJECT"/rmagi/src && rm $LIB_PYMAGI_SOURCE $LIB_PYMAGI_HEADERS
  cd "$PROJECT"/rmagi || return
  rm -r inst/include/cppoptlib
  ln -s "$(pwd)"/../cmagi/*.cpp src/
  ln -s "$(pwd)"/../cmagi/*.h src/
  git checkout -- R/zzz.R
  git checkout -- src/RcppTestingUtilities.cpp
fi
