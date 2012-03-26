#ifndef Elliptic_MSEM_Solver_HH
#define Elliptic_MSEM_Solver_HH

#include <dune/common/fmatrix.hh>

#include <dune/subgrid/subgrid.hh>


#include <dune/fem/solver/oemsolver/oemsolver.hh>
#include <dune/fem/solver/inverseoperators.hh>
#include <dune/fem/operator/discreteoperatorimp.hh>

#include <dune/fem/operator/2order/lagrangematrixsetup.hh>
#include <dune/fem/operator/matrix/spmatrix.hh>

#include <dune/multiscale/tools/assembler/righthandside_assembler.hh>
#include <dune/multiscale/tools/solver/MsFEM/msfem_localproblems/subgrid-list.hh>
#include <dune/multiscale/tools/assembler/matrix_assembler/elliptic_msfem_matrix_assembler.hh>

namespace Dune
{

  template< class DiscreteFunctionType >
  class Elliptic_MsFEM_Solver
  {

  public:

   typedef DiscreteFunctionType  DiscreteFunction;

   typedef typename DiscreteFunction :: FunctionSpaceType FunctionSpace;

   typedef typename DiscreteFunction :: DiscreteFunctionSpaceType DiscreteFunctionSpace;

   typedef typename DiscreteFunction :: LocalFunctionType LocalFunction;

   typedef typename DiscreteFunctionSpace :: LagrangePointSetType LagrangePointSet;

   typedef typename DiscreteFunctionSpace :: GridPartType GridPart;

   typedef typename DiscreteFunctionSpace :: GridType HostGrid;

   typedef typename HostGrid ::template Codim< 0 > :: template Partition< All_Partition > :: LevelIterator LevelEntityIteratorType;

   typedef typename DiscreteFunctionSpace :: IteratorType HostgridIterator;
   
   typedef typename HostgridIterator :: Entity HostEntity;
   
   typedef typename HostEntity :: EntityPointer HostEntityPointer; 

   
   enum { faceCodim = 1 };

   typedef typename GridPart :: IntersectionIteratorType IntersectionIterator;

   typedef typename LagrangePointSet :: template Codim< faceCodim > 
                                     :: SubEntityIteratorType
    FaceDofIterator;

   // --------------------------- subgrid typedefs ------------------------------------

   typedef SubGrid< HostGrid::dimension , HostGrid > SubGridType; 

   typedef LeafGridPart< SubGridType > SubGridPart;

   typedef LagrangeDiscreteFunctionSpace < FunctionSpace, SubGridPart, 1 > //1=POLORDER
      SubgridDiscreteFunctionSpace;

   typedef AdaptiveDiscreteFunction < SubgridDiscreteFunctionSpace > SubgridDiscreteFunction;
   
   typedef typename SubgridDiscreteFunctionSpace :: IteratorType CoarseGridIterator;

   typedef typename SubgridDiscreteFunction :: LocalFunctionType CoarseGridLocalFunction;
   
   typedef typename SubgridDiscreteFunctionSpace :: LagrangePointSetType
    CoarseGridLagrangePointSet;
   
   typedef typename CoarseGridLagrangePointSet :: template Codim< faceCodim > 
                                               :: SubEntityIteratorType
    CoarseGridFaceDofIterator;
//!-----------------------------------------------------------------------------------------


   //! --------------------- the standard matrix traits -------------------------------------

   struct MatrixTraits
   {
     typedef SubgridDiscreteFunctionSpace RowSpaceType;
     typedef SubgridDiscreteFunctionSpace ColumnSpaceType;
     typedef LagrangeMatrixSetup< false > StencilType;
     typedef ParallelScalarProduct< SubgridDiscreteFunctionSpace > ParallelScalarProductType;

     template< class M >
     struct Adapter
     {
       typedef LagrangeParallelMatrixAdapter< M > MatrixAdapterType;
     };
   };

   //! --------------------------------------------------------------------------------------


   //! --------------------- type of fem stiffness matrix -----------------------------------

   typedef SparseRowMatrixOperator< SubgridDiscreteFunction, SubgridDiscreteFunction, MatrixTraits > MsFEMMatrix;

   //! --------------------------------------------------------------------------------------


   //! --------------- solver for the linear system of equations ----------------------------

   // use Bi CG Stab [OEMBICGSTABOp] or GMRES [OEMGMRESOp] for non-symmetric matrices and CG [CGInverseOp] for symmetric ones. GMRES seems to be more stable, but is extremely slow!
   typedef OEMBICGSQOp/*OEMBICGSTABOp*/< SubgridDiscreteFunction, MsFEMMatrix > InverseMsFEMMatrix;

   //! --------------------------------------------------------------------------------------


  private:
    const DiscreteFunctionSpace &discreteFunctionSpace_;

    std :: ofstream *data_file_;
    
    // path where to save the data output
    std :: string path_;

    
  public:
   Elliptic_MsFEM_Solver( const DiscreteFunctionSpace &discreteFunctionSpace, std :: string path = "" )
     : discreteFunctionSpace_( discreteFunctionSpace ),
       data_file_( NULL )
     { path_ = path; }

