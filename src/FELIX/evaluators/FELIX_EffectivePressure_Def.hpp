//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include "Phalanx_DataLayout.hpp"
#include "Phalanx_TypeStrings.hpp"
#include "Teuchos_VerboseObject.hpp"

//uncomment the following line if you want debug output to be printed to screen
#define OUTPUT_TO_SCREEN

namespace FELIX {

template<typename EvalT, typename Traits, bool OnSide, bool Surrogate>
EffectivePressure<EvalT, Traits, OnSide, Surrogate>::
EffectivePressure (const Teuchos::ParameterList& p,
                   const Teuchos::RCP<Albany::Layouts>& dl) :
  H (p.get<std::string> ("Ice Thickness Variable Name"), dl->node_scalar),
  N (p.get<std::string> ("Effective Pressure Variable Name"), dl->node_scalar)
{
  regularized = false;

  numNodes = dl->node_scalar->dimension(1); // If OnSide=true, it will be fixed

  if (OnSide)
  {
    TEUCHOS_TEST_FOR_EXCEPTION (!dl->isSideLayouts, Teuchos::Exceptions::InvalidParameter,
                                "Error! The layout structure does not appear to be that of a side set.\n");

    basalSideName = p.get<std::string>("Side Set Name");
    numNodes = dl->node_scalar->dimension(2);
  }

  if (Surrogate)
  {
    alphaParam = PHX::MDField<const ScalarT,Dim> ("Hydraulic-Over-Hydrostatic Potential Ratio",dl->shared_param);
    this->addDependentField (alphaParam);

    Teuchos::ParameterList& plist = *p.get<Teuchos::ParameterList*>("Parameter List");

    regularized = plist.get("Regularize With Continuation",false);
    printedAlpha = -1.0;

    if (regularized)
      regularizationParam = PHX::MDField<const ScalarT,Dim>(plist.get<std::string>("Regularization Parameter Name"),dl->shared_param);
  }
  else
  {
    z_s  = PHX::MDField<const ParamScalarT>(p.get<std::string> ("Surface Height Variable Name"), dl->node_scalar);
    phi  = PHX::MDField<const HydroScalarT>(p.get<std::string> ("Hydraulic Potential Variable Name"), dl->node_scalar);

    this->addDependentField (phi);
    this->addDependentField (z_s);

    Teuchos::ParameterList& hydro_params = *p.get<Teuchos::ParameterList*>("FELIX Hydrology");

    use_h = false;
    if (hydro_params.get<bool>("Use Water Thickness In Effective Pressure Formula",false)) {
      use_h = true;

      h = PHX::MDField<const HydroScalarT>(p.get<std::string> ("Water Thickness Variable Name"), dl->node_scalar);
      this->addDependentField(h);
    }
  }

  this->addDependentField (H);
  this->addEvaluatedField (N);

  // Setting parameters
  Teuchos::ParameterList& physics  = *p.get<Teuchos::ParameterList*>("FELIX Physical Parameters");

  rho_i = physics.get<double>("Ice Density",910);
  rho_w = physics.get<double>("Water Density",1000);
  g     = physics.get<double>("Gravity Acceleration",9.8);

  this->setName("EffectivePressure"+PHX::typeAsString<EvalT>());
}

//**********************************************************************
template<typename EvalT, typename Traits, bool OnSide, bool Surrogate>
void EffectivePressure<EvalT, Traits, OnSide, Surrogate>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(H,fm);
  this->utils.setFieldData(N,fm);

  if (Surrogate)
  {
    this->utils.setFieldData(alphaParam,fm);
    if (regularized)
      this->utils.setFieldData(regularizationParam,fm);
  }
  else
  {
    this->utils.setFieldData(z_s,fm);
    this->utils.setFieldData(phi,fm);
    if (use_h) {
      this->utils.setFieldData(h,fm);
    }
  }
}

//**********************************************************************
template<typename EvalT, typename Traits, bool OnSide, bool Surrogate>
void EffectivePressure<EvalT, Traits, OnSide, Surrogate>::
evaluateFields (typename Traits::EvalData workset)
{
  if (OnSide)
    evaluateFieldsSide(workset);
  else
    evaluateFieldsCell(workset);
}

