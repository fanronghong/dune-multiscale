#include <config.h>
// dune-multiscale
// Copyright Holders: Patrick Henning, Rene Milk
// License: BSD 2-Clause License (http://opensource.org/licenses/BSD-2-Clause)

#include "localoperator.hh"

#include <assert.h>
#include <boost/assert.hpp>
#include <dune/common/exceptions.hh>
#include <dune/multiscale/problems/selector.hh>
#include <dune/stuff/common/parameter/configcontainer.hh>
#include <dune/stuff/fem/localmatrix_proxy.hh>
#include <dune/stuff/discretefunction/projection/heterogenous.hh>
#include <dune/stuff/fem/functions/integrals.hh>
#include <dune/multiscale/tools/misc.hh>
#include <dune/multiscale/msfem/localproblems/localproblemsolver.hh>
#include <dune/common/fmatrix.hh>
#include <dune/multiscale/common/traits.hh>
#include <dune/multiscale/msfem/localproblems/localproblemsolver.hh>
#include <dune/stuff/common/filesystem.hh>
#include <dune/stuff/fem/functions/checks.hh>
#include <dune/gdt/operators/projections.hh>
#include <dune/gdt/operators/prolongations.hh>
#include <dune/gdt/spaces/constraints.hh>
#include <dune/gdt/functionals/l2.hh>
#include <dune/stuff/common/exceptions.hh>

