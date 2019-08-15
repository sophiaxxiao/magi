#! /bin/bash

echo "
PKG_CXX=clang++
PKG_CXXFLAGS = -std=c++11 -O3 -DNDEBUG -Wall -I$PROJECT/include
PKG_LIBS = \$(LAPACK_LIBS) \$(BLAS_LIBS) \$(FLIBS)
" > src/Makevars

Rscript -e 'install.packages(c("devtools", "roxygen2"), repos="http://cran.us.r-project.org")'
Rscript -e 'devtools::install_deps(dependencies = TRUE, repos="http://cran.us.r-project.org")'

mkdir src/rcppgpds
rsync -avz ../gpds_cpp/*.cpp src/rcppgpds/
rsync -avz ../gpds_cpp/*.h src/rcppgpds/
perl -pi -e 's/\#include <armadillo>/\#include \"RcppArmadillo.h\"/g' src/rcppgpds/*.h
perl -pi -e 's/\#include <armadillo>/\#include \"RcppArmadillo.h\"/g' src/rcppgpds/*.cpp

perl -pi -e 's/std::cout/Rcpp::Rcout/g' src/rcppgpds/paralleltempering.cpp

rsync -avz src/rcppgpds/*.cpp src/
rsync -avz src/rcppgpds/*.h src/

rm -r src/rcppgpds/

Rscript -e "devtools::document();"
Rscript -e "pkgbuild:::compile_rcpp_attributes(); Rcpp::compileAttributes(); devtools::document(); devtools::install();"
