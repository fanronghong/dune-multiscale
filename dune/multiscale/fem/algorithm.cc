// dune-multiscale
// Copyright Holders: Patrick Henning, Rene Milk
// License: BSD 2-Clause License (http://opensource.org/licenses/BSD-2-Clause)

#include "algorithm.hh"

#include <dune/multiscale/tools/disc_func_writer/discretefunctionwriter.hh>
#include <dune/multiscale/tools/misc/outputparameter.hh>
#include <dune/multiscale/tools/homogenizer/elliptic_homogenizer.hh>
#include <dune/multiscale/tools/righthandside_assembler.hh>
#include <dune/multiscale/tools/misc/h1error.hh>

#include <dune/stuff/common/ranges.hh>
#include <dune/stuff/common/profiler.hh>
#include <dune/stuff/common/logging.hh>
#include <dune/stuff/common/parameter/configcontainer.hh>
#include <dune/fem/misc/l2norm.hh>
#include <dune/fem/misc/l2error.hh>
#include <dune/fem/misc/h1norm.hh>

#include <string>
#include <fstream>

#include "fem_traits.hh"

namespace {
  const std::string seperator_line = "---------------------------------------------------------------------------------\n";
}

namespace Dune {
namespace Multiscale {
namespace FEM {

//! set the dirichlet points to zero
template< class DiscreteFunctionType >
void boundaryTreatment(DiscreteFunctionType& rhs) {
  using namespace Dune::Stuff;
  const auto& discreteFunctionSpace = rhs.space();
  static const unsigned int faceCodim = 1;
  for (const auto& entity : discreteFunctionSpace)
  {
    for (const auto& intersection
         : Dune::Stuff::Common::intersectionRange(discreteFunctionSpace.gridPart(), entity))
    {
      if ( !intersection.boundary() )
        continue;
      auto rhsLocal = rhs.localFunction(entity);
      const auto face = intersection.indexInInside();
      for(auto point
          : Dune::Stuff::Common::lagrangePointSetRange<faceCodim>(rhs.space(), entity, face))
        rhsLocal[point] = 0;
    }
  }
} // boundaryTreatment


//! write discrete function to a file + VTK Output
void write_discrete_function(typename CommonTraits::DiscreteFunctionType& discrete_solution )
 {
  // write the final (discrete) solution to a file
  std::string solution_file = (boost::format("/fem_solution_refLevel_%d")
                                % DSC_CONFIG_GET("fem.grid_level", 4) ).str();
  DiscreteFunctionWriter(solution_file).append(discrete_solution);

  // writing paraview data output
  // general output parameters
  Dune::Multiscale::OutputParameters outputparam;

  // create and initialize output class
  typename CommonTraits::IOTupleType fem_solution_series(&discrete_solution);
  outputparam.set_prefix((boost::format("/fem_solution")).str());
  typename CommonTraits::DataOutputType femsol_dataoutput(discrete_solution.space().gridPart().grid(),
                                                 fem_solution_series, outputparam);
  // write data
  if (DSC_CONFIG_GET("problem.linear", true))
    femsol_dataoutput.writeData( 1.0 /*dummy*/, "fem-solution" );
  else
    femsol_dataoutput.writeData( 1.0 /*dummy*/, "fem-newton-solution" );
 }

//! \TODO docme
void solve(typename CommonTraits::DiscreteFunctionType& solution,
           const typename CommonTraits::DiscreteFunctionSpaceType& finerDiscreteFunctionSpace,
           const typename FEMTraits::EllipticOperatorType& discrete_elliptic_op,
           const std::string& filename,
           const Dune::RightHandSideAssembler< typename CommonTraits::DiscreteFunctionType >& rhsassembler)
{
  static const int fem_polorder = 2* CommonTraits::DiscreteFunctionSpaceType::polynomialOrder + 2;

  //! *************************** Assembling the problem ****************************

  //! (stiffness) matrix
  typename CommonTraits::FEMMatrix system_matrix("FEM Newton stiffness matrix", finerDiscreteFunctionSpace, finerDiscreteFunctionSpace);

  //! right hand side vector
  // right hand side for the finite element method with Newton solver:
  // ( also right hand side for the finer discrete function space )
  typename CommonTraits::DiscreteFunctionType system_rhs("fem newton rhs", finerDiscreteFunctionSpace);
  system_rhs.clear();

  const typename CommonTraits::FirstSourceType f;   // standard source f

  if (DSC_CONFIG_GET("problem.linear", true))
  {
    DSC_LOG_INFO << "Solving linear problem." << std::endl;
    DSC_LOG_INFO << "Solving linear problem with standard FEM and resolution level "
                  << DSC_CONFIG_GET("fem.grid_level", 4) << "." << std::endl;
    DSC_LOG_INFO << "------------------------------------------------------------------------------" << std::endl;

    // to assemble the computational time
    Dune::Timer assembleTimer;

    // assemble the stiffness matrix
    discrete_elliptic_op.assemble_matrix( system_matrix );

    DSC_LOG_INFO << "Time to assemble standard FEM stiffness matrix: " << assembleTimer.elapsed() << "s" << std::endl;

    // assemble right hand side
    rhsassembler.assemble< fem_polorder >(f, system_rhs);

    // set Dirichlet Boundary to zero
    boundaryTreatment(system_rhs);

    const typename FEMTraits::InverseFEMMatrix fem_biCGStab(system_matrix, 1e-8, 1e-8, 20000, DSC_CONFIG_GET("global.cgsolver_verbose", false));
    fem_biCGStab(system_rhs, solution);

    DSC_LOG_INFO << "---------------------------------------------------------------------------------" << std::endl;
    DSC_LOG_INFO << "Standard FEM problem solved in " << assembleTimer.elapsed() << "s." << std::endl << std::endl
              << std::endl;
  } else {
    DSC_LOG_INFO << "Solving non-linear problem." << std::endl;
    DSC_LOG_INFO << "Solving nonlinear problem with FEM + Newton-Method. Resolution level of grid = "
                  << DSC_CONFIG_GET("fem.grid_level", 4) << "." << std::endl;
    DSC_LOG_INFO << "---------------------------------------------------------------------------------" << std::endl;

    Dune::Timer assembleTimer;
    //! residual vector
    // current residual
    typename CommonTraits::DiscreteFunctionType residual(filename + "FEM Newton Residual", finerDiscreteFunctionSpace);
    residual.clear();

    typename CommonTraits::RangeType relative_newton_error_finescale = 10000.0;
    typename CommonTraits::RangeType rhs_L2_norm = 10000.0;

    int iteration_step = 1;
    // the Newton step for the FEM reference problem (solved with Newton Method):
    // L2-Norm of residual < tolerance ?
    double tolerance = 1e-06;
    while (relative_newton_error_finescale > tolerance)
    {
      // (here: solution = solution from the last iteration step)
      DSC_LOG_INFO << "Newton iteration " << iteration_step << ":" << std::endl;
      Dune::Timer stepAssembleTimer;
      // assemble the stiffness matrix
      discrete_elliptic_op.assemble_jacobian_matrix(solution, system_matrix);

      DSC_LOG_INFO << "Time to assemble FEM Newton stiffness matrix for current iteration: "
                   << stepAssembleTimer.elapsed() << "s" << std::endl;

      // assemble right hand side
      const typename CommonTraits::DiffusionType diffusion_op;
      rhsassembler.assemble_for_Newton_method< fem_polorder >(f,
                                                                       diffusion_op,
                                                                       solution,
                                                                       system_rhs);

      const Dune::L2Norm< typename CommonTraits::DiscreteFunctionType::GridPartType > l2norm(system_rhs.gridPart());
      rhs_L2_norm = l2norm.norm(system_rhs);
      if (rhs_L2_norm < 1e-10)
      {
        // residual solution almost identical to zero: break
        DSC_LOG_INFO << "Residual solution almost identical to zero. Therefore: break loop." << std::endl;
        DSC_LOG_INFO << "(L^2-Norm of current right hand side = " << rhs_L2_norm << " < 1e-10)" << std::endl;
        break;
      }
      // set Dirichlet Boundary to zero
      boundaryTreatment(system_rhs);

      const typename FEMTraits::InverseFEMMatrix fem_newton_biCGStab(system_matrix, 1e-8, 1e-8, 20000, true);
      fem_newton_biCGStab(system_rhs, residual);

      if ( residual.dofsValid() )
      {
        solution += residual;
        relative_newton_error_finescale = l2norm.norm(residual);
        relative_newton_error_finescale /= l2norm.norm(solution);

        DSC_LOG_INFO << "Relative L2-Newton Error = " << relative_newton_error_finescale << std::endl;
        // residual solution almost identical to zero: break
        DSC_LOG_INFO << "Relative L2-Newton Error = " << relative_newton_error_finescale << std::endl;
        if (relative_newton_error_finescale <= tolerance)
        {
          DSC_LOG_INFO << "Since tolerance = " << tolerance << ": break loop." << std::endl;
        }
        residual.clear();
      } else {
        DSC_LOG_INFO << "WARNING! Invalid dofs in 'residual'." << std::endl;
        break;
      }
      iteration_step += 1;
    }

    if (DSC_CONFIG_GET("problem.linear", true))
    {
      DSC_LOG_INFO << "Finite Element Problem solved in " << assembleTimer.elapsed() << "s." << std::endl
                   << std::endl << std::endl;
    }
    else
    {
      DSC_LOG_INFO << "Nonlinear Finite Element Problem solved with Newton Method in " << assembleTimer.elapsed() << "s." << std::endl
                   << std::endl << std::endl;
    }
    DSC_LOG_INFO << seperator_line;

  }// end 'problem.linear <-> else'

  //! ********************** End of assembling the reference problem ***************************
}

//! outputs Problem info to output stream
template <class ProblemDataType>
void print_info(ProblemDataType info, std::ostream& out)
{
  // epsilon is specified in the parameter file
  // 'epsilon' in for instance A^{epsilon}(x) = A(x,x/epsilon)
  const double epsilon_ = DSC_CONFIG_GET("problem.epsilon", 1.0f);
  const int refinement_level_ = DSC_CONFIG_GET("fem.grid_level", 4);
  out << "Log-File for Elliptic Model Problem " << Problem::name << "." << std::endl << std::endl;
  if (DSC_CONFIG_GET("problem.linear", true))
    out << "Problem is declared as being LINEAR." << std::endl;
  else
    out << "Problem is declared as being NONLINEAR." << std::endl;

  if (info.has_exact_solution) {
    out << "Exact solution is available." << std::endl << std::endl;
  } else {
    out << "Exact solution is not available." << std::endl << std::endl;
  }
  out << "Computations were made for:" << std::endl << std::endl;
  out << "Refinement Level for Grid = " << refinement_level_ << std::endl << std::endl;

  out << "Epsilon = " << epsilon_ << std::endl << std::endl;
}


//! the main FEM computation
void algorithm(typename CommonTraits::GridPointerType& macro_grid_pointer,
               const std::string filename) {
  using namespace Dune;

  const typename CommonTraits::ModelProblemDataType problem_data;
  print_info(problem_data, DSC_LOG_INFO);
  //! ---------------------------- grid parts ----------------------------------------------
  // grid part for the global function space, required for the finite element problem
  typename CommonTraits::GridPartType gridPart(*macro_grid_pointer);
  //! --------------------------------------------------------------------------------------

  //! ------------------------- discrete function spaces -----------------------------------
  // the global-problem function space:
  typename CommonTraits::DiscreteFunctionSpaceType discreteFunctionSpace(gridPart);
  //! --------------------------------------------------------------------------------------

  // defines the matrix A^{\epsilon} in our global problem  - div ( A^{\epsilon}(\nabla u^{\epsilon} ) = f
  const typename CommonTraits::DiffusionType diffusion_op;

  //! define the right hand side assembler tool
  // (for linear and non-linear elliptic and parabolic problems, for sources f and/or G )
  Dune::RightHandSideAssembler< typename CommonTraits::DiscreteFunctionType > rhsassembler;

  //! define the discrete (elliptic) operator that describes our problem
  // ( effect of the discretized differential operator on a certain discrete function )
  const typename FEMTraits::EllipticOperatorType discrete_elliptic_op(discreteFunctionSpace, diffusion_op);

  //! solution vector
  // - By solution, we denote the "discrete solution" determined with FEM in the linear case or FEM-Newton (nonlinear case)
  //    ( if the elliptic problem is linear, the 'solution' is determined without the Newton method )
  // - solution of the finite element method, where we use the Newton method to solve the non-linear system of equations
  typename CommonTraits::DiscreteFunctionType discrete_solution(filename + " FEM(-Newton) Solution", discreteFunctionSpace);
  discrete_solution.clear();

  solve(discrete_solution, discreteFunctionSpace, discrete_elliptic_op, filename, rhsassembler);

  // write FEM solution to a file and produce a VTK output
  write_discrete_function(discrete_solution);

  //! ----------------- compute L2- and H1- errors -------------------
  if (Problem::ModelProblemData::has_exact_solution)
  {

    DSC_LOG_INFO << std::endl << "The L2 and H1 error:" << std::endl << std::endl;
    H1Error< typename CommonTraits::DiscreteFunctionType > h1error;
    L2Error< typename CommonTraits::DiscreteFunctionType > l2error;

    const typename CommonTraits::ExactSolutionType u;

    typedef typename CommonTraits::ExactSolutionType ExactSolution;

    const int order_quadrature_rule = 13;

    typename CommonTraits::RangeType fem_error = l2error.norm< ExactSolution >
       (u, discrete_solution, order_quadrature_rule /* * CommonTraits::DiscreteFunctionSpaceType::polynomialOrder */ );
    DSC_LOG_INFO << "|| u_fem - u_exact ||_L2 =  " << fem_error << std::endl << std::endl;

    typename CommonTraits::RangeType h1_fem_error(0.0);
    h1_fem_error = h1error.semi_norm < ExactSolution >(u, discrete_solution, order_quadrature_rule);
    h1_fem_error += fem_error;
    DSC_LOG_INFO << "|| u_fem - u_exact ||_H1 =  " << h1_fem_error << std::endl << std::endl;
  }
}

//! \TODO docme
void algorithm_hom_fem(typename CommonTraits::GridPointerType& macro_grid_pointer,
                       const std::string filename) {
  using namespace Dune;

  const typename CommonTraits::ModelProblemDataType problem_data;
  print_info(problem_data, DSC_LOG_INFO);
  //! ---------------------------- grid parts ----------------------------------------------
  // grid part for the global function space, required for the finite element problem
  typename CommonTraits::GridPartType gridPart(*macro_grid_pointer);
  //! --------------------------------------------------------------------------------------

  //! ------------------------- discrete function spaces -----------------------------------
  // the global-problem function space:
  typename CommonTraits::DiscreteFunctionSpaceType discreteFunctionSpace(gridPart);
  //! --------------------------------------------------------------------------------------

  // defines the matrix A^{\epsilon} in our global problem  - div ( A^{\epsilon}(\nabla u^{\epsilon} ) = f
  const typename CommonTraits::DiffusionType diffusion_op;

  //! define the right hand side assembler tool
  // (for linear and non-linear elliptic and parabolic problems, for sources f and/or G )
  Dune::RightHandSideAssembler< typename CommonTraits::DiscreteFunctionType > rhsassembler;
  const typename CommonTraits::FirstSourceType f;   // standard source f

  //! define the discrete (elliptic) operator that describes our problem
  // ( effect of the discretized differential operator on a certain discrete function )
  const typename FEMTraits::EllipticOperatorType discrete_elliptic_op(discreteFunctionSpace, diffusion_op);

  // unit cube grid for the computations of cell problems
  const std::string unit_cell_location = "../dune/multiscale/grids/cell_grids/unit_cube.dgf";
  // descretized homogenizer:

  typedef Dune::Homogenizer< typename CommonTraits::GridType, typename CommonTraits::DiffusionType > HomogenizerType;

  // to create an empty diffusion matrix that can be filled with constant values
  typedef Problem::ConstantDiffusionMatrix< typename CommonTraits::FunctionSpaceType, typename HomogenizerType::HomTensorType >
     HomDiffusionType;

  const HomogenizerType disc_homogenizer(unit_cell_location);
  const typename HomogenizerType::HomTensorType A_hom = disc_homogenizer.getHomTensor(diffusion_op);
  const HomDiffusionType hom_diffusion_op(A_hom);

  //!TODO check: hatte nur 2 tmp parameter, Masse hinzugefUGT
  typedef DiscreteEllipticOperator< typename CommonTraits::DiscreteFunctionType,
                                    HomDiffusionType, typename CommonTraits::MassTermType > HomEllipticOperatorType;

  HomEllipticOperatorType hom_discrete_elliptic_op( discreteFunctionSpace, hom_diffusion_op);

  typename CommonTraits::FEMMatrix hom_stiff_matrix("homogenized stiffness matrix", discreteFunctionSpace, discreteFunctionSpace);

  typename CommonTraits::DiscreteFunctionType hom_rhs("homogenized rhs", discreteFunctionSpace);
  hom_rhs.clear();

  //! solution vector
  // - By solution, we denote the (discrete) homogenized solution determined with FEM on the coarse scale and FEM for the cell problems
  typename CommonTraits::DiscreteFunctionType homogenized_solution(filename + " Homogenized Solution", discreteFunctionSpace);
  homogenized_solution.clear();
  hom_discrete_elliptic_op.assemble_matrix(hom_stiff_matrix);

  constexpr int hmm_polorder = 2* CommonTraits::DiscreteFunctionSpaceType::polynomialOrder + 2;
  rhsassembler.assemble < hmm_polorder >(f, hom_rhs);

  // set Dirichlet Boundary to zero
  boundaryTreatment(hom_rhs);

  const typename FEMTraits::InverseFEMMatrix hom_biCGStab(hom_stiff_matrix, 1e-8, 1e-8, 20000, DSC_CONFIG_GET("global.cgsolver_verbose", false));
  hom_biCGStab(hom_rhs, homogenized_solution);

  // write FEM solution to a file and produce a VTK output
  // ---------------------------------------------------------------------------------

  // write the final (discrete) solution to a file
  std::string solution_file = (boost::format("/homogenized_solution_macro_refLevel_%d")
                                % DSC_CONFIG_GET("fem.grid_level", 4) ).str();
  DiscreteFunctionWriter(solution_file).append(homogenized_solution);

  // writing paraview data output
  // general output parameters
  Dune::Multiscale::OutputParameters outputparam;

  // create and initialize output class
  typename CommonTraits::IOTupleType hom_fem_solution_series(&homogenized_solution);
  outputparam.set_prefix((boost::format("/homogenized_solution")).str());
  typename CommonTraits::DataOutputType homfemsol_dataoutput(homogenized_solution.space().gridPart().grid(),
                                                    hom_fem_solution_series, outputparam);
  homfemsol_dataoutput.writeData( 1.0 /*dummy*/, "homogenized-solution" );

  // ---------------------------------------------------------------------------------

}

} //namespace FEM {
} //namespace Multiscale {
} //namespace Dune {
