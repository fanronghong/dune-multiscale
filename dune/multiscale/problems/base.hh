// dune-multiscale
// Copyright Holders: Patrick Henning, Rene Milk
// License: BSD 2-Clause License (http://opensource.org/licenses/BSD-2-Clause)

#ifndef DUNE_MS_PROBLEMS_BASE_HH
#define DUNE_MS_PROBLEMS_BASE_HH

#include <dune/multiscale/common/traits.hh>
#include <dune/multiscale/msfem/msfem_traits.hh>
#include <dune/stuff/common/memory.hh>
#include <dune/stuff/fem/functions/analytical.hh>
#include <dune/stuff/functions/interfaces.hh>
#include <dune/stuff/functions/constant.hh>
#include <dune/stuff/grid/boundaryinfo.hh>
#include <memory>
#include <string>

namespace Dune {
namespace Multiscale {
namespace Problem {

typedef CommonTraits::DomainType DomainType;
typedef CommonTraits::RangeType RangeType;
typedef CommonTraits::JacobianRangeType JacobianRangeType;
//! type of first source term (right hand side of differential equation or type of 'f')
typedef CommonTraits::FunctionBaseType SourceType;
//! type of inhomogeneous Dirichlet boundary condition
typedef CommonTraits::FunctionBaseType DirichletBCType;
//! type of inhomogeneous Neumann boundary condition
typedef CommonTraits::FunctionBaseType NeumannBCType;
//! type of exact solution (in general unknown)
typedef CommonTraits::FunctionBaseType ExactSolutionType;

struct DiffusionBase : public CommonTraits::DiffusionFunctionBaseType {

  //! currently used in gdt assembler
  virtual void evaluate(const DomainType& x, CommonTraits::DiffusionFunctionBaseType::RangeType& y) const = 0;

  virtual ~DiffusionBase() {}

  //! in the linear setting, use the structure
  //! A^{\epsilon}_i(x,\xi) = A^{\epsilon}_{i1}(x) \xi_1 + A^{\epsilon}_{i2}(x) \xi_2
  //! (diffusive) flux = A^{\epsilon}( x , direction )
  //! (typically direction is some 'gradient_of_a_function')
  virtual void diffusiveFlux(const DomainType& x, const Problem::JacobianRangeType& direction,
                             Problem::JacobianRangeType& flux) const = 0;