namespace Dune {

// forward, to be used in the traits
template< class LocalizableFunctionImp >
class CoarseBasisProduct;


/**
 *  \brief Traits for the Product evaluation.
 */
template< class LocalizableFunctionImp >
class CoarseBasisProductTraits
{
public:
  typedef CoarseBasisProduct< LocalizableFunctionImp > derived_type;
  typedef LocalizableFunctionImp            LocalizableFunctionType;
  static_assert(std::is_base_of< Dune::Stuff::IsLocalizableFunction, LocalizableFunctionImp >::value,
                "LocalizableFunctionImp has to be derived from Stuff::IsLocalizableFunction.");
};


template< class LocalizableFunctionImp >
class CoarseBasisProduct
  : public GDT::LocalEvaluation::Codim0Interface< CoarseBasisProductTraits< LocalizableFunctionImp >, 1 >
{
public:
  typedef CoarseBasisProductTraits< LocalizableFunctionImp >   Traits;
  typedef typename Traits::LocalizableFunctionType  LocalizableFunctionType;

  CoarseBasisProduct(const LocalizableFunctionType& inducingFunction)
    : inducingFunction_(inducingFunction)
  {}

  template< class EntityType >
  class LocalfunctionTuple
  {
    typedef typename LocalizableFunctionType::LocalfunctionType LocalfunctionType;
  public:
    typedef std::tuple< std::shared_ptr< LocalfunctionType > > Type;
  };

  template< class EntityType >
  typename LocalfunctionTuple< EntityType >::Type localFunctions(const EntityType& entity) const
  {
    return std::make_tuple(inducingFunction_.local_function(entity));
  }

  /**
   * \brief extracts the local functions and calls the correct order() method
   */
  template< class E, class D, int d, class R, int rT, int rCT >
  size_t order(const typename LocalfunctionTuple< E >::Type& localFuncs,
               const Stuff::LocalfunctionSetInterface< E, D, d, R, rT, rCT >& testBase) const
  {
    const auto localFunction = std::get< 0 >(localFuncs);
    return order(*localFunction, testBase);
  }

  /**
   *  \todo add copydoc
   *  \return localFunction.order() + testBase.order()
   */
  template< class E, class D, int d, class R, int rL, int rCL, int rT, int rCT >
  size_t order(const Stuff::LocalfunctionInterface< E, D, d, R, rL, rCL >& localFunction,
               const Stuff::LocalfunctionSetInterface< E, D, d, R, rT, rCT >& testBase) const
  {
    return localFunction.order() + testBase.order();
  } // int order(...)

  /**
   * \brief extracts the local functions and calls the correct evaluate() method
   */
  template< class E, class D, int d, class R, int rT, int rCT >
   void evaluate(const typename LocalfunctionTuple< E >::Type& localFuncs,
                 const Stuff::LocalfunctionSetInterface< E, D, d, R, rT, rCT >& testBase,
                 const Dune::FieldVector< D, d >& localPoint,
                 Dune::DynamicVector< R >& ret) const
  {
    const auto localFunction = std::get< 0 >(localFuncs);
    evaluate(*localFunction, testBase, localPoint, ret);
  }

  template< class E, class D, int d, class R, int rL, int rCL, int rT, int rCT >
  void evaluate(const Stuff::LocalfunctionInterface< E, D, d, R, rL, rCL >& localFunction,
                const Stuff::LocalfunctionSetInterface< E, D, d, R, rT, rCT >& testBase,
                const Dune::FieldVector< D, d >& localPoint,
                Dune::DynamicVector< R >& ret) const
  {
    typedef Dune::FieldVector< R, 1 > RangeType;
    // evaluate local function
    DUNE_THROW(NotImplemented, "wrong eval still");
    const auto functionValue = localFunction.evaluate(localPoint);
    // evaluate test base
    const size_t size = testBase.size();
    std::vector< RangeType > testValues(size, RangeType(0));
    testBase.evaluate(localPoint, testValues);
    // compute product
    assert(ret.size() >= size);
    for (size_t ii = 0; ii < size; ++ii) {
      ret[ii] = functionValue[0] * testValues[ii];
    }
  }


private:
  const LocalizableFunctionType& inducingFunction_;
}; // class Product



namespace Multiscale {
namespace MsFEM {


LocalProblemOperator::LocalProblemOperator(const CoarseSpaceType& coarse_space,
                                           const LocalGridDiscreteFunctionSpaceType& space,
                                           const DiffusionOperatorType& diffusion_op)
  : localSpace_(space)
  , diffusion_operator_(diffusion_op)
  , local_diffusion_operator_(diffusion_operator_)
  , coarse_space_(coarse_space)
  , system_matrix_(space.mapper().size(), space.mapper().size(),
                   EllipticOperatorType::pattern(space))
  , system_assembler_(localSpace_)
  , elliptic_operator_(local_diffusion_operator_, system_matrix_, localSpace_)
  , constraints_(Problem::getModelData()->subBoundaryInfo(), space.mapper().maxNumDofs(), space.mapper().maxNumDofs())
  , dirichletZero_(0)

{
  assemble_matrix();
}

void LocalProblemOperator::assemble_matrix()
    // x_T is the barycenter of the macro grid element T
{
  system_assembler_.add(elliptic_operator_);


} // assemble_matrix

void LocalProblemOperator::assemble_all_local_rhs(const CoarseEntityType& coarseEntity,
                                                  MsFEMTraits::LocalSolutionVectorType& allLocalRHS) {
  BOOST_ASSERT_MSG(allLocalRHS.size() > 0, "You need to preallocate the necessary space outside this function!");

  //! @todo correct the error message below (+1 for simplecial, +2 for arbitrary), as there's no finespace any longer
  //  BOOST_ASSERT_MSG(
  //      (DSG::is_simplex_grid(coarse_space_) && allLocalRHS.size() == GridType::dimension + 1) ||
  //          (!(DSG::is_simplex_grid(coarse_space_)) &&
  //           static_cast<long long>(allLocalRHS.size()) ==
  //               static_cast<long long>(specifier.fineSpace().mapper().maxNumDofs() + 2)),
  //      "You need to allocate storage space for the correctors for all unit vector/all coarse basis functions"
  //      " and the dirichlet- and neuman corrector");

  // build unit vectors (needed for cases where rhs is assembled for unit vectors instead of coarse
  // base functions)
  constexpr auto dimension = CommonTraits::GridType::dimension;
  CommonTraits::JacobianRangeType unitVectors[dimension];
  for (int i = 0; i < dimension; ++i) {
    for (int j = 0; j < dimension; ++j) {
      if (i == j) {
        unitVectors[i][0][j] = 1.0;
      } else {
        unitVectors[i][0][j] = 0.0;
      }
    }
  }

  LocalGridDiscreteFunctionType dirichletExtension(localSpace_, "dirichletExtension");
  CommonTraits::DiscreteFunctionType dirichletExtensionCoarse(coarse_space_, "Dirichlet Extension Coarse");

  GDT::SystemAssembler<CommonTraits::DiscreteFunctionSpaceType> global_system_assembler_(coarse_space_);
  GDT::Operators::DirichletProjectionLocalizable< CommonTraits::GridViewType, CommonTraits::DirichletDataType,
      CommonTraits::DiscreteFunctionType >
      coarse_dirichlet_projection_operator(*(coarse_space_.grid_view()),
                                    DMP::getModelData()->boundaryInfo(),
                                    *DMP::getDirichletData(),
                                    dirichletExtensionCoarse);
  global_system_assembler_.add(coarse_dirichlet_projection_operator, new GDT::ApplyOn::BoundaryEntities<CommonTraits::GridViewType>());
  global_system_assembler_.assemble();
  GDT::Operators::LagrangeProlongation<MsFEMTraits::LocalGridViewType> projection(*localSpace_.grid_view());
  projection.apply(dirichletExtensionCoarse, dirichletExtension);

  const bool is_simplex_grid = DSG::is_simplex_grid(coarse_space_);
  const auto numBoundaryCorrectors = is_simplex_grid ? 1u : 2u;
  const auto numInnerCorrectors = allLocalRHS.size() - numBoundaryCorrectors;
  //!*********** anfang neu gdt

  if (is_simplex_grid)
      DUNE_THROW(NotImplemented, "special treatment for simplicial grids missing");

  typedef CoarseBasisProduct<LocalDiffusionType> EvaluationType;
  typedef GDT::Functionals::L2Volume< LocalDiffusionType, CommonTraits::GdtVectorType,
      MsFEMTraits::LocalSpaceType, MsFEMTraits::LocalGridViewType,
      EvaluationType > RhsFunctionalType;
  std::vector<std::unique_ptr<RhsFunctionalType>> rhs_functionals(numInnerCorrectors);
  std::size_t coarseBaseFunc = 0;
  for (; coarseBaseFunc < numInnerCorrectors; ++coarseBaseFunc)
  {
    auto& rhs_vector = allLocalRHS[coarseBaseFunc]->vector();
    rhs_functionals[coarseBaseFunc] = DSC::make_unique<RhsFunctionalType>(local_diffusion_operator_, rhs_vector, localSpace_);
    system_assembler_.add(*rhs_functionals[coarseBaseFunc]);
  }

  coarseBaseFunc++; // coarseBaseFunc == numInnerCorrectors
  //neumann correktor
  typedef typename CommonTraits::NeumannDataType::template Transfer<MsFEMTraits::LocalEntityType>::Type LocalNeumannType;
//  LocalNeumannType local_neumann()
  GDT::Functionals::L2Face< LocalNeumannType, CommonTraits::GdtVectorType, MsFEMTraits::LocalSpaceType >
      neumann_functional(Dune::Multiscale::Problem::getNeumannData()->transfer<MsFEMTraits::LocalEntityType>(),
                         allLocalRHS[coarseBaseFunc]->vector(), localSpace_);
  system_assembler_.add(neumann_functional);


  coarseBaseFunc++;// coarseBaseFunc == 1 + numInnerCorrectors
  //dirichlet correktor
  {
//    const auto dirichletLF = dirichletExtension.local_function(entity);
//    dirichletLF.jacobian(local_point, dirichletJac);
//    diffusion_operator_.diffusiveFlux(global_point, dirichletJac, diffusion);
//    for (unsigned int i = 0; i < numBaseFunctions; ++i) {
//      rhsLocalFunction[i] -= weight * (diffusion[0] * gradient_phi[i][0]);
//    }
  }

  //dirichlet-0 for all rhs
  typedef GDT::ApplyOn::BoundaryEntities< MsFEMTraits::LocalGridViewType > OnLocalBoundaryEntities;
  LocalGridDiscreteFunctionType dirichlet_projection(localSpace_);
  GDT::Operators::DirichletProjectionLocalizable< MsFEMTraits::LocalGridViewType, MsFEMTraits::LocalConstantFunctionType,
                                                  MsFEMTraits::LocalGridDiscreteFunctionType >
      dirichlet_projection_operator(*(localSpace_.grid_view()),
                                    allLocalDirichletInfo_,
                                    dirichletZero_,
                                    dirichlet_projection);
  system_assembler_.add(dirichlet_projection_operator, new OnLocalBoundaryEntities());

  system_assembler_.add(constraints_, system_matrix_, new OnLocalBoundaryEntities());
  for (auto& rhs : allLocalRHS )
    system_assembler_.add(constraints_, rhs->vector(), new OnLocalBoundaryEntities());
  system_assembler_.assemble();

  //!*********** ende neu gdt

}


void LocalProblemOperator::apply_inverse(const MsFEMTraits::LocalGridDiscreteFunctionType &current_rhs,
                                         MsFEMTraits::LocalGridDiscreteFunctionType &current_solution)
{
  if (!current_rhs.dofs_valid())
    DUNE_THROW(Dune::InvalidStateException, "Local MsFEM Problem RHS invalid.");

  const auto solver =
      Dune::Multiscale::Problem::getModelData()->symmetricDiffusion() ? std::string("cg") : std::string("bcgs");
  typedef BackendChooser<LocalGridDiscreteFunctionSpaceType>::InverseOperatorType LocalInverseOperatorType;
  const auto localProblemSolver = DSC::make_unique<LocalInverseOperatorType>(system_matrix_,
                                                                             current_rhs.space().communicator());
                                                                             /*1e-8, 1e-8, 20000,
                                               DSC_CONFIG_GET("msfem.localproblemsolver_verbose", false), solver,
                                               DSC_CONFIG_GET("preconditioner_type", std::string("sor")), 1);*/
  localProblemSolver->apply(current_rhs.vector(), current_solution.vector());

  if (!current_solution.dofs_valid())
    DUNE_THROW(Dune::InvalidStateException, "Current solution of the local msfem problem invalid!");
}

} // namespace MsFEM {
} // namespace Multiscale {
} // namespace Dune {


