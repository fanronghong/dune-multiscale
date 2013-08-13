// dune-multiscale
// Copyright Holders: Patrick Henning, Rene Milk
// License: BSD 2-Clause License (http://opensource.org/licenses/BSD-2-Clause)

#ifndef Elliptic_LOD_Solver_HH
#define Elliptic_LOD_Solver_HH

#ifdef HAVE_CMAKE_CONFIG
  #include "cmake_config.h"
#else
  #include "config.h"
#endif // ifdef HAVE_CMAKE_CONFIG

#include <dune/common/fmatrix.hh>

#include <dune/fem/solver/oemsolver/oemsolver.hh>
#include <dune/fem/operator/discreteoperatorimp.hh>
#include <dune/fem/operator/2order/lagrangematrixsetup.hh>
#include <dune/fem/operator/matrix/spmatrix.hh>
#include <dune/fem/space/common/adaptmanager.hh>


#include <dune/istl/matrix.hh>
#include <dune/stuff/fem/functions/checks.hh>
#include <dune/stuff/common/logging.hh>
#include <dune/stuff/function/interface.hh>

#include <dune/multiscale/common/traits.hh>
#include <dune/multiscale/lod/lod_traits.hh>
#include <dune/multiscale/problems/base.hh>
#include <dune/multiscale/common/righthandside_assembler.hh>

namespace Dune {
namespace Multiscale {
namespace MsFEM {


class Elliptic_Rigorous_MsFEM_Solver
{
private:
  typedef CommonTraits::DiscreteFunctionType DiscreteFunctionType;
  typedef DiscreteFunctionType DiscreteFunction;

  typedef typename DiscreteFunction::FunctionSpaceType FunctionSpace;

  typedef typename DiscreteFunction::DiscreteFunctionSpaceType DiscreteFunctionSpace;

  typedef typename DiscreteFunction::LocalFunctionType LocalFunction;

  typedef typename DiscreteFunctionSpace::LagrangePointSetType LagrangePointSet;

  typedef typename DiscreteFunctionSpace::GridPartType GridPart;

  typedef Fem::CachingQuadrature< GridPart, 0 > CoarseQuadrature;
  typedef Fem::CachingQuadrature< GridPart, 1 > HostFaceQuadrature;

  typedef typename DiscreteFunctionSpace::GridType HostGrid;

  typedef typename HostGrid::Traits::LeafIndexSet HostGridLeafIndexSet;

  typedef typename HostGrid::Traits::LeafIndexSet CoarseGridLeafIndexSet;

  typedef typename DiscreteFunctionSpace::DomainType        DomainType;
  typedef typename DiscreteFunctionSpace::RangeType         RangeType;
  typedef typename DiscreteFunctionSpace::JacobianRangeType JacobianRangeType;

  static const int dimension = GridPart::GridType::dimension;
  
  class OrderedDomainType
  : public DomainType
  {
    public:
  
    OrderedDomainType ( DomainType point ) 
    : DomainType( point )
    {   
    }

    const bool operator< ( OrderedDomainType point_2 ) const
    {

      for (int i = 0; i < dimension; ++i)
      {
        if ( (*this)[i] < point_2[i] ) 
          return true;
        if ( (*this)[i] > point_2[i] ) 
          return false;
      }

      return false;
    }

    const bool operator<= ( OrderedDomainType point_2 ) const
    {
      for (int i = 0; i < dimension; ++i)
      {
        if ( (*this)[i] < point_2[i] ) 
          return true;
        if ( (*this)[i] > point_2[i] ) 
          return false;
      }
      return true;
    }
    
    const bool operator> ( OrderedDomainType point_2 ) const
    {
      for (int i = 0; i < dimension; ++i)
      {
        if ( (*this)[i] > point_2[i] ) 
          return true;
        if ( (*this)[i] < point_2[i] ) 
          return false;
      }
      return false;
    }
    
    const bool operator>= ( OrderedDomainType point_2 ) const
    {
      for (int i = 0; i < dimension; ++i)
      {
        if ( (*this)[i] > point_2[i] ) 
          return true;
        if ( (*this)[i] < point_2[i] ) 
          return false;
      }
      return true;
    }
    
    const bool operator== ( OrderedDomainType point_2 ) const
    {
      for (int i = 0; i < dimension; ++i)
      {
        if ( (*this)[i] > point_2[i] ) 
          return false;
        if ( (*this)[i] < point_2[i] ) 
          return false;
      }
      return true;
    }

  };
  
