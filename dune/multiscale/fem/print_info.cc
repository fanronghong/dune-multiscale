#include <config.h>
#include <boost/format.hpp>
#include <boost/optional/optional.hpp>
#include <dune/multiscale/common/output_traits.hh>
#include <dune/multiscale/tools/misc/outputparameter.hh>
#include <dune/multiscale/tools/discretefunctionwriter.hh>
#include <dune/stuff/common/parameter/configcontainer.hh>
#include <dune/pdelab/gridfunctionspace/vtk.hh>
#include <memory>
#include <sstream>

#include "dune/multiscale/common/traits.hh"
#include "dune/multiscale/problems/base.hh"
#include "dune/multiscale/problems/selector.hh"
#include "print_info.hh"

namespace Dune {
namespace Multiscale {
namespace FEM {

void write_discrete_function(typename CommonTraits::DiscreteFunction_ptr& discrete_solution, const std::string prefix) {
  // writing paraview data output
  // general output parameters
  Dune::Multiscale::OutputParameters outputparam;

  // create and initialize output class
  typename OutputTraits::IOTupleType fem_solution_series(discrete_solution.get());
  outputparam.set_prefix((boost::format("%s_solution") % prefix).str());
  typename OutputTraits::DataOutputType femsol_dataoutput(discrete_solution->space().gridPart().grid(),
                                                          fem_solution_series, outputparam);

  femsol_dataoutput.writeData(1.0 /*dummy*/, (boost::format("%s_solution") % prefix).str());

  //! -------------------------- writing data output Exact Solution ------------------------
  if (Problem::getModelData()->hasExactSolution()) {
    const auto& u = *Dune::Multiscale::Problem::getExactSolution();
    const OutputTraits::DiscreteExactSolutionType discrete_exact_solution("discrete exact solution ", u,
                                                                          discrete_solution->space().gridPart());
    // create and initialize output class
    OutputTraits::ExSolIOTupleType exact_solution_series(&discrete_exact_solution);
    outputparam.set_prefix("exact_solution");
    OutputTraits::ExSolDataOutputType exactsol_dataoutput(discrete_solution->space().gridPart().grid(),
                                                          exact_solution_series, outputparam);
    // write data
    exactsol_dataoutput.writeData(1.0 /*dummy*/, "exact-solution");
  }
}

void print_info(const CommonTraits::ModelProblemDataType& info, std::ostream& out) {
  // epsilon is specified in the parameter file
  // 'epsilon' in for instance A^{epsilon}(x) = A(x,x/epsilon)
  const double epsilon_ = DSC_CONFIG_GET("problem.epsilon", 1.0f);
  const int refinement_level_ = DSC_CONFIG_GET("fem.grid_level", 4);
  out << "Log-File for Elliptic Model Problem " << Problem::name() << "." << std::endl << std::endl;
  if (Problem::getModelData()->linear())
    out << "Problem is declared as being LINEAR." << std::endl;
  else
    out << "Problem is declared as being NONLINEAR." << std::endl;

  if (info.hasExactSolution()) {
    out << "Exact solution is available." << std::endl << std::endl;
  } else {
    out << "Exact solution is not available." << std::endl << std::endl;
  }
  out << "Computations were made for:" << std::endl << std::endl;
  out << "Refinement Level for Grid = " << refinement_level_ << std::endl << std::endl;

  out << "Epsilon = " << epsilon_ << std::endl << std::endl;
}

void write_discrete_function(CommonTraits::PdelabVectorType &discrete_solution, const std::string prefix)
{
  typedef PDELab::DiscreteGridFunction<CommonTraits::GridFunctionSpaceType,CommonTraits::PdelabVectorType> DGF;
  const auto& gfs = discrete_solution.gridFunctionSpace();
  SubsamplingVTKWriter<CommonTraits::GridFunctionSpaceType::Traits::GridView> vtkwriter(gfs.gridView(), CommonTraits::polynomial_order);
  PDELab::vtk::DefaultFunctionNameGenerator nn(prefix);
  PDELab::addSolutionToVTKWriter(vtkwriter,gfs,discrete_solution, nn);
  vtkwriter.write(prefix,Dune::VTK::appendedraw);
}

} // namespace FEM {
} // namespace Multiscale {
} // namespace Dune {
