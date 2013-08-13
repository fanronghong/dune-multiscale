// dune-multiscale
// Copyright Holders: Patrick Henning, Rene Milk
// License: BSD 2-Clause License (http://opensource.org/licenses/BSD-2-Clause)

#ifndef DUNE_MS_PROBLEMS_SELECTOR_HH
#define DUNE_MS_PROBLEMS_SELECTOR_HH

#include <config.h>
#include <dune/multiscale/problems/base.hh>
#include <dune/multiscale/msfem/msfem_traits.hh>

namespace Dune {
namespace Multiscale {
namespace Problem {

typedef std::unique_ptr<const CommonTraits::FunctionBaseType> BasePtr;
BasePtr getFirstSource();
BasePtr getExactSolution();
BasePtr getSecondSource();
BasePtr getMassTerm();
std::unique_ptr<const CommonTraits::DiffusionType> getDiffusion();
std::unique_ptr<const CommonTraits::LowerOrderTermType> getLowerOrderTerm();
std::unique_ptr<const CommonTraits::DirichletBCType> getDirichletBC();
std::unique_ptr<const CommonTraits::NeumannBCType> getNeumannBC();
BasePtr getDefaultDummyFunction();
std::unique_ptr<const CommonTraits::ModelProblemDataType> getModelData();
std::unique_ptr<const CommonTraits::DirichletDataType> getDirichletData();
std::unique_ptr<const CommonTraits::NeumannDataType> getNeumannData();

template< class GridImp, class IntersectionImp >
bool isNeumannBoundary(const Dune::Intersection<GridImp, IntersectionImp>& face);
template< class GridImp, class IntersectionImp >
bool isDirichletBoundary(const Dune::Intersection<GridImp, IntersectionImp>& face);

std::string name();

} //! @} namespace Problem
} // namespace Multiscale
} // namespace Dune

#endif // DUNE_MS_PROBLEMS_SELECTOR_HH
