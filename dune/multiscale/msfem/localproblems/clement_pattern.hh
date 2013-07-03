#ifndef DUNE_MULTISCALE_CLEMENT_PATTERN_HH
#define DUNE_MULTISCALE_CLEMENT_PATTERN_HH

#include <dune/stuff/aliases.hh>
#include <dune/stuff/la/container/pattern.hh>
#include <dune/multiscale/tools/misc.hh>

#include <unordered_map>
#include <unordered_set>

namespace Dune {
namespace Multiscale {
namespace MsFEM {

template <class IndexSetType>
struct EntityPointerHash {
  const IndexSetType& index_set_;

  EntityPointerHash(const IndexSetType& index_set)
    :index_set_(index_set)
  {}

  template < class GridImp, class IteratorImp >
  std::size_t operator() (const Dune::EntityPointer< GridImp, IteratorImp >& ptr) const
  {
    return index_set_.index(*ptr);
  }
};

template < class FunctionSpaceTraits >
std::vector<int> mapEach(const Dune::Fem::DiscreteFunctionSpaceInterface<FunctionSpaceTraits>& space,
                         const typename Dune::Fem::DiscreteFunctionSpaceInterface<FunctionSpaceTraits>::EntityType& entity)
{
  const auto& mapper = space.blockMapper();
  std::vector<int> ret(mapper.numDofs(entity));
  auto add = [&](int localDof, int globalDof){ ret[localDof] = globalDof; };
  mapper.mapEach(entity, add);
  return ret;
}

template<class DomainSpace, class RangeSpace>
class ClemementPattern : public DSL::SparsityPatternDefault {
  typedef DSL::SparsityPatternDefault BaseType;
  typedef typename DomainSpace::EntityType::EntityPointer DomainEntityPointerType;
  typedef typename RangeSpace::EntityType::EntityPointer RangeEntityPointerType;
  typedef std::unordered_set<RangeEntityPointerType,
                             EntityPointerHash<typename RangeSpace::IndexSetType> >
    FineEntitySetType;
  typedef std::unordered_map<DomainEntityPointerType,
                             FineEntitySetType,
                             EntityPointerHash<typename DomainSpace::IndexSetType> >
    SupportMapType;
  //! maps each DomainEntity to its support set
  SupportMapType support_map_;

public:
  //! creates an entity/neighbor pattern with domainSpace.size() == #rows sets
  ClemementPattern(const DomainSpace& domainSpace,
                   const RangeSpace& rangeSpace,
                   const MacroMicroGridSpecifier& specifier)
      : BaseType(domainSpace.size())
      , support_map_(domainSpace.gridPart().grid().size(0),
                     typename SupportMapType::hasher(domainSpace.indexSet()))
  {
      for (const auto& domain_entity : domainSpace)
      {
          const auto globalI_vec = mapEach(domainSpace, domain_entity);
          FineEntitySetType range_set(specifier.getLevelDifference()*3,
                                      typename FineEntitySetType::hasher(rangeSpace.indexSet()));
          const auto father_of_loc_grid_ent =
                  Stuff::Grid::make_father(rangeSpace.gridPart().grid().leafIndexSet(),
                                           domainSpace.grid().template getHostEntity< 0 >(domain_entity),
                                           specifier.getLevelDifference());
          for(const auto& range_entity : rangeSpace)
          {
              if (!Stuff::Grid::entities_identical(range_entity, *father_of_loc_grid_ent))
                  continue;
              range_set.insert(RangeEntityPointerType(range_entity));
              for (const auto i : globalI_vec) {
                  const auto globalJ_vec = mapEach(rangeSpace,range_entity);
                  auto& columns = BaseType::inner(i);
                  for (const auto j : globalJ_vec) {
                      columns.insert(j);
                  }
              }
          }
          support_map_.insert(std::make_pair(DomainEntityPointerType(domain_entity), range_set));
      }
  }

  const SupportMapType& support() const
  {
      return support_map_;
  }
};


} //namespace MsFEM {
} //namespace Multiscale {
} //namespace Dune {

#endif // DUNE_MULTISCALE_CLEMENT_PATTERN_HH