  //! the jacobian matrix (JA^{\epsilon}) of the diffusion operator A^{\epsilon} at the position "\nabla v" in direction
  //! "nabla w", i.e.
  //! jacobian diffusiv flux = JA^{\epsilon}(\nabla v) nabla w:
  //! jacobianDiffusiveFlux = A^{\epsilon}( x , position_gradient ) direction_gradient
  virtual void jacobianDiffusiveFlux(const DomainType& x, const Problem::JacobianRangeType& /*position_gradient*/,
                                     const Problem::JacobianRangeType& direction_gradient,
                                     Problem::JacobianRangeType& flux) const;
  virtual size_t order() const { return 2; }
};

typedef DiffusionBase::Transfer<MsFEMTraits::LocalEntityType>::Type LocalDiffusionType;

class DirichletDataBase : public Dune::Multiscale::CommonTraits::FunctionBaseType {
public:
  virtual void evaluate(const DomainType& x, RangeType& y) const = 0;
  virtual size_t order() const { return 3; }
};

class ZeroDirichletData : public DirichletDataBase {
public:
  virtual void evaluate(const DomainType& /*x*/, RangeType& y) const DS_FINAL { y = RangeType(0.0); }
  virtual size_t order() const { return 0; }
};

class NeumannDataBase : public Dune::Multiscale::CommonTraits::FunctionBaseType {
public:
  virtual void evaluate(const DomainType& x, RangeType& y) const = 0;
  virtual size_t order() const { return 3; }
};

class ZeroNeumannData : public NeumannDataBase {
public:
  virtual void evaluate(const DomainType& /*x*/, RangeType& y) const DS_FINAL { y = RangeType(0.0); }
  virtual size_t order() const { return 0; }
};

/**
 * \addtogroup Problem
 * @{
 *
 * in general we regard problems of the following type:

 * - div ( A^{\epsilon} (x,\nabla u^{\epsilon}) ) + m^{\epsilon} u^{\epsilon} = f - div G

 * Here we have:
 * u^{\epsilon} = exact solution of the problem
 * A^{\epsilon} = diffusion matrix (or tensor), e.g. with the structure A^{\epsilon}(x) = A(x,\frac{x}{\epsilon})
 * m^{\epsilon} = a mass term (or reaction term), e.g. with the structure m^{\epsilon}(x) = m(x,\frac{x}{\epsilon})
 * f = first source term with the structure f = f(x) (=> no micro-scale dependency)
 * G = second source term with the structure G = G(x) (=> no micro-scale dependency).
 * (Note that 'G' is directly implemented! We do not implement '- div G'!)

 * A^{\epsilon} is can be a monotone operator (=> use HMM, the MsFEM is not implemented for nonlinear problems)


 * ! we use the following class names:


 * class ExactSolution -> describes u^{\epsilon}
 * methods:
 *   evaluate  u^{\epsilon}( x )        --> evaluate
 *   evaluate  \grad u^{\epsilon}( x )  --> jacobian


 * class Diffusion -> describes A^{\epsilon}
 * methods:
 *   evaluate A^{\epsilon}( x , direction )            --> diffusiveFlux
 *   evaluate DA^{\epsilon}( x , position ) direction  --> jacobianDiffusiveFlux


 * class MassTerm -> describes m^{\epsilon}
 * methods:
 *   evaluate m^{\epsilon}( x )         --> evaluate
 *

 * class FirstSource -> describes f
 * methods:
 *   evaluate f( x )                    --> evaluate


 * class SecondSource -> describes G
 * methods:
 *   evaluate G( x )                    --> evaluate


 ! See 'elliptic_problems/example.hh' for details


 * The mass (or reaction) term m^{\epsilon} is given by:
 * ! m^{\epsilon} := \epsilon
 * Since \epsilon tends to zero, we may say that we do not have a real mass term for our problem. It is a simple
 * condition to fix the solution which is only unique up to a constant. In fact we still approximate the solution of the
 * problem without mass.

 * The first source term f is given by:
 * ! f(x) := ****
 * since the second source is zero, f will form the right hand side (RHS) of our discrete problem

 * The second source term G is constantly zero:
 * ! G(x) := 0

 * !FirstSource defines the right hand side (RHS) of the governing problem (i.e. it defines 'f').
 * The value of the right hand side (i.e. the value of 'f') at 'x' is accessed by the method 'evaluate'. That means 'y
 * := f(x)' and 'y' is returned. It is only important that 'RHSFunction' knows the function space ('FuncSpace') that it
 * is part from. (f \in FunctionSpace)
 *
 *
**/
class IModelProblemData {
protected:
  typedef CommonTraits::GridViewType View;
  typedef DSG::BoundaryInfoInterface<typename View::Intersection> BoundaryInfoType;
  typedef MsFEMTraits::LocalGridType::LeafGridView SubView;
  typedef DSG::BoundaryInfoInterface<typename SubView::Intersection> SubBoundaryInfoType;

public:
  //! Constructor for ModelProblemData
  inline IModelProblemData() {}
  virtual ~IModelProblemData() {}

  /**
   * @brief getMacroGridFile returns a path to a Dune::Grid loadable file (dgf)
   * @return macroGridName is set to said path
   * \todo paths need to be relative to binary
   */
  virtual std::string getMacroGridFile() const = 0;

  //! does the problem implement an exact solution?
  virtual bool hasExactSolution() const { return false; }

  //! is the diffusion matrix symmetric?
  virtual bool symmetricDiffusion() const { return true; }

  //! linear/nonlinear toggle
  virtual bool linear() const { return true; }

  virtual const BoundaryInfoType& boundaryInfo() const = 0;

  virtual const SubBoundaryInfoType& subBoundaryInfo() const = 0;

  virtual std::pair<CommonTraits::DomainType, CommonTraits::DomainType> gridCorners() const {
    return {CommonTraits::DomainType(0.0), CommonTraits::DomainType(1.0)};
  }
};

} //! @} namespace Problem
} // namespace Multiscale {
} // namespace Dune {

namespace DMP = Dune::Multiscale::Problem;

#define MSCONSTANTFUNCTION(classname, constant)                                                                        \
  class classname : public Dune::Multiscale::CommonTraits::ConstantFunctionBaseType {                                  \
  public:                                                                                                              \
    classname()                                                                                                        \
      : Dune::Multiscale::CommonTraits::ConstantFunctionBaseType(constant) {}                                          \
  };

#define MSNULLFUNCTION(classname)                                                                                      \
  class classname : public Dune::Multiscale::CommonTraits::ConstantFunctionBaseType {                                  \
  public:                                                                                                              \
    classname()                                                                                                        \
      : Dune::Multiscale::CommonTraits::ConstantFunctionBaseType(0.0) {}                                               \
  };

#endif // DUNE_MS_PROBLEMS_BASE_HH