   Elliptic_MsFEM_Solver( const DiscreteFunctionSpace &discreteFunctionSpace, std :: ofstream& data_file, std :: string path = "" )
     : discreteFunctionSpace_( discreteFunctionSpace ),
       data_file_( &data_file )
     { path_ = path; }
     
   template < class Stream >
   void oneLinePrint( Stream& stream, const DiscreteFunction& func )
    {
      typedef typename DiscreteFunction::ConstDofIteratorType
         DofIteratorType;
      DofIteratorType it = func.dbegin();
      stream << "\n" << func.name() << ": [ ";
      for ( ; it != func.dend(); ++it )
         stream << std::setw(5) << *it << "  ";

      stream << " ] " << std::endl;
     }


   // - ∇ (A(x,∇u)) + b ∇u + c u = f - divG
   // then:
   // A --> diffusion operator ('DiffusionOperatorType')
   // b --> advective part ('AdvectionTermType')
   // c --> reaction part ('ReactionTermType')
   // f --> 'first' source term, scalar ('SourceTermType')
   // G --> 'second' source term, vector valued ('SecondSourceTermType')

   // homogenous Dirchilet boundary condition!:
   template< class DiffusionOperator, class SourceTerm >
   void solve_dirichlet_zero( const DiffusionOperator &diffusion_op,
                              const SourceTerm &f,
                              const int coarse_level,
                                    DiscreteFunction &solution )
   {
     HostGrid &grid = discreteFunctionSpace_.gridPart().grid();
     const GridPart &gridPart = discreteFunctionSpace_.gridPart();
     int number_of_level_host_entities = grid.size( coarse_level, 0 /*codim*/ );
     
     //! default - no layers
     // number of layers per coarse grid entity T:  U(T) is created by enrichting T with n(T)-layers.
     std :: vector < int > number_of_layers( number_of_level_host_entities );
     for ( int i = 0; i < number_of_level_host_entities; i+=1 )
        { number_of_layers[i] = 0; }
     
     solve_dirichlet_zero( diffusion_op, f, coarse_level, number_of_layers, solution );
   }
   
   
   // homogenous Dirchilet boundary condition!:
   template< class DiffusionOperator, class SourceTerm >
   void solve_dirichlet_zero( const DiffusionOperator &diffusion_op,
                              const SourceTerm &f,
                              const int coarse_level,
			       // number of layers per coarse grid entity T:  U(T) is created by enrichting T with n(T)-layers.
			       std :: vector < int >& number_of_layers,
                              DiscreteFunction &solution )
   {

     // discrete elliptic MsFEM operator (corresponds with MsFEM Matrix)
     typedef DiscreteEllipticMsFEMOperator< SubgridDiscreteFunction,
                                            DiscreteFunction,
					     DiffusionOperator > EllipticMsFEMOperatorType;

     HostGrid &grid = discreteFunctionSpace_.gridPart().grid();
     const GridPart &gridPart = discreteFunctionSpace_.gridPart();

     LevelEntityIteratorType coarse_level_it = grid.template lbegin< 0 >( coarse_level );

     // create subgrid:
     SubGridType subGrid( grid );
     subGrid.createBegin();

     for( ; coarse_level_it != grid.template lend< 0 >( coarse_level ); ++coarse_level_it )
         subGrid.insertPartial( *coarse_level_it );

     subGrid.createEnd();

     subGrid.report();
     
     SubGridPart subGridPart( subGrid );

     SubgridDiscreteFunctionSpace coarseDiscreteFunctionSpace( subGridPart );

     SubgridDiscreteFunction coarse_msfem_solution( "Coarse Part MsFEM Solution", coarseDiscreteFunctionSpace );
     coarse_msfem_solution.clear();

     //! create subgrids:
     bool silence = false;

     const int coarse_level = coarseDiscreteFunctionSpace_.gridPart().grid().maxLevel();

     typedef SubGridList< DiscreteFunction, SubGridType > SubGridListType;
     SubGridListType subgrid_list_( discreteFunctionSpace , number_of_layers, coarse_level , silence );


     //! define the right hand side assembler tool
     // (for linear and non-linear elliptic and parabolic problems, for sources f and/or G )
     RightHandSideAssembler< SubgridDiscreteFunction > rhsassembler;

     //! define the discrete (elliptic) operator that describes our problem
     // ( effect of the discretized differential operator on a certain discrete function )
     EllipticMsFEMOperatorType elliptic_msfem_op( coarseDiscreteFunctionSpace,
                                                  discreteFunctionSpace_, number_of_layers,
                                                  diffusion_op, *data_file_, path_ );
     // discrete elliptic operator (corresponds with FEM Matrix)

     //! (stiffness) matrix
     MsFEMMatrix msfem_matrix( "MsFEM stiffness matrix", coarseDiscreteFunctionSpace, coarseDiscreteFunctionSpace );
     
     //! right hand side vector
     // right hand side for the finite element method:
     SubgridDiscreteFunction msfem_rhs( "MsFEM right hand side", coarseDiscreteFunctionSpace );
     msfem_rhs.clear();

     std :: cout << "Solving MsFEM problem." << std :: endl;

     if ( data_file_ )
      {
        if (data_file_->is_open())
         {
           *data_file_ << "Solving linear problem with MsFEM and coarse grid level " << coarse_level << "." << std :: endl;
           *data_file_ << "------------------------------------------------------------------------------" << std :: endl;
         }
      }
      
     // to assemble the computational time
     Dune::Timer assembleTimer;

     // assemble the MsFEM stiffness matrix
     elliptic_msfem_op.assemble_matrix( msfem_matrix ); 
     
     std::cout << "Time to assemble MsFEM stiffness matrix: " << assembleTimer.elapsed() << "s" << std::endl;

     if ( data_file_ )
      {
        if (data_file_->is_open())
         {
           *data_file_ << "Time to assemble MsFEM stiffness matrix: " << assembleTimer.elapsed() << "s" << std::endl;
         }
      }
      
     // assemble right hand side
     rhsassembler.template assemble< 2 * SubgridDiscreteFunctionSpace :: polynomialOrder + 2 >( f , msfem_rhs);

     //oneLinePrint( std::cout , fem_rhs );
    
     
     // --- boundary treatment ---
     // set the dirichlet points to zero (in righ hand side of the fem problem)
     CoarseGridIterator endit = coarseDiscreteFunctionSpace.end();
     for( CoarseGridIterator it = coarseDiscreteFunctionSpace.begin(); it != endit; ++it )
       {

         HostEntityPointer host_entity_pointer = subGrid.template getHostEntity<0>( *it );
         const HostEntity& host_entity = *host_entity_pointer;

         IntersectionIterator iit = gridPart.ibegin( host_entity );
         const IntersectionIterator endiit = gridPart.iend( host_entity );
         for( ; iit != endiit; ++iit )
           {

              if( !(*iit).boundary() )
                continue;

              CoarseGridLocalFunction rhsLocal = msfem_rhs.localFunction( *it );
	      
              const CoarseGridLagrangePointSet &lagrangePointSet
                = coarseDiscreteFunctionSpace.lagrangePointSet( *it );

              const int face = (*iit).indexInInside();

              CoarseGridFaceDofIterator faceIterator
                = lagrangePointSet.template beginSubEntity< faceCodim >( face );
              const CoarseGridFaceDofIterator faceEndIterator
                = lagrangePointSet.template endSubEntity< faceCodim >( face );
              for( ; faceIterator != faceEndIterator; ++faceIterator )
                rhsLocal[ *faceIterator ] = 0;

           }

       }
     // --- end boundary treatment ---

     InverseMsFEMMatrix msfem_biCGStab( msfem_matrix, 1e-8, 1e-8, 20000, VERBOSE );
     msfem_biCGStab( msfem_rhs, coarse_msfem_solution );

     if ( data_file_ )
      {
        if (data_file_->is_open())
         {
           *data_file_ << "---------------------------------------------------------------------------------" << std :: endl;
           *data_file_ << "MsFEM problem solved in " << assembleTimer.elapsed() << "s." << std :: endl << std :: endl << std :: endl;
         }
      }      
      
      
     // oneLinePrint( std::cout , solution );

     // copy coarse grid function (defined on the subgrid) into a fine grid function
     solution.clear();

     for( CoarseGridIterator it = coarseDiscreteFunctionSpace.begin(); it != endit; ++it )
       {

         HostEntityPointer host_entity_pointer = subGrid.template getHostEntity<0>( *it );
         const HostEntity& host_entity = *host_entity_pointer;


         CoarseGridLocalFunction sub_loc_value = coarse_msfem_solution.localFunction( *it );
         LocalFunction host_loc_value = solution.localFunction( host_entity );

         const unsigned int numBaseFunctions = sub_loc_value.baseFunctionSet().numBaseFunctions();
         for( unsigned int i = 0; i < numBaseFunctions; ++i )
           {
             host_loc_value[ i ] = sub_loc_value[ i ];
           }

       }

     std :: cout << "Auf Grobskalen MsFEM Anteil noch Feinksalen MsFEM Anteil aufaddieren." << std :: endl << std :: endl;  
   }


   //! the following methods are not yet implemented, however note that the required tools are 
   //! already available via 'righthandside_assembler.hh' and 'elliptic_fem_matrix_assembler.hh'!

   template< class DiffusionOperatorType, class ReactionTermType, class SourceTermType >
   void solve( )
   {
     std :: cout << "No implemented!" << std :: endl;
   }


   template< class DiffusionOperatorType, class ReactionTermType, class SourceTermType, class SecondSourceTermType>
   void solve( )
   {
     std :: cout << "No implemented!" << std :: endl;
   }



  };



}

#endif // #ifndef Elliptic_MSEM_Solver_HH
