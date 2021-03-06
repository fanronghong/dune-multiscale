// dune-multiscale
// Copyright Holders: Patrick Henning, Rene Milk
// License: BSD 2-Clause License (http://opensource.org/licenses/BSD-2-Clause)

#ifndef DUNE_MULTISCALE_MSFEM_ALGORITHM_HH
#define DUNE_MULTISCALE_MSFEM_ALGORITHM_HH

#include <dune/multiscale/common/traits.hh>
#include <dune/multiscale/msfem/msfem_traits.hh>
#include <string>
#include <vector>

namespace Dune {
namespace Multiscale {

class LocalsolutionProxy;
struct OutputParameters;
class LocalGridList;

//! \TODO docme
std::map<std::string, double> msfem_algorithm();

} // namespace Multiscale {
} // namespace Dune {

#endif // DUNE_MULTISCALE_MSFEM_ALGORITHM_HH