template<typename EvalT, typename Traits, bool OnSide, bool Surrogate>
void EffectivePressure<EvalT, Traits, OnSide, Surrogate>::
evaluateFieldsSide (typename Traits::EvalData workset)
{
  const Albany::SideSetList& ssList = *(workset.sideSets);
  Albany::SideSetList::const_iterator it_ss = ssList.find(basalSideName);

  if (it_ss==ssList.end())
    return;

  const std::vector<Albany::SideStruct>& sideSet = it_ss->second;
  std::vector<Albany::SideStruct>::const_iterator iter_s;

  if (Surrogate)
  {
    ParamScalarT alpha = Albany::convertScalar<ScalarT,ParamScalarT>(alphaParam(0));
    if (regularized)
    {
      alpha = alpha*std::sqrt(Albany::convertScalar<ScalarT,ParamScalarT>(regularizationParam(0)));
    }

#ifdef OUTPUT_TO_SCREEN
    Teuchos::RCP<Teuchos::FancyOStream> output(Teuchos::VerboseObjectBase::getDefaultOStream());
    if (std::fabs(printedAlpha-alpha)>0.0001)
    {
      *output << "[Effective Pressure<" << PHX::typeAsString<EvalT>() << ">]] alpha = " << alpha << "\n";
      printedAlpha = alpha;
    }
#endif

    for (iter_s=sideSet.begin(); iter_s!=sideSet.end(); ++iter_s)
    {
      // Get the local data of side and cell
      const int cell = iter_s->elem_LID;
      const int side = iter_s->side_local_id;

      for (int node=0; node<numNodes; ++node)
      {
        // N = p_i-p_w
        // p_i = rho_i*g*H
        // p_w = alpha*p_i
        N (cell,side,node) = (1-alpha)*rho_i*g*H(cell,side,node);
      }
    }
  }
  else
  {
    for (const auto& it : sideSet)
    {
      // Get the local data of side and cell
      const int cell = it.elem_LID;
      const int side = it.side_local_id;

      for (int node=0; node<numNodes; ++node)
      {
        // N = p_i-p_w
        // p_i = rho_i*g*H
        // p_w = rho_w*g*z_b - phi
        N (cell,side,node) = std::max(rho_i*g*H(cell,side,node) + rho_w*g*(z_s(cell,side,node) - H(cell,side,node)) - phi (cell,side,node),0.0);
      }
    }
  }
}

template<typename EvalT, typename Traits, bool OnSide, bool Surrogate>
void EffectivePressure<EvalT, Traits, OnSide, Surrogate>::
evaluateFieldsCell (typename Traits::EvalData workset)
{
  if (Surrogate)
  {
    ParamScalarT alpha = Albany::convertScalar<ScalarT,ParamScalarT>(alphaParam(0));
    if (regularized)
    {
      alpha = alpha*std::sqrt(Albany::convertScalar<ScalarT,ParamScalarT>(regularizationParam(0)));
    }

#ifdef OUTPUT_TO_SCREEN
    Teuchos::RCP<Teuchos::FancyOStream> output(Teuchos::VerboseObjectBase::getDefaultOStream());
    if (std::fabs(printedAlpha-alpha)>0.0001)
    {
      *output << "[Effective Pressure " << PHX::typeAsString<EvalT>() << "] alpha = " << alpha << "\n";
      printedAlpha = alpha;
    }
#endif

    for (int cell=0; cell<workset.numCells; ++cell)
    {
      for (int node=0; node<numNodes; ++node)
      {
        // N = p_i-p_w
        // p_i = rho_i*g*H
        // p_w = rho_w*g*z_b - phi
        N (cell,node) = (1-alpha)*rho_i*g*H(cell,node);
      }
    }
  }
  else
  {
    HydroScalarT zero (0.0);
    for (int cell=0; cell<workset.numCells; ++cell)
    {
      for (int node=0; node<numNodes; ++node)
      {
        // N = p_i-p_w;
        // p_i = rho_i*g*H;
        // p_w = phi - rho_w*g*(z_b+h);   (recall, z_b is in km, h in m)
        N(cell,node) = rho_i*g*H(cell,node) + rho_w*g*(z_s(cell,node) - H(cell,node)) + (rho_w*g/1000)*(use_h ? h(cell,node) : zero) - phi(cell,node);
      }
    }
  }
}

} // Namespace FELIX
