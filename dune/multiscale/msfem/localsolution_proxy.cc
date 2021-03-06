#include <config.h>

#include "localsolution_proxy.hh"

#include <dune/xt/common/timings.hh>
#include <dune/multiscale/msfem/localproblems/localgridsearch.hh>
#include <dune/multiscale/msfem/localsolution_proxy.hh>
#include <dune/multiscale/msfem/proxygridview.hh>
#include <dune/gdt/operators/prolongations.hh>

Dune::Multiscale::LocalsolutionProxy::LocalsolutionProxy(CorrectionsMapType&& corrections,
                                                         const CommonTraits::SpaceType& coarseSpace,
                                                         const LocalGridList& gridlist)
  : BaseType(*corrections.begin()->second)
  , corrections_(std::move(corrections))
  , view_(coarseSpace.grid_view())
  , index_set_(view_.grid().leafIndexSet())
  , search_(coarseSpace, gridlist)
{
  assert(corrections_.size() == index_set_.size(0));
}

std::unique_ptr<Dune::Multiscale::LocalsolutionProxy::LocalFunctionType>
Dune::Multiscale::LocalsolutionProxy::local_function(const BaseType::EntityType& entity) const
{
  const auto& coarse_cell = search_->current_coarse_pointer();
  auto it = corrections_.find(index_set_.index(coarse_cell));
  if (it != corrections_.end())
    return it->second->local_function(entity);
  DUNE_THROW(InvalidStateException, "Coarse cell was not found!");
}

void Dune::Multiscale::LocalsolutionProxy::add(const Dune::Multiscale::CommonTraits::DiscreteFunctionType& coarse_func)
{
  Dune::XT::Common::ScopedTiming st("proxy.add");
  CorrectionsMapType targets;
  for (auto& cr : corrections_) {
    targets[cr.first] =
        Dune::XT::Common::make_unique<MsFEMTraits::LocalGridDiscreteFunctionType>(cr.second->space(), "tmpcorrection");
  }
  for (auto& range_pr : targets) {
    const auto id = range_pr.first;
    auto& range = *range_pr.second;
    const Dune::GDT::Operators::LagrangeProlongation<MsFEMTraits::LocalGridViewType> prolongation_operator(
        range.space().grid_view());
    prolongation_operator.apply(coarse_func, range);
    auto& correction = *corrections_[id];
    correction.vector() += range.vector();
  }
}

Dune::Multiscale::LocalGridSearch& Dune::Multiscale::LocalsolutionProxy::search()
{
  return *search_;
}

void Dune::Multiscale::LocalsolutionProxy::visualize_parts(const Dune::XT::Common::Configuration& config) const
{
  const auto rank = MPIHelper::getCollectiveCommunication().rank();
  boost::format name("rank_%04d_msfemsolution_parts_%08i");
  boost::filesystem::path base(config.get("global.datadir", "data/"));
  for (const auto& part_pair : corrections_) {
    const auto& id = part_pair.first;
    const auto& solution = *part_pair.second;
    const auto path = base / (name % rank % id).str();
    solution.visualize(path.string());
  }
}

void Dune::Multiscale::LocalsolutionProxy::visualize(const std::string&) const
{
  DUNE_THROW(NotImplemented, "due the proxying to multiple functions this cannot work");
}
