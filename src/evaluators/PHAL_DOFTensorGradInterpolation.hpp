//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef PHAL_DOFTENSORGRAD_INTERPOLATION_HPP
#define PHAL_DOFTENSORGRAD_INTERPOLATION_HPP

#include "Phalanx_config.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"

#include "Albany_Layouts.hpp"

namespace PHAL {
/** \brief Finite Element Interpolation Evaluator

    This evaluator interpolates nodal DOFTensor values to their
    gradients at quad points.

*/

template<typename EvalT, typename Traits>
class DOFTensorGradInterpolation : public PHX::EvaluatorWithBaseImpl<Traits>,
 			 public PHX::EvaluatorDerived<EvalT, Traits>  {

public:

  DOFTensorGradInterpolation(const Teuchos::ParameterList& p,
                              const Teuchos::RCP<Albany::Layouts>& dl);

  void postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& vm);

  void evaluateFields(typename Traits::EvalData d);

private:

  typedef typename EvalT::ScalarT ScalarT;
  typedef typename EvalT::MeshScalarT MeshScalarT;


  // Input:
  //! Values at nodes
  PHX::MDField<ScalarT,Cell,Node,VecDim,VecDim> val_node;
  //! Basis Functions
  PHX::MDField<MeshScalarT,Cell,Node,QuadPoint,Dim> GradBF;

  // Output:
  //! Values at quadrature points
  PHX::MDField<ScalarT,Cell,QuadPoint,VecDim,VecDim,Dim> grad_val_qp;

  std::size_t numNodes;
  std::size_t numQPs;
  std::size_t numDims;
  std::size_t vecDim;

};

//! Specialization for Jacobian evaluation taking advantage of known sparsity
template<typename Traits>
class DOFTensorGradInterpolation<PHAL::AlbanyTraits::Jacobian, Traits>\
      : public PHX::EvaluatorWithBaseImpl<Traits>,
 	public PHX::EvaluatorDerived<PHAL::AlbanyTraits::Jacobian, Traits>  {

public:

  DOFTensorGradInterpolation(const Teuchos::ParameterList& p,
                              const Teuchos::RCP<Albany::Layouts>& dl);

  void postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& vm);

  void evaluateFields(typename Traits::EvalData d);

private:

  typedef PHAL::AlbanyTraits::Jacobian::ScalarT ScalarT;
  typedef PHAL::AlbanyTraits::Jacobian::MeshScalarT MeshScalarT;


  // Input:
  //! Values at nodes
  PHX::MDField<ScalarT,Cell,Node,VecDim,VecDim> val_node;
  //! Basis Functions
  PHX::MDField<MeshScalarT,Cell,Node,QuadPoint,Dim> GradBF;

  // Output:
  //! Values at quadrature points
  PHX::MDField<ScalarT,Cell,QuadPoint,VecDim,VecDim,Dim> grad_val_qp;

  std::size_t numNodes;
  std::size_t numQPs;
  std::size_t numDims;
  std::size_t vecDim;
  std::size_t offset;
};

#ifdef ALBANY_SG
//! Specialization for SGJacobian evaluation taking advantage of known sparsity
template<typename Traits>
class DOFTensorGradInterpolation<PHAL::AlbanyTraits::SGJacobian, Traits>\
      : public PHX::EvaluatorWithBaseImpl<Traits>,
 	public PHX::EvaluatorDerived<PHAL::AlbanyTraits::SGJacobian, Traits>  {

public:

  DOFTensorGradInterpolation(const Teuchos::ParameterList& p,
                              const Teuchos::RCP<Albany::Layouts>& dl);

  void postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& vm);

  void evaluateFields(typename Traits::EvalData d);

private:

  typedef PHAL::AlbanyTraits::SGJacobian::ScalarT ScalarT;
  typedef PHAL::AlbanyTraits::SGJacobian::MeshScalarT MeshScalarT;


  // Input:
  //! Values at nodes
  PHX::MDField<ScalarT,Cell,Node,VecDim,VecDim> val_node;
  //! Basis Functions
  PHX::MDField<MeshScalarT,Cell,Node,QuadPoint,Dim> GradBF;

  // Output:
  //! Values at quadrature points
  PHX::MDField<ScalarT,Cell,QuadPoint,VecDim,VecDim,Dim> grad_val_qp;

  std::size_t numNodes;
  std::size_t numQPs;
  std::size_t numDims;
  std::size_t vecDim;
  std::size_t offset;
};
#endif

#ifdef ALBANY_ENSEMBLE
//! Specialization for MPJacobian evaluation taking advantage of known sparsity
template<typename Traits>
class DOFTensorGradInterpolation<PHAL::AlbanyTraits::MPJacobian, Traits>\
      : public PHX::EvaluatorWithBaseImpl<Traits>,
 	public PHX::EvaluatorDerived<PHAL::AlbanyTraits::MPJacobian, Traits>  {

public:

  DOFTensorGradInterpolation(const Teuchos::ParameterList& p,
                              const Teuchos::RCP<Albany::Layouts>& dl);

  void postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& vm);

  void evaluateFields(typename Traits::EvalData d);

private:

  typedef PHAL::AlbanyTraits::MPJacobian::ScalarT ScalarT;
  typedef PHAL::AlbanyTraits::MPJacobian::MeshScalarT MeshScalarT;


  // Input:
  //! Values at nodes
  PHX::MDField<ScalarT,Cell,Node,VecDim,VecDim> val_node;
  //! Basis Functions
  PHX::MDField<MeshScalarT,Cell,Node,QuadPoint,Dim> GradBF;

  // Output:
  //! Values at quadrature points
  PHX::MDField<ScalarT,Cell,QuadPoint,VecDim,VecDim,Dim> grad_val_qp;

  std::size_t numNodes;
  std::size_t numQPs;
  std::size_t numDims;
  std::size_t vecDim;
  std::size_t offset;
};
#endif

}

#endif