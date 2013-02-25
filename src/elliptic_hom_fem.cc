#include "common.hh"

// The following FEM code requires an access to the 'ModelProblemData' class,
// which provides us with information about f, A, \Omega, etc.


#ifdef HAVE_CMAKE_CONFIG
 #include "cmake_config.h"
#elif defined (HAVE_CONFIG_H)
 #include <config.h>
#endif // ifdef HAVE_CMAKE_CONFIG


//! local (dune-multiscale) includes
#include <dune/multiscale/fem/fem_traits.hh>
#include <dune/multiscale/fem/algorithm.hh>

//! (very restrictive) homogenizer
#include <dune/multiscale/tools/homogenizer/elliptic_analytical_homogenizer.hh>
#include <dune/multiscale/tools/homogenizer/elliptic_homogenizer.hh>


int main(int argc, char** argv) {
  try {
    init(argc, argv);

    if ( !DSC_CONFIG_GET("problem.linear", 1 ) )
     DUNE_THROW(Dune::InvalidStateException, "Problem is declared to be nonlinear. Homogenizers are only available for linear homogenization problems.");
     
    using namespace Dune::Multiscale::FEM;

    DSC_PROFILER.startTiming("total_cpu");

    const std::string path = DSC_CONFIG_GET("global.datadir", "data/");

    // generate directories for data output
    DSC::testCreateDirectory(path);

    // name of the error file in which the data will be saved
    std::string filename_;
    const Problem::ModelProblemData info;

    if ( !info.problemIsPeriodic() )
     DUNE_THROW(Dune::InvalidStateException, "Problem is declared to be non-periodic. Homogenizers are only available for periodic homogenization problems.");

    if ( DSC_CONFIG_GET("problem.stochastic_pertubation", false) )
     DUNE_THROW(Dune::InvalidStateException, "Homogenizers are only available for non-stochastically perturbed problems. Please, switch off the key 'problem.stochastic_pertubation'.");
    
    const std::string save_filename = std::string(path + "/logdata/ms.log.log");
    DSC_LOG_INFO << "LOG FILE " << std::endl << std::endl;
    std::cout << "Data will be saved under: " << save_filename << std::endl;
    DSC_LOG_INFO << "Solving the homogenized problem with a standard Finite Element method." << std::endl << std::endl;

    // refinement_level denotes the grid refinement level for the global problem, i.e. it describes 'H'
    const int refinement_level = DSC_CONFIG_GET("fem.grid_level", 4);

    // name of the grid file that describes the macro-grid:
    const std::string gridName = info.getMacroGridFile();
    DSC_LOG_INFO << "loading dgf: " << gridName << std::endl;

    // create a grid pointer for the DGF file belongig to the macro grid:
    FEMTraits::GridPointerType grid_pointer(gridName);

    // refine the grid 'starting_refinement_level' times:
    grid_pointer->globalRefine(refinement_level);

    algorithm_hom_fem(grid_pointer, filename_);

    const auto cpu_time = DSC_PROFILER.stopTiming("total_cpu") / 1000.f;
    DSC_LOG_INFO << "Total runtime of the program: " << cpu_time << "ms" << std::endl;
    return 0;
  } catch (Dune::Exception& e) {
    std::cerr << e.what() << std::endl;
  }
  return 1;
} // main