#if 0 // alter dune-fem code
LocalProblemOperator::assemble_all_local_rhs
  // get the base function set of the coarse space for the given coarse entity
  const auto& coarseBaseSet = coarse_space_.basisFunctionSet(coarseEntity);
  std::vector<CoarseBaseFunctionSetType::JacobianRangeType> coarseBaseFuncJacs(coarseBaseSet.size());

  // gradient of micro scale base function:
  std::vector<JacobianRangeType> gradient_phi(localSpace.blockMapper().maxNumDofs());
  std::vector<RangeType> phi(localSpace.blockMapper().maxNumDofs());


  for (auto& localGridCell : localSpace) {
    const auto& geometry = localGridCell.geometry();
    const bool hasBoundaryIntersection = localGridCell.hasBoundaryIntersections();
    auto dirichletLF = dirichletExtension.localFunction(localGridCell);
    JacobianRangeType dirichletJac(0.0);

    for (std::size_t coarseBaseFunc = 0; coarseBaseFunc < allLocalRHS.size(); ++coarseBaseFunc) {
      auto rhsLocalFunction = allLocalRHS[coarseBaseFunc]->localFunction(localGridCell);

      const auto& baseSet = rhsLocalFunction.basisFunctionSet();
      const auto numBaseFunctions = baseSet.size();

      // correctors with index < numInnerCorrectors are for the basis functions, corrector at
      // position numInnerCorrectors is for the neumann values, corrector at position numInnerCorrectors+1
      // for the dirichlet values.
      if (coarseBaseFunc < numInnerCorrectors || coarseBaseFunc == numInnerCorrectors + 1) {
        const auto quadrature = DSFe::make_quadrature(localGridCell, localSpace);
        const auto numQuadraturePoints = quadrature.nop();
        for (size_t quadraturePoint = 0; quadraturePoint < numQuadraturePoints; ++quadraturePoint) {
          const auto& local_point = quadrature.point(quadraturePoint);
          // global point in the subgrid
          const auto global_point = geometry.global(local_point);

          const double weight = quadrature.weight(quadraturePoint) * geometry.integrationElement(local_point);

          JacobianRangeType diffusion(0.0);
          if (coarseBaseFunc < numInnerCorrectors) {
            if (is_simplex_grid)
              diffusion_operator_.diffusiveFlux(global_point, unitVectors[coarseBaseFunc], diffusion);
            else {
              const DomainType quadInCoarseLocal = coarseEntity.geometry().local(global_point);
              coarseBaseSet.jacobianAll(quadInCoarseLocal, coarseBaseFuncJacs);
              diffusion_operator_.diffusiveFlux(global_point, coarseBaseFuncJacs[coarseBaseFunc], diffusion);
            }
          } else {
            dirichletLF.jacobian(local_point, dirichletJac);
            diffusion_operator_.diffusiveFlux(global_point, dirichletJac, diffusion);
          }
          baseSet.jacobianAll(quadrature[quadraturePoint], gradient_phi);
          for (unsigned int i = 0; i < numBaseFunctions; ++i) {
            rhsLocalFunction[i] -= weight * (diffusion[0] * gradient_phi[i][0]);
          }
        }
      }

      // boundary integrals
      if (coarseBaseFunc == numInnerCorrectors && hasBoundaryIntersection) {
        const auto intEnd = localSpace.gridPart().iend(localGridCell);
        for (auto iIt = localSpace.gridPart().ibegin(localGridCell); iIt != intEnd; ++iIt) {
          const auto& intersection = *iIt;
          if (DMP::is_neumann(intersection)) {
            const auto orderOfIntegrand =
                (CommonTraits::polynomial_order - 1) + 2 * (CommonTraits::polynomial_order + 1);
            const auto quadOrder = std::ceil((orderOfIntegrand + 1) / 2);
            const auto faceQuad = DSFe::make_quadrature(intersection, localSpace, quadOrder);
            RangeType neumannValue(0.0);
            const auto numQuadPoints = faceQuad.nop();
            // loop over all quadrature points
            for (unsigned int iqP = 0; iqP < numQuadPoints; ++iqP) {
              // get local coordinate of quadrature point
              const auto& xLocal = faceQuad.localPoint(iqP);
              const auto& faceGeometry = intersection.geometry();

              // the following does not work because subgrid does not implement geometryInInside()
              // const auto& insideGeometry    = intersection.geometryInInside();
              // const typename FaceQuadratureType::CoordinateType& xInInside = insideGeometry.global(xLocal);
              // therefore, we have to do stupid things:
              const auto& xGlobal = faceGeometry.global(xLocal);
              auto insidePtr = intersection.inside();
              const auto& insideEntity = *insidePtr;
              const auto& xInInside = insideEntity.geometry().local(xGlobal);
              const double factor = faceGeometry.integrationElement(xLocal) * faceQuad.weight(iqP);

              neumannData.evaluate(xGlobal, neumannValue);
              baseSet.evaluateAll(xInInside, phi);
              for (unsigned int i = 0; i < numBaseFunctions; ++i) {
                rhsLocalFunction[i] -= factor * (neumannValue * phi[i]);
              }
            }
          }
        }
      }
    }
  }
#endif
