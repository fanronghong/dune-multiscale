#include <config.h>
#include <dune/stuff/common/parameter/configcontainer.hh>
#include <dune/stuff/fem/functions/integrals.hh>
#include <dune/stuff/common/profiler.hh>
#include <sstream>

#include "dune/multiscale/msfem/msfem_traits.hh"
#include "coarse_scale_operator.hh"

namespace Dune {
namespace Multiscale {
namespace MsFEM {

CoarseScaleOperator::CoarseScaleOperator(const CoarseDiscreteFunctionSpace& coarseDiscreteFunctionSpace,
  LocalGridList &subgrid_list, const CommonTraits::DiffusionType &diffusion_op)
  : coarseDiscreteFunctionSpace_(coarseDiscreteFunctionSpace)
  , subgrid_list_(subgrid_list)
  , diffusion_operator_(diffusion_op)
  , petrovGalerkin_(false)
  , global_matrix_("MsFEM stiffness matrix", coarseDiscreteFunctionSpace_, coarseDiscreteFunctionSpace_){
  // the local problem:
  // Let 'T' denote a coarse grid element and
  // let 'U(T)' denote the environment of 'T' that corresponds with the subgrid.

  // if Petrov-Galerkin-MsFEM
  if (petrovGalerkin_)
    DSC_LOG_INFO << "Assembling Petrov-Galerkin-MsFEM Matrix." << std::endl;
  else // if classical (symmetric) MsFEM
    DSC_LOG_INFO << "Assembling MsFEM Matrix." << std::endl;

  //!TODO diagonal stencil reicht
  global_matrix_.reserve(DSFe::diagonalAndNeighborStencil(global_matrix_));
  global_matrix_.clear();

  Fem::DomainDecomposedIteratorStorage< CommonTraits::GridPartType > threadIterators(coarseDiscreteFunctionSpace_.gridPart());

  #ifdef _OPENMP
  #pragma omp parallel
  #endif
  {
  for (const auto& coarse_grid_entity : threadIterators) {
    const auto& coarse_grid_geometry = coarse_grid_entity.geometry();
    assert(coarse_grid_entity.partitionType() == InteriorEntity);

    DSFe::LocalMatrixProxy<MatrixType> local_matrix(global_matrix_, coarse_grid_entity, coarse_grid_entity);

    const auto& coarse_grid_baseSet = local_matrix.domainBasisFunctionSet();
    const auto numMacroBaseFunctions = coarse_grid_baseSet.size();

    Multiscale::MsFEM::LocalSolutionManager localSolutionManager(coarseDiscreteFunctionSpace_, coarse_grid_entity, subgrid_list_);
    localSolutionManager.load();
    const auto& localSolutions = localSolutionManager.getLocalSolutions();
    assert(localSolutions.size() > 0);
    std::vector<JacobianRangeType> gradientPhi(numMacroBaseFunctions);

    for (const auto& localGridEntity : localSolutionManager.space()) {
      // ignore overlay elements
      if (subgrid_list_.covers(coarse_grid_entity, localGridEntity)) {
        const auto& local_grid_geometry = localGridEntity.geometry();

        // higher order quadrature, since A^{\epsilon} is highly variable
        const auto localQuadrature =
            DSFe::make_quadrature(localGridEntity, localSolutionManager.space());
        const auto numQuadraturePoints = localQuadrature.nop();

        // number of local solutions without the boundary correctors. Those are only needed for the right hand side
        const auto numLocalSolutions = localSolutions.size() - localSolutionManager.numBoundaryCorrectors();
        // evaluate the jacobians of all local solutions in all quadrature points
        std::vector<std::vector<JacobianRangeType>> allLocalSolutionEvaluations(
            numLocalSolutions, std::vector<JacobianRangeType>(localQuadrature.nop(), JacobianRangeType(0.0)));
        for (auto lsNum : DSC::valueRange(numLocalSolutions)) {
          auto& sll = localSolutions[lsNum];
          assert(sll.get());
          assert(sll->dofsValid());
          assert(localSolutionManager.space().indexSet().contains(localGridEntity));
          auto localFunction = sll->localFunction(localGridEntity);
          localFunction.evaluateQuadrature(localQuadrature, allLocalSolutionEvaluations[lsNum]);
        }

        for (size_t localQuadraturePoint = 0; localQuadraturePoint < numQuadraturePoints; ++localQuadraturePoint) {
          // local (barycentric) coordinates (with respect to entity)
          const auto& local_subgrid_point = localQuadrature.point(localQuadraturePoint);

          auto global_point_in_U_T = local_grid_geometry.global(local_subgrid_point);
          const double weight_local_quadrature = localQuadrature.weight(localQuadraturePoint) *
                                                 local_grid_geometry.integrationElement(local_subgrid_point);

          // evaluate the jacobian of the coarse grid base set
          const auto& local_coarse_point = coarse_grid_geometry.local(global_point_in_U_T);
          coarse_grid_baseSet.jacobianAll(local_coarse_point, gradientPhi);

          for (unsigned int i = 0; i < numMacroBaseFunctions; ++i) {
            for (unsigned int j = 0; j < numMacroBaseFunctions; ++j) {
              RangeType local_integral(0.0);

              // Compute the gradients of the i'th and j'th local problem solutions
              JacobianRangeType gradLocProbSoli(0.0), gradLocProbSolj(0.0);
              if (DSG::is_simplex_grid(coarseDiscreteFunctionSpace_)) {
                assert(allLocalSolutionEvaluations.size() == CommonTraits::GridType::dimension);
                // ∇ Phi_H + ∇ Q( Phi_H ) = ∇ Phi_H + ∂_x1 Phi_H ∇Q( e_1 ) + ∂_x2 Phi_H ∇Q( e_2 )
                for (int k = 0; k < CommonTraits::GridType::dimension; ++k) {
                  gradLocProbSoli.axpy(gradientPhi[i][0][k], allLocalSolutionEvaluations[k][localQuadraturePoint]);
                  gradLocProbSolj.axpy(gradientPhi[j][0][k], allLocalSolutionEvaluations[k][localQuadraturePoint]);
                }
              } else {
                assert(allLocalSolutionEvaluations.size() == numMacroBaseFunctions);
                gradLocProbSoli = allLocalSolutionEvaluations[i][localQuadraturePoint];
                gradLocProbSolj = allLocalSolutionEvaluations[j][localQuadraturePoint];
              }

              JacobianRangeType reconstructionGradPhii(gradientPhi[i]);
              reconstructionGradPhii += gradLocProbSoli;
              JacobianRangeType reconstructionGradPhij(gradientPhi[j]);
              reconstructionGradPhij += gradLocProbSolj;
              JacobianRangeType diffusive_flux(0.0);
              diffusion_operator_.diffusiveFlux(global_point_in_U_T, reconstructionGradPhii, diffusive_flux);
              if (petrovGalerkin_)
                local_integral += weight_local_quadrature * (diffusive_flux[0] * gradientPhi[j][0]);
              else
                local_integral += weight_local_quadrature * (diffusive_flux[0] * reconstructionGradPhij[0]);

              // add entries
              local_matrix.add(j, i, local_integral);
            }
          }
        }
      }
    }
  } // for
  } // omp region

  // set unit rows for dirichlet dofs
  Dune::Multiscale::getConstraintsCoarse(coarseDiscreteFunctionSpace_).applyToOperator(global_matrix_);
  global_matrix_.communicate();
}

void CoarseScaleOperator::apply_inverse(const CoarseScaleOperator::CoarseDiscreteFunction &rhs, CoarseScaleOperator::CoarseDiscreteFunction &solution)
{
  BOOST_ASSERT_MSG(rhs.dofsValid(), "Coarse scale RHS DOFs need to be valid!");
  DSC_PROFILER.startTiming("msfem.solveCoarse");
  const typename BackendChooser<CoarseDiscreteFunctionSpace>::InverseOperatorType inverse(global_matrix_, 1e-8, 1e-8,
                                           DSC_CONFIG_GET("msfem.solver.iterations", rhs.size()),
                                           DSC_CONFIG_GET("msfem.solver.verbose", false), "bcgs",
                                           DSC_CONFIG_GET("msfem.solver.preconditioner_type", std::string("sor")));
  inverse(rhs, solution);
  if (!solution.dofsValid())
    DUNE_THROW(InvalidStateException, "Degrees of freedom of coarse solution are not valid!");
  DSC_LOG_INFO << "Time to solve coarse MsFEM problem: " << DSC_PROFILER.stopTiming("msfem.solveCoarse")
               << "ms." << std::endl;
} // constructor

} // namespace MsFEM {
} // namespace Multiscale {
} // namespace Dune {
