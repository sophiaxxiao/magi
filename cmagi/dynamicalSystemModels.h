#ifndef DYNAMICALSYSTEMMODELS_H
#define DYNAMICALSYSTEMMODELS_H

#include "classDefinition.h"

arma::mat fnmodelODE(const arma::vec &, const arma::mat &, const arma::vec &);
arma::cube fnmodelDx(const arma::vec &, const arma::mat &, const arma::vec &);
arma::cube fnmodelDtheta(const arma::vec &, const arma::mat &, const arma::vec &);

arma::mat hes1modelODE(const arma::vec &, const arma::mat &, const arma::vec &);
arma::cube hes1modelDx(const arma::vec &, const arma::mat &, const arma::vec &);
arma::cube hes1modelDtheta(const arma::vec &, const arma::mat &, const arma::vec &);

arma::mat hes1logmodelODE(const arma::vec &, const arma::mat &, const arma::vec &);
arma::cube hes1logmodelDx(const arma::vec &, const arma::mat &, const arma::vec &);
arma::cube hes1logmodelDtheta(const arma::vec &, const arma::mat &, const arma::vec &);

arma::mat hes1logmodelODEfixg(const arma::vec &, const arma::mat &, const arma::vec &);
arma::cube hes1logmodelDxfixg(const arma::vec &, const arma::mat &, const arma::vec &);
arma::cube hes1logmodelDthetafixg(const arma::vec &, const arma::mat &, const arma::vec &);

arma::mat hes1logmodelODEfixf(const arma::vec &, const arma::mat &, const arma::vec &);
arma::cube hes1logmodelDxfixf(const arma::vec &, const arma::mat &, const arma::vec &);
arma::cube hes1logmodelDthetafixf(const arma::vec &, const arma::mat &, const arma::vec &);

arma::mat HIVmodelODE(const arma::vec &, const arma::mat &, const arma::vec &);
arma::cube HIVmodelDx(const arma::vec &, const arma::mat &, const arma::vec &);
arma::cube HIVmodelDtheta(const arma::vec &, const arma::mat &, const arma::vec &);

arma::mat ptransmodelODE(const arma::vec &, const arma::mat &, const arma::vec &);
arma::cube ptransmodelDx(const arma::vec &, const arma::mat &, const arma::vec &);
arma::cube ptransmodelDtheta(const arma::vec &, const arma::mat &, const arma::vec &);

arma::mat MichaelisMentenModelODE(const arma::vec &, const arma::mat &, const arma::vec &);
arma::cube MichaelisMentenModelDx(const arma::vec &, const arma::mat &, const arma::vec &);
arma::cube MichaelisMentenModelDtheta(const arma::vec &, const arma::mat &, const arma::vec &);


arma::mat MichaelisMentenLogModelODE(const arma::vec &, const arma::mat &, const arma::vec &);
arma::cube MichaelisMentenLogModelDx(const arma::vec &, const arma::mat &, const arma::vec &);
arma::cube MichaelisMentenLogModelDtheta(const arma::vec &, const arma::mat &, const arma::vec &);

arma::mat MichaelisMentenlog1xModelODE(const arma::vec &, const arma::mat &, const arma::vec &);
arma::cube MichaelisMentenlog1xModelDx(const arma::vec &, const arma::mat &, const arma::vec &);
arma::cube MichaelisMentenlog1xModelDtheta(const arma::vec &, const arma::mat &, const arma::vec &);

arma::mat MichaelisMentenModelVaODE(const arma::vec &, const arma::mat &, const arma::vec &);
arma::cube MichaelisMentenModelVaDx(const arma::vec &, const arma::mat &, const arma::vec &);
arma::cube MichaelisMentenModelVaDtheta(const arma::vec &, const arma::mat &, const arma::vec &);

arma::mat MichaelisMentenModelVb4pODE(const arma::vec &, const arma::mat &, const arma::vec &);
arma::cube MichaelisMentenModelVb4pDx(const arma::vec &, const arma::mat &, const arma::vec &);
arma::cube MichaelisMentenModelVb4pDtheta(const arma::vec &, const arma::mat &, const arma::vec &);

arma::mat MichaelisMentenModelVb2pODE(const arma::vec &, const arma::mat &, const arma::vec &);
arma::cube MichaelisMentenModelVb2pDx(const arma::vec &, const arma::mat &, const arma::vec &);
arma::cube MichaelisMentenModelVb2pDtheta(const arma::vec &, const arma::mat &, const arma::vec &);

arma::mat lacOperonODE(const arma::vec &, const arma::mat &, const arma::vec &);
arma::cube lacOperonDx(const arma::vec &, const arma::mat &, const arma::vec &);
arma::cube lacOperonDtheta(const arma::vec &, const arma::mat &, const arma::vec &);

arma::mat lacOperonLogODE(const arma::vec &, const arma::mat &, const arma::vec &);
arma::cube lacOperonLogDx(const arma::vec &, const arma::mat &, const arma::vec &);
arma::cube lacOperonLogDtheta(const arma::vec &, const arma::mat &, const arma::vec &);

arma::mat repressilatorGeneRegulationODE(const arma::vec &, const arma::mat &, const arma::vec &);
arma::cube repressilatorGeneRegulationDx(const arma::vec &, const arma::mat &, const arma::vec &);
arma::cube repressilatorGeneRegulationDtheta(const arma::vec &, const arma::mat &, const arma::vec &);

arma::mat repressilatorGeneRegulationLogODE(const arma::vec &, const arma::mat &, const arma::vec &);
arma::cube repressilatorGeneRegulationLogDx(const arma::vec &, const arma::mat &, const arma::vec &);
arma::cube repressilatorGeneRegulationLogDtheta(const arma::vec &, const arma::mat &, const arma::vec &);

#endif