  // typedef typename HostGrid ::template Codim< 0 > :: template Partition< All_Partition > :: LevelIterator
  // LevelEntityIteratorType;

  typedef typename DiscreteFunctionSpace::IteratorType HostgridIterator;
  typedef HostgridIterator CoarsegridIterator;
  
  typedef typename HostgridIterator::Entity HostEntity;

  typedef typename HostEntity::EntityPointer HostEntityPointer;
  
  typedef typename HostEntity::EntitySeed FineGridEntitySeed;

  // typedef typename HostGrid :: template Codim< 0 > :: template Partition< All_Partition > :: LevelIterator
  // HostGridLevelEntityIterator;

  enum { faceCodim = 1 };

  typedef typename GridPart::IntersectionIteratorType IntersectionIterator;

  // --------------------------- subgrid typedefs ------------------------------------
  typedef MsFEMTraits::SubGridListType SubGridListType;
  typedef MsFEMTraits::SubGridType  SubGridType;
  typedef MsFEMTraits::SubGridPartType SubGridPartType;
  typedef MsFEMTraits::SubGridDiscreteFunctionSpaceType SubGridDiscreteFunctionSpaceType;
  typedef MsFEMTraits::SubGridDiscreteFunctionType SubGridDiscreteFunctionType;
  typedef typename SubGridDiscreteFunctionType::LocalFunctionType SubGridLocalFunctionType;
  typedef typename SubGridDiscreteFunctionSpaceType::IteratorType SubGridIteratorType;
  typedef typename SubGridIteratorType::Entity SubGridEntityType;
  //!-----------------------------------------------------------------------------------------

  //! --------------------- istl matrix and vector types -------------------------------------

  typedef BlockVector< FieldVector< double, 1> > VectorType;
  typedef Matrix< FieldMatrix< double,1,1 > > MatrixType;
  typedef MatrixAdapter< MatrixType, VectorType, VectorType > MatrixOperatorType;
  //typedef SeqGS< MatrixType, VectorType, VectorType > PreconditionerType;
  typedef SeqSOR< MatrixType, VectorType, VectorType > PreconditionerType;
  //typedef BiCGSTABSolver< VectorType > SolverType;
  typedef InverseOperatorResult InverseOperatorResultType;

  typedef std::vector< shared_ptr<DiscreteFunction> > MsFEMBasisFunctionType;
  //! ----------------------------------------------------------------------------------------

  const DiscreteFunctionSpace& discreteFunctionSpace_;


public:
  Elliptic_Rigorous_MsFEM_Solver(const DiscreteFunctionSpace& discreteFunctionSpace);

private:

  //! vtk visualization of msfem basis functions
  void vtk_output( MsFEMBasisFunctionType& msfem_basis_function_list,
                   std::string basis_name = "msfem_basis_function" ) const;

   //! for each subgrid, store the vector of basis functions ids that
   //! correspond to interior coarse grid nodes in the subgrid
   // information stored in 'std::vector< std::vector< int > >'
   void assemble_interior_basis_ids( MacroMicroGridSpecifier& specifier,
                                     SubGridListType& subgrid_list,
                                     std::map<int,int>& global_id_to_internal_id,
                                     std::map< OrderedDomainType, int >& coordinates_to_global_coarse_node_id,
                                     std::vector< std::vector< int > >& ids_basis_function_in_extended_subgrid,
                                     std::vector< std::vector< int > >& ids_basis_function_in_subgrid,
                                     std::vector< std::vector< int > >& ids_basis_function_in_interior_subgrid) const;

   void subgrid_to_hostrid_projection( const SubGridDiscreteFunctionType& sub_func,
                                       DiscreteFunction& host_func) const;

