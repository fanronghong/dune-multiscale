#ifndef LOCALGRIDSEARCH_HH
#define LOCALGRIDSEARCH_HH

#include <dune/multiscale/msfem/msfem_traits.hh>
#include <dune/multiscale/msfem/localproblems/subgrid-list.hh>
#include <dune/stuff/grid/search.hh>
#include <dune/stuff/common/memory.hh>

namespace Dune {
namespace Multiscale {
namespace MsFEM {

//! given a Localgridlist, facilitate searching for evaluation points in a pseudo-hierachical manner
template <class GridViewImp>
class LocalGridSearch
    : public DSG::EntitySearchBase<GridViewImp>
{
  typedef GridViewImp LocalGridViewType;
  typedef DSG::EntitySearchBase<LocalGridViewType> BaseType;
  typedef GenericReferenceElements< typename  BaseType::LocalCoordinateType::value_type,
                                             BaseType::LocalCoordinateType::dimension > RefElementType;
  typedef typename LocalGridViewType::template Codim< 0 >::Iterator IteratorType;

  typedef typename BaseType::EntityType::EntityPointer EntityPointer;

  typedef DSG::EntityInlevelSearch<LocalGridViewType> PerGridSearchType;
  typedef CommonTraits::DiscreteFunctionSpaceType CoarseGridSpaceType;
  typedef typename CoarseGridSpaceType::GridType::Traits::LeafIndexSet::IndexType IndexType;

public:
  typedef typename BaseType::EntityPointerVectorType EntityPointerVectorType;

  LocalGridSearch(const CoarseGridSpaceType& space, const LocalGridList& gridlist);

  template < class PointContainerType >
  EntityPointerVectorType operator() (const PointContainerType& points);

  const EntityPointer& current_coarse_pointer() const;

  template< class PointIterator >
  bool covers_strict(const typename CoarseGridSpaceType::EntityType& coarse_entity, const PointIterator first, const PointIterator last) {
    const auto& reference_element = Stuff::Grid::reference_element(coarse_entity);
    const auto& coarse_geometry = coarse_entity.geometry();
    for(auto it = first; it != last; ++it) {
      if(!reference_element.checkInside(coarse_geometry.local(*it)))
        return false;
    }
    return true;
  }

private:
  const CoarseGridSpaceType& coarse_space_;
  const LocalGridList& gridlist_;
  std::map<IndexType, std::unique_ptr<PerGridSearchType>> coarse_searches_;
};

//! only returns a vector of entitypointers if all queried points lie within a single coarse cell
template <class GridViewImp>
template <class PointContainerType>
typename LocalGridSearch<GridViewImp>::EntityPointerVectorType LocalGridSearch<GridViewImp>::operator()(const PointContainerType& points)
{
  const auto count_nulls = [&](const typename EntityPointerVectorType::value_type& ptr){return ptr==nullptr;};
  //! \TODO potential speedup by caching last coarse_entity position instead fo restarting at front
  for(const auto& coarse_entity : coarse_space_) {
    const auto& localgrid = gridlist_.getSubGrid(coarse_entity);
    const auto& index_set = coarse_space_.gridPart().grid().leafIndexSet();
    const auto index = index_set.index(coarse_entity);
    auto& current_search_ptr = coarse_searches_[index];
    if(!current_search_ptr)
      current_search_ptr = DSC::make_unique<PerGridSearchType>(localgrid.leafView());
    if(covers_strict(coarse_entity, points.begin(), points.end())) {
      auto entity_ptrs = current_search_ptr->operator()(points);
      const auto null_count = std::count_if(entity_ptrs.begin(), entity_ptrs.end(), count_nulls);
      if(null_count == 0)
        return entity_ptrs;
    }
  }
  DUNE_THROW(InvalidStateException, "local grid search failed");
}

template <class GridViewImp>
LocalGridSearch<GridViewImp>::LocalGridSearch(const CoarseGridSpaceType& coarse_space, const LocalGridList& gridlist)
  : coarse_space_(coarse_space)
  , gridlist_(gridlist)
{}

} // namespace MsFEM {
} // namespace Multiscale {
} // namespace Dune {

#endif // LOCALGRIDSEARCH_HH
