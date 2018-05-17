//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include "Phalanx_DataLayout.hpp"
#include "Phalanx_TypeStrings.hpp"
#include "Teuchos_VerboseObject.hpp"

namespace FELIX
{

template<typename EvalT, typename Traits, bool IsStokes>
HydrologyWaterDischarge<EvalT, Traits, IsStokes>::
HydrologyWaterDischarge (const Teuchos::ParameterList& p,
                         const Teuchos::RCP<Albany::Layouts>& dl) :
  gradPhi (p.get<std::string> ("Hydraulic Potential Gradient Variable Name"), dl->qp_gradient),
  h       (p.get<std::string> ("Water Thickness Variable Name"), dl->qp_scalar),
  q       (p.get<std::string> ("Water Discharge Variable Name"), dl->qp_gradient)
{

  if (IsStokes) {
    TEUCHOS_TEST_FOR_EXCEPTION (!dl->isSideLayouts, std::logic_error,
                                "Error! For coupling with StokesFO, the Layouts structure must be that of the basal side.\n");

    sideSetName = p.get<std::string>("Side Set Name");

    numQPs  = dl->qp_gradient->dimension(2);
    numDim  = dl->qp_gradient->dimension(3);
  } else {
    numQPs  = dl->qp_gradient->dimension(1);
    numDim  = dl->qp_gradient->dimension(2);
  }

  this->addDependentField(gradPhi);
  this->addDependentField(h);

  this->addEvaluatedField(q);

  // Setting parameters
  Teuchos::ParameterList& hydrology = *p.get<Teuchos::ParameterList*>("FELIX Hydrology");

  k_0   = hydrology.get<double>("Transmissivity");
  alpha = hydrology.get<double>("Darcy Law: Water Thickness Exponent");
  beta  = hydrology.get<double>("Darcy Law: Potential Gradient Norm Exponent");

  TEUCHOS_TEST_FOR_EXCEPTION (beta<=1, Teuchos::Exceptions::InvalidParameter, "Error! 'Darcy Law: Potential Gradient Norm Exponent' must be larger than 1.0.\n");

  if (beta==2.0) {
    needsGradPhiNorm = false;
  } else {
    needsGradPhiNorm = true;
  }

  if (needsGradPhiNorm) {
    gradPhiNorm = decltype(gradPhiNorm)(p.get<std::string>("Hydraulic Potential Gradient Norm Variable Name"), dl->qp_scalar);
    this->addDependentField(gradPhiNorm);
  }

  regularize = hydrology.get<bool>("Regularize With Continuation", false);
  if (regularize)
  {
    regularizationParam = PHX::MDField<ScalarT,Dim>(p.get<std::string>("Regularization Parameter Name"),dl->shared_param);
    this->addDependentField(regularizationParam);
  }

  this->setName("HydrologyWaterDischarge"+PHX::typeAsString<EvalT>());
}

//**********************************************************************
template<typename EvalT, typename Traits, bool IsStokes>
void HydrologyWaterDischarge<EvalT, Traits, IsStokes>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(gradPhi,fm);
  this->utils.setFieldData(h,fm);
  if (regularize)
  {
    this->utils.setFieldData(regularizationParam,fm);
  }

  this->utils.setFieldData(q,fm);
}

//**********************************************************************
template<typename EvalT, typename Traits, bool IsStokes>
void HydrologyWaterDischarge<EvalT, Traits, IsStokes>::evaluateFields (typename Traits::EvalData workset)
{
  if (IsStokes) {
    evaluateFieldsSide(workset);
  } else {
    evaluateFieldsCell(workset);
  }
}

template<typename EvalT, typename Traits, bool IsStokes>
void HydrologyWaterDischarge<EvalT, Traits, IsStokes>::evaluateFieldsCell (typename Traits::EvalData workset)
{
  ScalarT regularization(0.0);
  if (regularize)
  {
    regularization = regularizationParam(0);
  }
  Teuchos::RCP<Teuchos::FancyOStream> output(Teuchos::VerboseObjectBase::getDefaultOStream());
  int procRank = Teuchos::GlobalMPISession::getRank();
  int numProcs = Teuchos::GlobalMPISession::getNProc();
  output->setProcRankAndSize (procRank, numProcs);
  output->setOutputToRootOnly (0);

  static ScalarT printedReg = -1;
  if (printedReg!=regularization)
  {
    *output << "reg = " << regularization << "\n";
    printedReg = regularization;
  }

  if (needsGradPhiNorm) {
    double grad_norm_exponent = beta - 2.0;
    for (int cell=0; cell < workset.numCells; ++cell) {
      for (int qp=0; qp < numQPs; ++qp) {
        for (int dim(0); dim<numDim; ++dim) {
          q(cell,qp,dim) = -k_0 * (std::pow(h(cell,qp),alpha)+regularization)
                                * std::pow(gradPhiNorm(cell,qp),grad_norm_exponent)
                                * gradPhi(cell,qp,dim);
        }
      }
    }
  } else {
    for (int cell=0; cell < workset.numCells; ++cell) {
      for (int qp=0; qp < numQPs; ++qp) {
        for (int dim(0); dim<numDim; ++dim) {
          q(cell,qp,dim) = -k_0 * (std::pow(h(cell,qp),alpha)+regularization) * gradPhi(cell,qp,dim);
        }
      }
    }
  }
}

template<typename EvalT, typename Traits, bool IsStokes>
void HydrologyWaterDischarge<EvalT, Traits, IsStokes>::
evaluateFieldsSide (typename Traits::EvalData workset)
{
  if (workset.sideSets->find(sideSetName)==workset.sideSets->end())
    return;

  ScalarT regularization(0.0);
  if (regularize)
  {
    regularization = regularizationParam(0);
  }

  Teuchos::RCP<Teuchos::FancyOStream> output(Teuchos::VerboseObjectBase::getDefaultOStream());
  int procRank = Teuchos::GlobalMPISession::getRank();
  int numProcs = Teuchos::GlobalMPISession::getNProc();
  output->setProcRankAndSize (procRank, numProcs);
  output->setOutputToRootOnly (0);
  static ScalarT printedReg = -1;
  if (printedReg!=regularization)
  {
    *output << "[HydrologyWaterDischarge<" << PHX::typeAsString<EvalT>() << ">] reg = " << regularization << "\n";
    printedReg = regularization;
  }

  const std::vector<Albany::SideStruct>& sideSet = workset.sideSets->at(sideSetName);
  for (auto const& it_side : sideSet)
  {
    // Get the local data of side and cell
    const int cell = it_side.elem_LID;
    const int side = it_side.side_local_id;

    if (needsGradPhiNorm) {
      double grad_norm_exponent = beta - 2.0;
      for (int qp=0; qp < numQPs; ++qp) {
        for (int dim(0); dim<numDim; ++dim) {
          q(cell,side,qp,dim) = -k_0 * (std::pow(h(cell,side,qp),alpha)+regularization)
                                     * std::pow(gradPhiNorm(cell,side,qp),grad_norm_exponent)
                                     * gradPhi(cell,side,qp,dim);
        }
      }
    } else {
      for (int qp=0; qp < numQPs; ++qp) {
        for (int dim(0); dim<numDim; ++dim) {
          q(cell,side,qp,dim) = -k_0 * (std::pow(h(cell,side,qp),alpha)+regularization) * gradPhi(cell,side,qp,dim);
        }
      }
    }
  }
}

} // Namespace FELIX