  //! create standard coarse grid basis functions as discrete functions defined on the fine grid
  // ------------------------------------------------------------------------------------
  void add_coarse_basis_contribution( MacroMicroGridSpecifier& specifier,
                                      std::map<int,int>& global_id_to_internal_id,
                                      MsFEMBasisFunctionType& msfem_basis_function_list ) const;


  //! add corrector part to MsFEM basis functions
  void add_corrector_contribution(MacroMicroGridSpecifier &specifier,
                                   std::map<int,int>& global_id_to_internal_id,
                                   SubGridListType& subgrid_list,
                                   MsFEMBasisFunctionType& msfem_basis_function_list ) const;


  //! assemble global dirichlet corrector
  void assemble_global_dirichlet_corrector(MacroMicroGridSpecifier &specifier,
                                           MsFEMTraits::SubGridListType& subgrid_list,
                                           DiscreteFunction& global_dirichlet_corrector ) const;

  //! assemble global neumann corrector
  void assemble_global_neumann_corrector(MacroMicroGridSpecifier &specifier,
                                         MsFEMTraits::SubGridListType& subgrid_list,
                                         DiscreteFunction& global_neumann_corrector ) const;

  template< class DiffusionOperator, class SeedSupportStorage >
  RangeType evaluate_bilinear_form( const DiffusionOperator& diffusion_op,
                                    const DiscreteFunction& func1, const DiscreteFunction& func2,
                                    const SeedSupportStorage& support_of_ms_basis_func_intersection ) const
  {
    RangeType value = 0.0;

    const int polOrder = 2* DiscreteFunctionSpace::polynomialOrder + 2;
    for (const auto& support : support_of_ms_basis_func_intersection)
    {
      const auto it = discreteFunctionSpace_.grid().entityPointer(support);
 
      LocalFunction loc_func_1 = func1.localFunction(*it);
      LocalFunction loc_func_2 = func2.localFunction(*it);

      const auto& geometry = (*it).geometry();
 
      const Fem::CachingQuadrature< GridPart, 0 > quadrature( *it , polOrder);
      const int numQuadraturePoints = quadrature.nop();
      for (int quadraturePoint = 0; quadraturePoint < numQuadraturePoints; ++quadraturePoint)
      {
        DomainType global_point = geometry.global( quadrature.point(quadraturePoint) );

        //weight
        double weight = geometry.integrationElement( quadrature.point(quadraturePoint) );
        weight *= quadrature.weight(quadraturePoint);

        // gradients of func1 and func2
        JacobianRangeType grad_func_1, grad_func_2;
        loc_func_1.jacobian( quadrature[quadraturePoint], grad_func_1);
        loc_func_2.jacobian( quadrature[quadraturePoint], grad_func_2);

        // A \nabla func1
        JacobianRangeType diffusive_flux(0.0);
        diffusion_op.diffusiveFlux( global_point, grad_func_1, diffusive_flux);

        value += weight * ( diffusive_flux[0] * grad_func_2[0] );
      }
    }
    return value;
  }


