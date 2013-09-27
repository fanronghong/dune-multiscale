#include "outputparameter.hh"

#include <config.h>

#include <dune/stuff/common/parameter/configcontainer.hh>
#include <dune/stuff/common/filesystem.hh>
#include <dune/multiscale/problems/selector.hh>

namespace Dune {
namespace Multiscale {

// OutputParameters::OutputParameters(const std::string path
//                                  = DSC_CONFIG_GET("global.datadir", "data")
OutputParameters::OutputParameters(const std::string path_in)
  : my_prefix_("solutions")
  , my_path_(path_in) {
  DSC::testCreateDirectory(my_path_);
}

void OutputParameters::set_prefix(std::string my_prefix) {
  my_prefix_ = my_prefix;
  DSC::testCreateDirectory(my_prefix_);
  // std :: cout << "Set prefix. my_prefix_ = " << my_prefix_ << std :: endl;
}

//! base of file name for data file
std::string OutputParameters::prefix() const { return my_prefix_; }

//! path where the data is stored
std::string OutputParameters::path() const { return my_path_; }

int OutputParameters::outputformat() const {
  // return 0; // GRAPE (lossless format)
  return 1; // VTK
            // return 2; // VTK vertex data
            // return 3; // gnuplot
}

bool OutputParameters::separateRankPath() const { return false; }

std::string OutputParameters::macroGridName(const int /*dim*/) const {
  return Problem::getModelData()->getMacroGridFile();
}

} // namespace Multiscale
} // namespace Dune
