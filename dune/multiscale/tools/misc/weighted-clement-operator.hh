#ifndef DUNE_DIVERGENCE_HH
#define DUNE_DIVERGENCE_HH

//- Dune includes
#include <dune/common/fmatrix.hh>
#include <dune/common/timer.hh>

#include <dune/fem/storage/array.hh>
#include <dune/fem/quadrature/quadrature.hh>
#include <dune/fem/operator/common/operator.hh>
#include <dune/fem/operator/2order/lagrangematrixsetup.hh>


namespace Dune
{

  template< class DiscreteFunction, class CoarseDiscreteFunction, class MatrixTraits, class CoarseBasisFunctionList >
  class WeightedClementOp 
  : public Operator< typename DiscreteFunction :: RangeFieldType,
                     typename CoarseDiscreteFunction :: RangeFieldType,
                     DiscreteFunction,
                     CoarseDiscreteFunction >,
    public OEMSolver::PreconditionInterface                         
  {                                                                 /*@LST0E@*/
    // needs to be friend for conversion check 
    friend class Conversion<ThisType,OEMSolver::PreconditionInterface>;
    
  private:
    //! type of discrete functions
    typedef DiscreteFunction DiscreteFunctionType;
    
    typedef CoarseDiscreteFunction CoarseDiscreteFunctionType;

    //! type of this LaplaceFEOp
    typedef WeightedClementOp< DiscreteFunctionType, CoarseDiscreteFunction, MatrixTraits, CoarseBasisFunctionList > WeightedClementOpType;

    typedef WeightedClementOpType ThisType;

    //! type of discrete function space
    typedef typename DiscreteFunctionType :: DiscreteFunctionSpaceType
      DiscreteFunctionSpaceType;
    
    typedef typename CoarseDiscreteFunction :: DiscreteFunctionSpaceType
      CoarseDiscreteFunctionSpaceType;
      
    //! field type of range
    typedef typename DiscreteFunctionSpaceType :: RangeType
      RangeType;
      
    typedef typename DiscreteFunctionSpaceType :: DomainType
      DomainType;
      
    //! field type of range
    typedef typename DiscreteFunctionSpaceType :: RangeFieldType
      RangeFieldType;
       
    //! type of grid partition
    typedef typename DiscreteFunctionSpaceType :: GridPartType GridPartType;
    //! type of grid
    typedef typename DiscreteFunctionSpaceType :: GridType GridType;

    typedef typename CoarseDiscreteFunctionSpaceType :: GridPartType CoarseGridPartType;
    
    //! type of jacobian
    typedef typename DiscreteFunctionSpaceType :: JacobianRangeType
      JacobianRangeType;
    //! type of the base function set
    typedef typename DiscreteFunctionSpaceType :: BaseFunctionSetType
      BaseFunctionSetType;
    
    typedef typename CoarseDiscreteFunctionSpaceType :: BaseFunctionSetType
      CoarseBaseFunctionSetType;
      
    enum{dimRange = GridType::dimension};

    //! polynomial order of base functions
    enum { polynomialOrder = DiscreteFunctionSpaceType :: polynomialOrder };

    //! The grid's dimension
    enum { dimension = GridType :: dimension };
        
    //! type of quadrature to be used
    typedef CachingQuadrature< GridPartType, 0 > QuadratureType;
    typedef CachingQuadrature< CoarseGridPartType, 0 > CoarseQuadratureType;

    typedef typename MatrixTraits
      :: template  MatrixObject< LagrangeMatrixTraits< MatrixTraits > >
      :: MatrixObjectType LinearOperatorType;

    //! get important types from the MatrixObject 
    typedef typename LinearOperatorType :: LocalMatrixType LocalMatrixType;
    typedef typename LinearOperatorType :: PreconditionMatrixType PreconditionMatrixType;
    typedef typename LinearOperatorType :: MatrixType MatrixType;
    typedef typename DiscreteFunctionSpaceType :: MapperType MapperType;
    typedef std::vector<DomainType> CoarseNodeVectorType;

    // type of DofManager
    typedef DofManager< GridType > DofManagerType;

   
  public:
    //! constructor 
    explicit WeightedClementOp( const DiscreteFunctionSpaceType& space,
                                const CoarseDiscreteFunctionSpaceType& coarse_space,
                                const CoarseNodeVectorType& coarse_nodes,
                                const CoarseBasisFunctionList& coarse_basis,
                                const std::map<int,int>& global_id_to_internal_id,
                                const MacroMicroGridSpecifier< CoarseDiscreteFunctionSpaceType >& specifier )
    : discreteFunctionSpace_( space ),
      coarse_space_( coarse_space ),
      coarse_nodes_( coarse_nodes ),
      coarse_basis_( coarse_basis ),
      global_id_to_internal_id_( global_id_to_internal_id ),
      specifier_( specifier ),
      dofManager_( DofManagerType :: instance( space.grid() ) ),
      linearOperator_( discreteFunctionSpace_, coarse_space_ ),
      sequence_( -1 ),
      gradCache_( discreteFunctionSpace_.mapper().maxNumDofs() ),
      values_( discreteFunctionSpace_.mapper().maxNumDofs() )
    {
    }
        
  private:
    // prohibit copying
    WeightedClementOp ( const ThisType & ) = delete;

  public:                                                           /*@LST0S@*/
    //! apply the operator
    virtual void operator() ( const DiscreteFunctionType &u, 
                              CoarseDiscreteFunctionType &w ) const 
    {
      systemMatrix().apply( u, w );                                 /*@\label{sto:matrixEval}@*/
    }                                                               /*@LST0E@*/
  
    //! return reference to preconditioning matrix, used by OEM-Solver
    const PreconditionMatrixType &preconditionMatrix () const
    {
      return systemMatrix().preconditionMatrix();
    }
                                                                    /*@LST0S@*/
    virtual void applyTransposed ( const CoarseDiscreteFunctionType &u,
                                   DiscreteFunctionType &w) const 
    {
      systemMatrix().apply_t(u,w);                /*@\label{sto:applytransposed}@*/
    }                                                             /*@LST0E@*/

    //! return true if preconditioning is enabled
    bool hasPreconditionMatrix () const
    {
      return linearOperator_.hasPreconditionMatrix();
    }

    //! print the system matrix into a stream
    void print ( std :: ostream & out = std :: cout ) const 
    {
      systemMatrix().matrix().print( out );
    }

    //! return reference to discreteFunctionSpace
    const DiscreteFunctionSpaceType &discreteFunctionSpace () const
    {
      return discreteFunctionSpace_;
    }

    /*! \brief obtain a reference to the system matrix
     *
     *  The assembled matrix is returned. If the system matrix has not been
     *  assembled, yet, the assembly is performed.
     *
     *  \returns a reference to the system matrix
     */
    LinearOperatorType &systemMatrix () const                        /*@LST0S@*/
    {
      // if stored sequence number it not equal to the one of the
      // dofManager (or space) then the grid has been changed
      // and matrix has to be assembled new
      if( sequence_ != dofManager_.sequence() )                     /*@\label{sto:sequence}@*/
        assemble();

      return linearOperator_;
    }    

    /** \brief perform a grid walkthrough and assemble the global matrix */
    
    void assemble ()  const
    {
      const DiscreteFunctionSpaceType &space = discreteFunctionSpace();

      // reserve memory for matrix 
      linearOperator_.reserve();                                   /*@LST0E@*/

      // create timer (also stops time)
      Timer timer;

      // clear matrix                                             /*@LST0S@*/ 
      linearOperator_.clear();

      typedef typename DiscreteFunctionSpaceType :: IteratorType IteratorType;
      typedef typename CoarseDiscreteFunctionSpaceType :: IteratorType CoarseIteratorType;
      
      typedef typename IteratorType :: Entity EntityType;
      typedef typename CoarseIteratorType :: Entity CoarseEntityType;
      
      typedef typename EntityType :: Geometry GeometryType;
      typedef typename CoarseEntityType :: Geometry CoarseGeometryType;

      // coefficients in the matrix that describes the weighted Clement interpolation
      std::vector<double> coff( coarse_space_.size() );
      for(int c = 0; c < coarse_space_.size(); ++c)
        coff[c] = 0;
           
      CoarseIteratorType coarse_end = coarse_space_.end();
      for(CoarseIteratorType it = coarse_space_.begin(); it != coarse_end; ++it)
      {

        CoarseEntityType& entity = *it;

        // cache geometry of entity 
        const CoarseGeometryType coarse_geometry = entity.geometry();

        assert(entity.partitionType() == InteriorEntity);

        std::vector< RangeType > phi( coarse_space_.mapper().maxNumDofs() );

        // get base function set 
        const CoarseBaseFunctionSetType &coarse_baseSet = coarse_space_.baseFunctionSet( entity );
        const auto numBaseFunctions = coarse_baseSet.size();

        // create quadrature of appropriate order 
        CoarseQuadratureType quadrature( entity, 2 * polynomialOrder + 2 );
      
        // loop over all quadrature points
        const size_t numQuadraturePoints = quadrature.nop();
        for (size_t quadraturePoint = 0; quadraturePoint < numQuadraturePoints; ++quadraturePoint)
        {
          const typename CoarseQuadratureType::CoordinateType& local_point = quadrature.point(quadraturePoint);

          const double weight = quadrature.weight(quadraturePoint) * coarse_geometry.integrationElement(local_point);

          coarse_baseSet.evaluateAll( quadrature[quadraturePoint], phi );

          for (unsigned int i = 0; i < numBaseFunctions; ++i)
           {
             int global_dof_number = coarse_space_.mapToGlobal( entity, i );
             coff[ global_dof_number ] += weight * phi[i];
           }

	}

      }

      for ( size_t c = 0; c < coff.size(); ++c )
      {
        if ( coff[c] != 0.0 )
          coff[c] = 1.0 / coff[c];
      }

      for(CoarseIteratorType coarse_it = coarse_space_.begin(); coarse_it != coarse_end; ++coarse_it)
      {
         CoarseEntityType& coarse_entity = *coarse_it;
         const auto& coarseGridLeafIndexSet = coarse_space_.gridPart().grid().leafIndexSet();
  
         IteratorType end = space.end();
         for(IteratorType it = space.begin(); it != end; ++it)
         {
            EntityType& entity = *it;

            auto father_of_loc_grid_ent =
              Stuff::Grid::make_father(coarseGridLeafIndexSet,
                                       space.grid().template getHostEntity< 0 >(entity),
                                       specifier_.getLevelDifference());
            if (!Stuff::Grid::entities_identical(coarse_entity, *father_of_loc_grid_ent))
              continue;

            LocalMatrixType localMatrix = linearOperator_.localMatrix( entity, coarse_entity );

            const CoarseGeometryType coarse_geometry = coarse_entity.geometry();
	       
            // get base function set 
            const CoarseBaseFunctionSetType &coarse_baseSet = coarse_space_.baseFunctionSet( coarse_entity );
            const auto coarse_numBaseFunctions = coarse_baseSet.size();

            const auto& coarse_lagrangepoint_set = specifier_.coarseSpace().lagrangePointSet(coarse_entity);
      
            // only implemented for 3 Lagrange Points, i.e. piecewise linear functions
            assert( coarse_numBaseFunctions == 3 );
            std::vector< RangeType > coarse_phi_corner_0( coarse_numBaseFunctions );
            std::vector< RangeType > coarse_phi_corner_1( coarse_numBaseFunctions );
            std::vector< RangeType > coarse_phi_corner_2( coarse_numBaseFunctions );
	    
            std::vector< DomainType > coarse_corners( coarse_numBaseFunctions );

	    // coarse_corner_phi_j[i] = coarse basis function i evaluated in corner j
            coarse_baseSet.evaluateAll( coarse_lagrangepoint_set.point( 0 ) , coarse_phi_corner_0 );
            coarse_baseSet.evaluateAll( coarse_lagrangepoint_set.point( 1 ) , coarse_phi_corner_1 );
            coarse_baseSet.evaluateAll( coarse_lagrangepoint_set.point( 2 ) , coarse_phi_corner_2 );
	    
            for(size_t loc_point = 0; loc_point < coarse_numBaseFunctions ; ++loc_point ) {
               coarse_corners[ loc_point ] = coarse_geometry.global(coarse_lagrangepoint_set.point( loc_point ) );
            }

            LinearLagrangeFunction2D< DiscreteFunctionSpaceType > coarse_basis_interpolation_0
               ( coarse_corners[0], coarse_phi_corner_0[0], coarse_corners[1], coarse_phi_corner_1[0], coarse_corners[2], coarse_phi_corner_2[0] );

            LinearLagrangeFunction2D< DiscreteFunctionSpaceType > coarse_basis_interpolation_1
               ( coarse_corners[0], coarse_phi_corner_0[1], coarse_corners[1], coarse_phi_corner_1[1], coarse_corners[2], coarse_phi_corner_2[1] );

            LinearLagrangeFunction2D< DiscreteFunctionSpaceType > coarse_basis_interpolation_2
               ( coarse_corners[0], coarse_phi_corner_0[2], coarse_corners[1], coarse_phi_corner_1[2], coarse_corners[2], coarse_phi_corner_2[2] );
 
            // cache geometry of entity 
            const GeometryType geometry = entity.geometry();

            std::vector< RangeType > fine_phi( space.mapper().maxNumDofs() );

            // get base function set 
            const BaseFunctionSetType &baseSet = space.baseFunctionSet( entity );
            const auto numBaseFunctions = baseSet.size();
        
            // create quadrature of appropriate order 
            QuadratureType quadrature( entity, 2 * polynomialOrder + 2 );

            // loop over all quadrature points
            const size_t numQuadraturePoints = quadrature.nop();
            for (size_t quadraturePoint = 0; quadraturePoint < numQuadraturePoints; ++quadraturePoint)
            {
               const typename QuadratureType::CoordinateType& local_point = quadrature.point(quadraturePoint);
               DomainType global_point = geometry.global( quadrature.point(quadraturePoint) );

               const double weight = quadrature.weight(quadraturePoint) * geometry.integrationElement(local_point);

               baseSet.evaluateAll( quadrature[quadraturePoint], fine_phi );

               for (unsigned int j = 0; j < numBaseFunctions; ++j)
               {
                 for (unsigned int i = 0; i < coarse_numBaseFunctions; ++i)
                 {
                    int coarse_global_dof_number = coarse_space_.mapToGlobal( coarse_entity, i );
                    if ( specifier_.is_coarse_boundary_node( coarse_global_dof_number ) == true )
                      { continue; }

                    RangeType coarse_phi_i( 0.0 );

                    if (i == 0)
                      coarse_basis_interpolation_0.evaluate(global_point, coarse_phi_i);
                    if (i == 1)
                      coarse_basis_interpolation_1.evaluate(global_point, coarse_phi_i);
                    if (i == 2)
                      coarse_basis_interpolation_2.evaluate(global_point, coarse_phi_i);

                    localMatrix.add( i, j, weight * coff[coarse_global_dof_number] * coarse_phi_i * fine_phi[j] );
                 }
               }
            }

         }
      }

      // get elapsed time 
      const double assemblyTime = timer.elapsed();
      // in verbose mode print times 
      if ( Parameter :: verbose () )
        std :: cout << "Time to assemble weighted clement operator: " << assemblyTime << "s" << std :: endl;

      // get grid sequence number from space (for adaptive runs)    /*@LST0S@*/
      sequence_ = dofManager_.sequence();

    }

  protected:
    const DiscreteFunctionSpaceType &discreteFunctionSpace_;
    const CoarseDiscreteFunctionSpaceType& coarse_space_;
    const DofManagerType &dofManager_;

    //! pointer to the system matrix
    mutable LinearOperatorType linearOperator_;
    
    const CoarseNodeVectorType& coarse_nodes_;
    const CoarseBasisFunctionList& coarse_basis_;
    const std::map<int,int>& global_id_to_internal_id_;
 
    const MacroMicroGridSpecifier< CoarseDiscreteFunctionSpaceType >& specifier_;
    
    //! flag indicating whether the system matrix has been assembled
    mutable int sequence_;
      
    mutable Fem :: DynamicArray< JacobianRangeType > gradCache_;
    mutable Fem :: DynamicArray< RangeType > values_;
    mutable RangeFieldType weight_;    
  };                                                                  /*@LST0S@*//*@LST0E@*/

} // end namespace 
#endif