  // ------------------------------------------------------------------------------------
  template< class DiffusionOperator, class MatrixImp, class SeedSupportStorageList, class RelevantConstellationsList >
  void assemble_matrix( const DiffusionOperator& diffusion_op,
                        MsFEMBasisFunctionType& msfem_basis_function_list_1,
                        MsFEMBasisFunctionType& msfem_basis_function_list_2,
                        SeedSupportStorageList& support_of_ms_basis_func_intersection,
                        RelevantConstellationsList& relevant_constellations,
                        MatrixImp& system_matrix ) const
  {
    for (size_t row = 0; row != system_matrix.N(); ++row)
      for (size_t col = 0; col != system_matrix.M(); ++col)
        system_matrix[row][col] = 0.0;

#ifdef SYMMETRIC_DIFFUSION_MATRIX

   for (unsigned int t = 0; t < relevant_constellations.size(); ++t)
   {
      unsigned int row = get<0>(relevant_constellations[t]);
      unsigned int col = get<1>(relevant_constellations[t]);
      system_matrix[row][col]
        = evaluate_bilinear_form( diffusion_op, *(msfem_basis_function_list_1[row]), *(msfem_basis_function_list_2[col]), support_of_ms_basis_func_intersection[row][col] );
   }

   /* old version without 'relevant_constellations'-vector
   for (size_t row = 0; row != system_matrix.N(); ++row)
    for (size_t col = 0; col <= row; ++col)    
      system_matrix[row][col]
        = evaluate_bilinear_form( diffusion_op, *(msfem_basis_function_list_1[row]), *(msfem_basis_function_list_2[col]), support_of_ms_basis_func_intersection[row][col] );
   */
	
   for (size_t col = 0; col != system_matrix.N(); ++col )
    for (size_t row = 0; row < col; ++row)
      system_matrix[row][col] = system_matrix[col][row];

#else
     
   for (unsigned int t = 0; t < relevant_constellations.size(); ++t)
   {
      unsigned int row = get<0>(relevant_constellations[t]);
      unsigned int col = get<1>(relevant_constellations[t]);
      system_matrix[row][col]
        = evaluate_bilinear_form( diffusion_op, *(msfem_basis_function_list_1[row]), *(msfem_basis_function_list_2[col]), support_of_ms_basis_func_intersection[row][col] );

      if ( row != col )
      { system_matrix[col][row]
        = evaluate_bilinear_form( diffusion_op, *(msfem_basis_function_list_1[col]), *(msfem_basis_function_list_2[row]), support_of_ms_basis_func_intersection[col][row] ); }
   }

#endif
  }



  // ------------------------------------------------------------------------------------
  template< class SeedSupportStorageList, class VectorImp >
  void assemble_rhs( const CommonTraits::FirstSourceType& f,
                     const CommonTraits::DiffusionType& diffusion_op,
                     const CommonTraits::DiscreteFunctionType& dirichlet_extension,
                     const CommonTraits::NeumannBCType& neumann_bc,
                     const DiscreteFunction& global_dirichlet_corrector,
                     const DiscreteFunction& global_neumann_corrector,
//                     const std::vector< FineGridEntitySeed >& support_global_dirichlet_corrector,
//                     const std::vector< FineGridEntitySeed >& support_global_neumann_corrector,
                     MsFEMBasisFunctionType& msfem_basis_function_list,
                     SeedSupportStorageList& support_of_ms_basis_func_intersection,
                     VectorImp& rhs ) const
  {

    for (size_t col = 0; col != rhs.N(); ++col)
      rhs[col] = 0.0;

    for (size_t col = 0; col != rhs.N(); ++col)
    {
      
      typedef typename HostEntity::template Codim< 0 >::EntityPointer
          HostEntityPointer;

      const int polOrder = 2* DiscreteFunctionSpace::polynomialOrder + 2;
      for (int it_id = 0; it_id < support_of_ms_basis_func_intersection[col][col].size(); ++it_id)
//      for (const auto& entity : discreteFunctionSpace_)
      {
        HostEntityPointer it = discreteFunctionSpace_.grid().entityPointer( support_of_ms_basis_func_intersection[col][col][it_id] );
        const HostEntity& entity = *it;

        const auto& geometry = entity.geometry();

        const auto local_func = msfem_basis_function_list[col]->localFunction(entity);

        for (const auto& intersection
         : Dune::Stuff::Common::intersectionRange(discreteFunctionSpace_.gridPart(), entity ))
        {
           if ( !intersection.boundary() )
             continue;
           // boundaryId 1 = Dirichlet face; boundaryId 2 = Neumann face;
           if ( intersection.boundary() && (intersection.boundaryId() != 2) )
             continue;

           const auto face = intersection.indexInInside();
           const HostFaceQuadrature faceQuadrature( discreteFunctionSpace_.gridPart(),
                                                    intersection, polOrder, HostFaceQuadrature::INSIDE );
           const int numFaceQuadraturePoints = faceQuadrature.nop();

           enum { faceCodim = 1 };
           for (int faceQuadraturePoint = 0; faceQuadraturePoint < numFaceQuadraturePoints; ++faceQuadraturePoint)
           {
             RangeType func_in_x;
             local_func.evaluate( faceQuadrature[faceQuadraturePoint], func_in_x );

             const auto local_point_entity = faceQuadrature.point( faceQuadraturePoint ); 
             const auto global_point = geometry.global( local_point_entity ); 
             const auto local_point_face = intersection.geometry().local( global_point );

             RangeType neumann_value( 0.0 );
             neumann_bc.evaluate( global_point, neumann_value );

             const double face_weight = intersection.geometry().integrationElement( local_point_face )
                          * faceQuadrature.weight( faceQuadraturePoint );

             rhs[col] += face_weight * ( func_in_x * neumann_value );
           }
        }

        const auto glob_dirichlet_corrector_localized = global_dirichlet_corrector.localFunction(entity);
        const auto glob_neumann_corrector_localized = global_neumann_corrector.localFunction(entity);
        const auto dirichlet_extension_localized = dirichlet_extension.localFunction(entity);

        const Fem::CachingQuadrature< GridPart, 0 > quadrature( entity, polOrder);
        const int numQuadraturePoints = quadrature.nop();
        for (int quadraturePoint = 0; quadraturePoint < numQuadraturePoints; ++quadraturePoint)
        {
          DomainType global_point = geometry.global( quadrature.point(quadraturePoint) );

          const double weight = geometry.integrationElement( quadrature.point(quadraturePoint) )
                                * quadrature.weight(quadraturePoint);

          // gradients of func1 and func2
          RangeType func_in_x;
          local_func.evaluate( quadrature[quadraturePoint], func_in_x );

          JacobianRangeType grad_func_in_x;
          local_func.jacobian( quadrature[quadraturePoint], grad_func_in_x );

          JacobianRangeType grad_global_dirichlet_corrector;
          glob_dirichlet_corrector_localized.jacobian( quadrature[quadraturePoint], grad_global_dirichlet_corrector );

          JacobianRangeType grad_global_neumann_corrector;
          glob_neumann_corrector_localized.jacobian( quadrature[quadraturePoint], grad_global_neumann_corrector );

          JacobianRangeType grad_dirichlet_extension;
          dirichlet_extension_localized.jacobian( quadrature[quadraturePoint], grad_dirichlet_extension );

          JacobianRangeType flux_direction;
          flux_direction[0] = grad_dirichlet_extension[0] + grad_global_dirichlet_corrector[0] - grad_global_neumann_corrector[0];

          JacobianRangeType diffusive_flux;
          diffusion_op.diffusiveFlux(global_point, flux_direction, diffusive_flux );

          RangeType f_x(0.0);
          f.evaluate( global_point, f_x);

          rhs[col] += weight * ( func_in_x * f_x );
          rhs[col] -= weight * ( grad_func_in_x[0] * diffusive_flux[0] );
        }
      }
    }
  }

public:
  //! - ∇ (A(x,∇u)) + b ∇u + c u = f - divG
  //! then:
  //! A --> diffusion operator ('DiffusionOperatorType')
  //! b --> advective part ('AdvectionTermType')
  //! c --> reaction part ('ReactionTermType')
  //! f --> 'first' source term, scalar ('SourceTermType')
  //! G --> 'second' source term, vector valued ('SecondSourceTermType')
  //! homogenous Dirchilet boundary condition!:
  void solve(const CommonTraits::DiffusionType& diffusion_op,
             const CommonTraits::FirstSourceType& f,
             const CommonTraits::DiscreteFunctionType& dirichlet_extension,
             const CommonTraits::NeumannBCType& neumann_bc,
                   // number of layers per coarse grid entity T:  U(T) is created by enrichting T with
                   // n(T)-layers.
                   MacroMicroGridSpecifier &specifier,
                   MsFEMTraits::SubGridListType& subgrid_list,
                   DiscreteFunction& coarse_scale_part,
                   DiscreteFunction& fine_scale_part,
                   DiscreteFunction& solution) const;
};

} //namespace MsFEM {
} //namespace Multiscale {
} //namespace Dune {

#endif // #ifndef Elliptic_LOD_Solver_HH