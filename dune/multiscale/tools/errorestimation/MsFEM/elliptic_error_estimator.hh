#ifndef DUNE_MSFEM_ERRORESTIMATOR_HH
#define DUNE_MSFEM_ERRORESTIMATOR_HH

// where the quadratures are defined 
#include <dune/fem/quadrature/cachingquadrature.hh>


namespace Dune 
{


  template < class DiscreteFunctionImp,
             class DiffusionImp,
             class SourceImp, 
             class MacroMicroGridSpecifierImp,
             class SubGridListImp >
  class MsFEMErrorEstimator
  {

    typedef DiffusionImp DiffusionOperatorType;
    typedef SourceImp SourceType;
    typedef MacroMicroGridSpecifierImp MacroMicroGridSpecifierType;
    typedef SubGridListImp SubGridListType;
    
    //! Necessary typedefs for the DiscreteFunctionImp:

    typedef DiscreteFunctionImp DiscreteFunctionType;

    typedef typename DiscreteFunctionType      :: LocalFunctionType LocalFunctionType;
    typedef typename DiscreteFunctionType      :: DiscreteFunctionSpaceType DiscreteFunctionSpaceType;

    typedef typename DiscreteFunctionSpaceType :: RangeFieldType RangeFieldType;
    typedef typename DiscreteFunctionSpaceType :: DomainType DomainType;
    typedef typename DiscreteFunctionSpaceType :: RangeType RangeType;
    typedef typename DiscreteFunctionSpaceType :: JacobianRangeType
      JacobianRangeType;

    typedef typename DiscreteFunctionType      :: DofIteratorType   DofIteratorType;
    typedef typename DiscreteFunctionSpaceType :: GridPartType      GridPartType;
    typedef typename DiscreteFunctionSpaceType :: GridType          GridType;
    typedef typename DiscreteFunctionSpaceType :: IteratorType      IteratorType;

    typedef typename GridType :: Traits :: LeafIndexSet LeafIndexSetType;
    
    typedef typename GridPartType                  :: IntersectionIteratorType IntersectionIteratorType;
    typedef typename GridType :: template Codim<0> :: Entity                   EntityType; 
    typedef typename GridType :: template Codim<0> :: EntityPointer            EntityPointerType; 
    typedef typename GridType :: template Codim<0> :: Geometry                 EntityGeometryType; 
    typedef typename GridType :: template Codim<1> :: Geometry                 FaceGeometryType;
    typedef typename DiscreteFunctionSpaceType     :: BaseFunctionSetType      BaseFunctionSetType;

    typedef CachingQuadrature < GridPartType , 0 > EntityQuadratureType;
    typedef CachingQuadrature < GridPartType , 1 > FaceQuadratureType;

    enum { dimension = GridType :: dimension};
    enum { spacePolOrd = DiscreteFunctionSpaceType :: polynomialOrder }; 
    enum { maxnumOfBaseFct = 100 };

#if 0


  private:

    const PeriodicDiscreteFunctionSpaceType &periodicDiscreteFunctionSpace_;
    const DiscreteFunctionSpaceType &discreteFunctionSpace_;
    const DiscreteFunctionSpaceType &auxiliaryDiscreteFunctionSpace_;
    // an auxiliaryDiscreteFunctionSpace to get an Intersection Iterator for the periodicDiscreteFunctionSpace
    // (for the periodic grid partition there is no usable intersection iterator implemented, therefore we use the intersection iterator for the corresponding non-periodic grid partition (this not efficient and increases the estimated error, but it works)
#endif

    const DiscreteFunctionSpaceType& fineDiscreteFunctionSpace_;
    MacroMicroGridSpecifierType& specifier_;
    SubGridListType& subgrid_list_;
    const DiffusionOperatorType &diffusion_;
    const SourceType& f_;


  public:

    MsFEMErrorEstimator ( const DiscreteFunctionSpaceType &fineDiscreteFunctionSpace,
                          MacroMicroGridSpecifierType& specifier,
                          SubGridListType& subgrid_list,
                          const DiffusionOperatorType& diffusion,
                          const SourceType& f )
    : fineDiscreteFunctionSpace_( fineDiscreteFunctionSpace ),
      specifier_( specifier ),
      subgrid_list_( subgrid_list ),
      diffusion_( diffusion ),
      f_( f )
    {
    }




    //! method to get the local mesh size H of a coarse grid entity 'T'
    // works only for our 2D examples!!!! 
    RangeType get_coarse_grid_H( const EntityType &entity)
    {

      // entity_H means H (the diameter of the entity)
      RangeType entity_H = 0.0;

      const GridPartType &coarseGridPart = specifier_.coarseSpace().gridPart();

      // compute the size of the faces of the entities and selected the largest.
      IntersectionIteratorType endnit = coarseGridPart.iend(entity);
      for( IntersectionIteratorType nit = coarseGridPart.ibegin(entity); nit != endnit ; ++nit)
        {
          FaceQuadratureType innerFaceQuadrature( coarseGridPart, *nit, 0 , FaceQuadratureType::INSIDE);

          DomainType scaledOuterNormal =
               nit->integrationOuterNormal(innerFaceQuadrature.localPoint(0));

          // get 'volume' of the visited face (this only works because we do not have curved faces):
          RangeType visitedFaceVolume(0.0);
          for ( int k = 0; k < dimension; ++k ) 
                visitedFaceVolume += scaledOuterNormal[k] * scaledOuterNormal[k];
          visitedFaceVolume = sqrt(visitedFaceVolume);

          if (visitedFaceVolume > entity_H)
            entity_H = visitedFaceVolume;

        }

       return entity_H;

    }



    // for a coarse grid entity T:
    // return:  H_T^4 ||f||_{L^2(T)}^2
    RangeType indicator_f( const EntityType &entity )
    {

        // create quadrature for given geometry type 
        CachingQuadrature <GridPartType , 0 > entityQuadrature(entity, 2*spacePolOrd+2 ); 

        // get geoemetry of entity
        const EntityGeometryType& geometry = entity.geometry();

        RangeType H_T = get_coarse_grid_H(entity);

        RangeType y(0);
        RangeType local_indicator(0);

        const int quadratureNop = entityQuadrature.nop();
        for(int quadraturePoint = 0; quadraturePoint < quadratureNop; ++quadraturePoint)
         {
          const double weight = entityQuadrature.weight(quadraturePoint) * 
              geometry.integrationElement(entityQuadrature.point(quadraturePoint));

          f_.evaluate( geometry.global( entityQuadrature.point(quadraturePoint) ),y );
          y = y * y;

          local_indicator += weight * y;
         }

        local_indicator *= pow(H_T, 4.0);

        return local_indicator;
    }
    
#if 0

    // \eta_T^{app}
    RangeType indicator_app_1( const EntityType &entity,
                                     DiscreteFunctionType &u_H,
                               const PeriodicDiscreteFunctionType &corrector_u_H_on_entity )
     {

       RangeType local_indicator(0.0);

       Problem::ModelProblemData model_info;
       const double delta = model_info.getDelta();
       const double epsilon_estimated = model_info.getEpsilonEstimated();

       EntityQuadratureType entityQuadrature( entity , 0 ); // 0 = polynomial order
       // the global quadrature (quadrature on the macro element T)

       const EntityGeometryType& globalEntityGeometry = entity.geometry();

       const DomainType &x_T = globalEntityGeometry.global(entityQuadrature.point(0));

       const RangeType entityVolume = entityQuadrature.weight(0) * 
               globalEntityGeometry.integrationElement(entityQuadrature.point(0));

       // int_Y A_h^{\epsilon} - A^{\epsilob}
       RangeType difference[dimension];
       for( int k = 0; k < dimension; ++k )
         difference[k] = 0.0;

       // \nabla u_H(x_T)
       LocalFunctionType u_H_local = u_H.localFunction(entity);
       JacobianRangeType gradient_u_H(0.);
       u_H_local.jacobian(entityQuadrature[ 0 ], gradient_u_H);

       // iterator over the elements of the periodic micro grid:
       PeriodicIteratorType p_endit = periodicDiscreteFunctionSpace_.end();
       for( PeriodicIteratorType p_it = periodicDiscreteFunctionSpace_.begin(); p_it != p_endit; ++p_it )
        {

          const PeriodicEntityType &micro_entity = *p_it;

          int quadOrder = 2 * PeriodicDiscreteFunctionSpaceType :: polynomialOrder + 2;

          // two quadrature formulas ( A_h^{\eps}(y)=A^{\eps}(y_s) vs. A^{\eps}(y) )
          PeriodicEntityQuadratureType one_point_quadrature( micro_entity , 0 );
          PeriodicEntityQuadratureType high_order_quadrature( micro_entity , quadOrder );

          // Q_h(u_H)(x_T,y) on the micro entity:
          PeriodicLocalFunctionType loc_Q_u_H_x_T = corrector_u_H_on_entity.localFunction(micro_entity);
          JacobianRangeType gradient_Q_u_H_x_T(0.);
          loc_Q_u_H_x_T.jacobian(one_point_quadrature[ 0 ], gradient_Q_u_H_x_T);

          // S denotes the micro grid element (i.e. 'micro_entity')
          const PeriodicEntityGeometryType& geometry_S = micro_entity.geometry();
          // y_S denotes the barycenter of the micro grid element S:
          const DomainType &y_S = geometry_S.global(one_point_quadrature.point(0));

          // to evaluate A^{\epsilon}_h:
          DomainType globalPoint_center;
          JacobianRangeType direction_of_diffusion;
          for( int k = 0; k < dimension; ++k )
            {
             globalPoint_center[ k ] = x_T[ k ] + (delta * y_S[ k ]);
             direction_of_diffusion[ 0 ][ k ] = gradient_u_H[ 0 ][ k ] + gradient_Q_u_H_x_T[ 0 ][ k ];
            }

          JacobianRangeType diffusive_flux_y_S;
          diffusion_.diffusiveFlux( globalPoint_center,
                                    direction_of_diffusion,
                                    diffusive_flux_y_S );

          double cutting_function = 1.0;
          for( int k = 0; k < dimension; ++k )
            {
              // is the current quadrature point in the relevant cell?
              // Y = [ -0.5 , 0.5 ]^dimension
              if ( fabs(y_S[ k ]) > (0.5*(epsilon_estimated/delta)) )
                { cutting_function *= 0.0; }
            }

          const size_t numQuadraturePoints = high_order_quadrature.nop();
          for( size_t microQuadraturePoint = 0; microQuadraturePoint < numQuadraturePoints; ++microQuadraturePoint )
            {

              // local (barycentric) coordinates (with respect to entity)
              const typename PeriodicEntityQuadratureType::CoordinateType &y_barycentric = high_order_quadrature.point( microQuadraturePoint );

              // current quadrature point y in global coordinates
              DomainType y = geometry_S.global( y_barycentric );

              const double weight_micro_quadrature = high_order_quadrature.weight(  microQuadraturePoint ) * geometry_S.integrationElement( y_barycentric );

              // to evaluate A^{\epsilon}:
              DomainType globalPoint;
              for( int k = 0; k < dimension; ++k )
                globalPoint[ k ] = (delta * y[ k ]) + x_T[ k ];

              // diffusive flux in y (in direction \nabla u_H(x_T) + \nabla_y Q_h^T(u_H)(y_S) )
              JacobianRangeType diffusive_flux_y;
              diffusion_.diffusiveFlux( globalPoint,
                                        direction_of_diffusion,
                                        diffusive_flux_y );

              for( int k = 0; k < dimension; ++k )
                difference[ k ] += cutting_function * weight_micro_quadrature * ( diffusive_flux_y[ 0 ][ k ] - diffusive_flux_y_S[ 0 ][ k ] );

            }

        }

       for( int k = 0; k < dimension; ++k )
          local_indicator += pow( difference[ k ], 2.0 ) * entityVolume;

       return local_indicator;

     } // end of method


    // \bar{\eta}_T^{app}
    RangeType indicator_app_2( const EntityType &entity,
                                     DiscreteFunctionType &u_H,
                               const PeriodicDiscreteFunctionType &corrector_u_H_on_entity )
     {

       RangeType local_indicator(0.0);

       Problem::ModelProblemData model_info;
       const double delta = model_info.getDelta();
       const double epsilon_estimated = model_info.getEpsilonEstimated();

       EntityQuadratureType entityQuadrature( entity , 0 ); // 0 = polynomial order
       // the global quadrature (quadrature on the macro element T)

       const EntityGeometryType& globalEntityGeometry = entity.geometry();

       const DomainType &x_T = globalEntityGeometry.global(entityQuadrature.point(0));

       const RangeType entityVolume = entityQuadrature.weight(0) * 
               globalEntityGeometry.integrationElement(entityQuadrature.point(0));

       // int_Y A_h^{\epsilon} - A^{\epsilob}
       RangeType difference[dimension];
       for( int k = 0; k < dimension; ++k )
         difference[k] = 0.0;

       // \nabla u_H(x_T)
       LocalFunctionType u_H_local = u_H.localFunction(entity);
       JacobianRangeType gradient_u_H(0.);
       u_H_local.jacobian(entityQuadrature[ 0 ], gradient_u_H);

       // iterator over the elements of the periodic micro grid:
       PeriodicIteratorType p_endit = periodicDiscreteFunctionSpace_.end();
       for( PeriodicIteratorType p_it = periodicDiscreteFunctionSpace_.begin(); p_it != p_endit; ++p_it )
        {

          const PeriodicEntityType &micro_entity = *p_it;

          int quadOrder = 2 * PeriodicDiscreteFunctionSpaceType :: polynomialOrder + 2;

          // two quadrature formulas ( A_h^{\eps}(y)=A^{\eps}(y_s) vs. A^{\eps}(y) )
          PeriodicEntityQuadratureType one_point_quadrature( micro_entity , 0 );
          PeriodicEntityQuadratureType high_order_quadrature( micro_entity , quadOrder );

          // Q_h(u_H)(x_T,y) on the micro entity:
          PeriodicLocalFunctionType loc_Q_u_H_x_T = corrector_u_H_on_entity.localFunction(micro_entity);
          JacobianRangeType gradient_Q_u_H_x_T(0.);
          loc_Q_u_H_x_T.jacobian(one_point_quadrature[ 0 ], gradient_Q_u_H_x_T);

          // S denotes the micro grid element (i.e. 'micro_entity')
          const PeriodicEntityGeometryType& geometry_S = micro_entity.geometry();
          // y_S denotes the barycenter of the micro grid element S:
          const DomainType &y_S = geometry_S.global(one_point_quadrature.point(0));

          // to evaluate A^{\epsilon}_h:
          DomainType globalPoint_center;
          JacobianRangeType direction_of_diffusion;
          for( int k = 0; k < dimension; ++k )
            {
             globalPoint_center[ k ] = x_T[ k ] + (delta * y_S[ k ]);
             direction_of_diffusion[ 0 ][ k ] = gradient_u_H[ 0 ][ k ] + gradient_Q_u_H_x_T[ 0 ][ k ];
            }

          JacobianRangeType diffusive_flux_y_S;
          diffusion_.diffusiveFlux( globalPoint_center,
                                    direction_of_diffusion,
                                    diffusive_flux_y_S );

          const size_t numQuadraturePoints = high_order_quadrature.nop();
          for( size_t microQuadraturePoint = 0; microQuadraturePoint < numQuadraturePoints; ++microQuadraturePoint )
            {

              // local (barycentric) coordinates (with respect to entity)
              const typename PeriodicEntityQuadratureType::CoordinateType &y_barycentric = high_order_quadrature.point( microQuadraturePoint );

              // current quadrature point y in global coordinates
              DomainType y = geometry_S.global( y_barycentric );

              const double weight_micro_quadrature = high_order_quadrature.weight(  microQuadraturePoint ) * geometry_S.integrationElement( y_barycentric );

              // to evaluate A^{\epsilon}:
              DomainType globalPoint;
              for( int k = 0; k < dimension; ++k )
                globalPoint[ k ] = (delta * y[ k ]) + x_T[ k ];

              // diffusive flux in y (in direction \nabla u_H(x_T) + \nabla_y Q_h^T(u_H)(y_S) )
              JacobianRangeType diffusive_flux_y;
              diffusion_.diffusiveFlux( globalPoint,
                                        direction_of_diffusion,
                                        diffusive_flux_y );

              for( int k = 0; k < dimension; ++k )
                difference[ k ] += weight_micro_quadrature * ( diffusive_flux_y[ 0 ][ k ] - diffusive_flux_y_S[ 0 ][ k ] );

            }

        }

       for( int k = 0; k < dimension; ++k )
          local_indicator += pow( difference[ k ], 2.0 ) * entityVolume;

       return local_indicator;

     } // end of method



    // (1/2)*H_E^3*\eta_E^{res}
    // (1/2) since we visit every entity twice (inner and outer)
    template< class IntersectionType >
    RangeType indicator_res_E( const IntersectionType &intersection,
                                     DiscreteFunctionType &u_H,
                               const PeriodicDiscreteFunctionType &corrector_u_H_on_inner_entity,
                               const PeriodicDiscreteFunctionType &corrector_u_H_on_outer_entity )
     {

       RangeType local_indicator(0.0);

       Problem::ModelProblemData model_info;
       const double delta = model_info.getDelta();
       const double epsilon_estimated = model_info.getEpsilonEstimated();


       EntityPointerType inner_it =  intersection.inside();
       EntityPointerType outer_it =  intersection.outside();

       const EntityType& inner_entity = *inner_it;
       const EntityType& outer_entity = *outer_it;

       FaceQuadratureType faceQuadrature( discreteFunctionSpace_.gridPart(), intersection, 0 , FaceQuadratureType::INSIDE);

       DomainType unitOuterNormal =
                intersection.unitOuterNormal(faceQuadrature.localPoint(0));

       const FaceGeometryType& faceGeometry = intersection.geometry();
       // H_E (= |E|):
       const RangeType edge_length = faceGeometry.volume();

       // jump = innerValue - outerValue
       RangeType innerValue = 0.0;
       RangeType outerValue = 0.0;

       EntityQuadratureType innerEntityQuadrature( inner_entity , 0 );
       EntityQuadratureType outerEntityQuadrature( outer_entity , 0 ); // 0 = polynomial order
       // the global quadrature (quadrature on the macro element T)

       const EntityGeometryType& globalInnerEntityGeometry = inner_entity.geometry();
       const EntityGeometryType& globalOuterEntityGeometry = outer_entity.geometry();

       const DomainType &x_T_inner = globalInnerEntityGeometry.global(innerEntityQuadrature.point(0));
       const DomainType &x_T_outer = globalOuterEntityGeometry.global(outerEntityQuadrature.point(0));

       const RangeType innerEntityVolume = innerEntityQuadrature.weight(0) * 
               globalInnerEntityGeometry.integrationElement(innerEntityQuadrature.point(0));
       const RangeType outerEntityVolume = outerEntityQuadrature.weight(0) * 
               globalOuterEntityGeometry.integrationElement(outerEntityQuadrature.point(0));

       // \nabla u_H(x_T) (on the inner element T)
       LocalFunctionType inner_u_H_local = u_H.localFunction(inner_entity);
       JacobianRangeType gradient_inner_u_H(0.);
       inner_u_H_local.jacobian(innerEntityQuadrature[ 0 ], gradient_inner_u_H);

       // \nabla u_H(x_T) (on the outer element \bar{T})
       LocalFunctionType outer_u_H_local = u_H.localFunction(outer_entity);
       JacobianRangeType gradient_outer_u_H(0.);
       outer_u_H_local.jacobian(outerEntityQuadrature[ 0 ], gradient_outer_u_H);

       // iterator over the elements of the periodic micro grid:
       PeriodicIteratorType p_endit = periodicDiscreteFunctionSpace_.end();
       for( PeriodicIteratorType p_it = periodicDiscreteFunctionSpace_.begin(); p_it != p_endit; ++p_it )
        {

          const PeriodicEntityType &micro_entity = *p_it;


          //one point quadrature formula (since A_h^{\eps}(y)=A^{\eps}(y_s) )
          PeriodicEntityQuadratureType one_point_quadrature( micro_entity , 0 );

          // Q_h(u_H)(x_T,y) on the micro entity:
          PeriodicLocalFunctionType loc_Q_u_H_x_T_inner = corrector_u_H_on_inner_entity.localFunction(micro_entity);
          JacobianRangeType gradient_Q_u_H_x_T_inner(0.);
          loc_Q_u_H_x_T_inner.jacobian(one_point_quadrature[ 0 ], gradient_Q_u_H_x_T_inner);

          // Q_h(u_H)(x_{bar{T}},y) on the micro entity:
          PeriodicLocalFunctionType loc_Q_u_H_x_T_outer = corrector_u_H_on_outer_entity.localFunction(micro_entity);
          JacobianRangeType gradient_Q_u_H_x_T_outer(0.);
          loc_Q_u_H_x_T_outer.jacobian(one_point_quadrature[ 0 ], gradient_Q_u_H_x_T_outer);

          // S denotes the micro grid element (i.e. 'micro_entity')
          const PeriodicEntityGeometryType& geometry_S = micro_entity.geometry();

          // local (barycentric) coordinates (with respect to entity)
          const typename PeriodicEntityQuadratureType::CoordinateType &y_S_barycentric = one_point_quadrature.point(0);

          // y_S denotes the barycenter of the micro grid element S:
          const DomainType &y_S = geometry_S.global(y_S_barycentric);

          // to evaluate A^{\epsilon}_h:
          DomainType globalPoint_inner;
          JacobianRangeType direction_of_diffusion_inner;
          for( int k = 0; k < dimension; ++k )
           {
             globalPoint_inner[ k ] = x_T_inner[ k ] + (delta * y_S[ k ]);
             direction_of_diffusion_inner[ 0 ][ k ] = gradient_inner_u_H[ 0 ][ k ] + gradient_Q_u_H_x_T_inner[ 0 ][ k ];
           }

          // to evaluate A^{\epsilon}_h:
          DomainType globalPoint_outer;
          JacobianRangeType direction_of_diffusion_outer;
          for( int k = 0; k < dimension; ++k )
            {
             globalPoint_outer[ k ] = x_T_outer[ k ] + (delta * y_S[ k ]);
             direction_of_diffusion_outer[ 0 ][ k ] = gradient_outer_u_H[ 0 ][ k ] + gradient_Q_u_H_x_T_outer[ 0 ][ k ];
            }


          JacobianRangeType diffusive_flux_y_S_inner;
          diffusion_.diffusiveFlux( globalPoint_inner,
                                    direction_of_diffusion_inner,
                                    diffusive_flux_y_S_inner );

          JacobianRangeType diffusive_flux_y_S_outer;
          diffusion_.diffusiveFlux( globalPoint_outer,
                                    direction_of_diffusion_outer,
                                    diffusive_flux_y_S_outer );

          double cutting_function = 1.0;
          for( int k = 0; k < dimension; ++k )
            {
              // is the current quadrature point in the relevant cell?
              // Y = [ -0.5 , 0.5 ]^dimension
              if ( fabs(y_S[ k ]) > (0.5*(epsilon_estimated/delta)) )
                { cutting_function *= 0.0; }
            }

          const double weight_micro_quadrature = one_point_quadrature.weight(0) * geometry_S.integrationElement( y_S_barycentric );

          for( int k = 0; k < dimension; ++k )
            {
             innerValue += cutting_function * weight_micro_quadrature * diffusive_flux_y_S_inner[ 0 ][ k ] * unitOuterNormal[ k ];
             outerValue += cutting_function * weight_micro_quadrature * diffusive_flux_y_S_outer[ 0 ][ k ] * unitOuterNormal[ k ];
            }

        }

       local_indicator += pow( innerValue - outerValue, 2.0 );

       // in 2D (and this is what we assume) it holds |E|*H_E^3 = H_E^4 = |E|^4
       return ( pow(edge_length,4.0 ) * (1.0/2.0) * local_indicator );

     } // end of method


    // NOTE: This method ONLY works for uniformly refined micro-grid and GRIDDIM=2!
    // (even though it might be extended easily to other cases, here we use the cheapest strategy, which just works for the case described above!)
    // here, uniform also means that we have the same number of elements in every direction!)
    // \bar{\eta}_T^{res}
    RangeType indicator_res_T( const EntityType &entity,
                                     DiscreteFunctionType &u_H,
                               const PeriodicDiscreteFunctionType &corrector_u_H_on_entity )
     {


       RangeType local_indicator(0.0);

       Problem::ModelProblemData model_info;
       const double delta = model_info.getDelta();
       const double epsilon_estimated = model_info.getEpsilonEstimated();

       EntityQuadratureType entityQuadrature( entity , 0 ); // 0 = polynomial order
       // the global quadrature (quadrature on the macro element T)

       const EntityGeometryType& globalEntityGeometry = entity.geometry();

       const DomainType &x_T = globalEntityGeometry.global(entityQuadrature.point(0));

       const RangeType entityVolume = entityQuadrature.weight(0) * 
               globalEntityGeometry.integrationElement(entityQuadrature.point(0));

       // \nabla u_H(x_T)
       LocalFunctionType u_H_local = u_H.localFunction(entity);
       JacobianRangeType gradient_u_H(0.);
       u_H_local.jacobian(entityQuadrature[ 0 ], gradient_u_H);


       if ( dimension != 2 )
        {
          std :: cout << "The error indicator 'indicator_res_T' is not implemented for dimension!=2 and only works for uniformly refined micro-grids!" << std :: endl;
        }

       // edge length of a boundary face
       RangeType ref_edge_length = 1.0;

       GridPartType auxGridPart = auxiliaryDiscreteFunctionSpace_.gridPart();

       IteratorType micro_it = auxiliaryDiscreteFunctionSpace_.begin();

       // we just need one element to determine all the properties (due to uniform refinement)!
       IntersectionIteratorType endnit = auxGridPart.iend(*micro_it);
       for(IntersectionIteratorType nit = auxGridPart.ibegin(*micro_it); nit != endnit ; ++nit)
         {
           FaceQuadratureType faceQuadrature( auxGridPart, *nit, 1 , FaceQuadratureType::INSIDE);
           const FaceGeometryType& faceGeometry = nit->geometry();

           if ( ref_edge_length > faceGeometry.volume() )
            { ref_edge_length = faceGeometry.volume(); }

         }

       // number of boundary faces per cube-edge:
       int num_boundary_faces_per_direction = int( (1 / ref_edge_length) + 0.2 );
       // (+0.2 to avoid rounding errors)

       // generalized jump up/down
       RangeType jump_up_down[ num_boundary_faces_per_direction ];

       // generalized jump left/right
       RangeType jump_left_right[ num_boundary_faces_per_direction ];


       for( int id = 0; id < num_boundary_faces_per_direction; ++id )
         {
          jump_up_down[ id ] = 0.0;
          jump_left_right[ id ] = 0.0;
         }


       // procedure for computing the generalized jumps:
       // 1. compute the center of the current face E: (x_E,y_E)
       // 2. one and only one of these values fulfiles abs()=1/2
       //    (i.e. we either have 'abs(x_E)=1/2' or 'abs(y_E)=1/2')
       //    without loss of generality we assume 'abs(y_E)=1/2', then
       //    we are in the setting of 'jump_up_down'
       // 3. Any 'jump_up_down' recieves a unique ID by
       //     jump up down over (x,-1/2) and (x,1/2) = jump_up_down[ (num_boundary_faces_per_direction/2) + (( x - (edge_length/2) ) / edge_length) ]
       //     (num_boundary_faces_per_direction/2) + (( x - (edge_length/2) ) / edge_length) is the corresponding ID
       // 4. the situation is identical for 'abs(x_E)=1/2'.


       // iterator over the elements of the periodic micro grid:
       IteratorType p_endit = auxiliaryDiscreteFunctionSpace_.end();
       for( IteratorType p_it = auxiliaryDiscreteFunctionSpace_.begin(); p_it != p_endit; ++p_it )
        {

          // --------- the 'inner entity' (micro grid) ------------

          const EntityType &micro_entity = *p_it;

          // one point quadrature formula ( A_h^{\eps}(y)=A^{\eps}(y_s) )
          EntityQuadratureType one_point_quadrature( micro_entity , 0 );

          // Q_h(u_H)(x_T,y) on the micro entity:
          PeriodicLocalFunctionType loc_Q_u_H_x_T = corrector_u_H_on_entity.localFunction(micro_entity);
          JacobianRangeType gradient_Q_u_H_x_T(0.);
          loc_Q_u_H_x_T.jacobian(one_point_quadrature[ 0 ], gradient_Q_u_H_x_T);

          // S denotes the micro grid element (i.e. 'micro_entity')
          const EntityGeometryType& geometry_S = micro_entity.geometry();
          // y_S denotes the barycenter of the micro grid element S:
          const DomainType &y_S = geometry_S.global(one_point_quadrature.point(0));

          // to evaluate A^{\epsilon}_h (in center of current inner entity):
          DomainType globalPoint;
          JacobianRangeType direction_of_diffusion;
          for( int k = 0; k < dimension; ++k )
           {
            direction_of_diffusion[ 0 ][ k ] = gradient_u_H[ 0 ][ k ] + gradient_Q_u_H_x_T[ 0 ][ k ];
            globalPoint[ k ] = x_T[ k ] + (delta * y_S[ k ]);
           }

          JacobianRangeType diffusive_flux_y_S;
          diffusion_.diffusiveFlux( globalPoint,
                                    direction_of_diffusion,
                                    diffusive_flux_y_S );

          // ----------------------------------------------------

          IntersectionIteratorType p_endnit = auxGridPart.iend(micro_entity);
          for(IntersectionIteratorType p_nit = auxGridPart.ibegin(micro_entity); p_nit != p_endnit ; ++p_nit)
            {

               // Note: we are on the zero-centered unit cube! (That's why everything works!)

               FaceQuadratureType faceQuadrature( auxGridPart, *p_nit, 0, FaceQuadratureType::INSIDE );
               const FaceGeometryType& faceGeometry = p_nit->geometry();

               const RangeType edge_length = faceGeometry.volume();

               DomainType unitOuterNormal =
                       p_nit->unitOuterNormal(faceQuadrature.localPoint(0));

               //if there is a neighbor entity (the normal gradient jumps)
               if ( p_nit->neighbor() )
                {

                  // --------- the 'outer entity' (micro grid) ------------
                  //                ( neighbor entity )

                  EntityPointerType outer_p_it = p_nit->outside();
                  const EntityType &outer_micro_entity = *outer_p_it;

                  // one point quadrature formula ( A_h^{\eps}(y)=A^{\eps}(y_s) )
                  EntityQuadratureType outer_one_point_quadrature( outer_micro_entity , 0 );

                  // Q_h(u_H)(x_T,y) on the neighbor entity:
                  PeriodicLocalFunctionType outer_loc_Q_u_H_x_T = corrector_u_H_on_entity.localFunction(outer_micro_entity);
                  JacobianRangeType gradient_outer_Q_u_H_x_T(0.);
                  outer_loc_Q_u_H_x_T.jacobian(outer_one_point_quadrature[ 0 ], gradient_outer_Q_u_H_x_T);

                  // S denotes the micro grid element (i.e. 'micro_entity')
                  const EntityGeometryType& outer_geometry_S = outer_micro_entity.geometry();
                  // outer_y_S denotes the barycenter of the neighbor micro grid element of S:
                  const DomainType &outer_y_S = outer_geometry_S.global(outer_one_point_quadrature.point(0));

                  // to evaluate A^{\epsilon}_h (in center of current outer entity):
                  DomainType outer_globalPoint;
                  JacobianRangeType direction_of_diffusion_outside;
                  for( int k = 0; k < dimension; ++k )
                    {
                     direction_of_diffusion_outside[ 0 ][ k ] = gradient_u_H[ 0 ][ k ] + gradient_outer_Q_u_H_x_T[ 0 ][ k ];
                     outer_globalPoint[ k ] = x_T[ k ] + (delta * outer_y_S[ k ]);
                    }

                  JacobianRangeType diffusive_flux_y_S_outside;
                  diffusion_.diffusiveFlux( outer_globalPoint,
                                            direction_of_diffusion_outside,
                                            diffusive_flux_y_S_outside );

                  // ----------------------------------------------------

                  RangeType aux_value = 0.0;

                  for( int k = 0; k < dimension; ++k )
                    aux_value += ( diffusive_flux_y_S_outside[ 0 ][ k ] - diffusive_flux_y_S[ 0 ][ k ] ) * unitOuterNormal[ k ] ;

                  local_indicator += pow( edge_length , 4.0 ) * pow( aux_value , 2.0 );


                }
               else //if there is no neighbor entity, the face is a boundary face and we use the generalized gradient jumps.
                {

                  // Remember:
                  // procedure for computing the generalized jumps:
                  // 1. compute the center of the current face E: (x_E,y_E)
                  // 2. one and only one of these values fulfiles abs()=1/2
                  //    (i.e. we either have 'abs(x_E)=1/2' or 'abs(y_E)=1/2')
                  //    without loss of generality we assume 'abs(y_E)=1/2', then
                  //    we are in the setting of 'jump_up_down'
                  // 3. Any 'jump_up_down' recieves a unique ID by
                  //     jump up down over (x,-1/2) and (x,1/2) = ... jump_up_down[ (( x - (edge_length/2) ) / edge_length) ]
                  //     ... (( x - (edge_length/2) ) / edge_length) is the corresponding ID
                  // 4. the situation is identical for 'abs(x_E)=1/2'.

                  const DomainType &edge_center = geometry_S.global(faceQuadrature.point(0));

                  if ( fabs(edge_center[0]) == 0.5 )
                   {
                    // + 0.2 to avoid rounding errors!
                    int id = int( ( num_boundary_faces_per_direction / 2 ) + (( edge_center[1] - (edge_length / 2.0) ) / edge_length) + 0.2 );

                    // unit outer normal creates the correct sign!
                    for( int k = 0; k < dimension; ++k )
                       jump_up_down[ id ] += pow( edge_length , 2.0 ) * ( diffusive_flux_y_S[ 0 ][ k ] * unitOuterNormal[ k ] );

                    //std :: cout << "edge_center = " << edge_center << std :: endl;
                    //std :: cout << "edge_length = " << edge_length << std :: endl;
                    //std :: cout << "unitOuterNormal = " << unitOuterNormal << std :: endl;
                    //std :: cout << "diffusive_flux_y_S = " << diffusive_flux_y_S[ 0 ] << std :: endl;
                    //std :: cout << "jump_up_down id = " << id << std :: endl;
                    //std :: cout << "jump_up_down[" << id << "] = " << jump_up_down[ id ] << std :: endl << std :: endl;

                   }
                  else
                   {

                    // + 0.2 to avoid rounding errors!
                    int id = int( ( num_boundary_faces_per_direction / 2 ) + (( edge_center[0] - (edge_length / 2.0) ) / edge_length) + 0.2 );

                    // unit outer normal creates the correct sign!
                    for( int k = 0; k < dimension; ++k )
                       jump_left_right[ id ] += pow( edge_length , 2.0 ) * ( diffusive_flux_y_S[ 0 ][ k ] * unitOuterNormal[ k ] );

                    //std :: cout << "edge_center = " << edge_center << std :: endl;
                    //std :: cout << "edge_length = " << edge_length << std :: endl;
                    //std :: cout << "unitOuterNormal = " << unitOuterNormal << std :: endl;
                    //std :: cout << "diffusive_flux_y_S = " << diffusive_flux_y_S[ 0 ] << std :: endl;
                    //std :: cout << "jump_left_right id = " << id << std :: endl;
                    //std :: cout << "jump_left_right[" << id << "] = " << jump_left_right[ id ] << std :: endl << std :: endl;

                   }

                }

            }


        }

       for( int id = 0; id < num_boundary_faces_per_direction; ++id )
         {
          local_indicator += ( pow( jump_up_down[ id ] , 2.0 ) + pow( jump_left_right[ id ] , 2.0 ) );
         }

       local_indicator *= entityVolume;

       return local_indicator;

     } // end of method


//only for TFR:

    // NOTE: This method ONLY works for uniformly refined micro-grid and GRIDDIM=2!
    // (even though it might be extended easily to other cases, here we use the cheapest strategy, which just works for the case described above!)
    // here, uniform also means that we have the same number of elements in every direction!)
    // \eta_T^{tfr}
    RangeType indicator_tfr_1( const EntityType &entity,
                                     DiscreteFunctionType &u_H,
                               const PeriodicDiscreteFunctionType &corrector_u_H_on_entity )
     {

       RangeType local_indicator(0.0);

       Problem::ModelProblemData model_info;
       const double delta = model_info.getDelta();
       const double epsilon_estimated = model_info.getEpsilonEstimated();

       EntityQuadratureType entityQuadrature( entity , 0 ); // 0 = polynomial order
       // the global quadrature (quadrature on the macro element T)

       const EntityGeometryType& globalEntityGeometry = entity.geometry();

       const DomainType &x_T = globalEntityGeometry.global(entityQuadrature.point(0));

       const RangeType entityVolume = entityQuadrature.weight(0) * 
               globalEntityGeometry.integrationElement(entityQuadrature.point(0));

       // \nabla u_H(x_T)
       LocalFunctionType u_H_local = u_H.localFunction(entity);
       JacobianRangeType gradient_u_H(0.);
       u_H_local.jacobian(entityQuadrature[ 0 ], gradient_u_H);


       if ( dimension != 2 )
        {
          std :: cout << "The error indicator 'indicator_tfr_1' is not implemented for dimension!=2 and only works for uniformly refined micro-grids!" << std :: endl;
        }


       // edge length of a boundary face
       RangeType ref_edge_length = 1.0;

       GridPartType auxGridPart = auxiliaryDiscreteFunctionSpace_.gridPart();

       IteratorType micro_it = auxiliaryDiscreteFunctionSpace_.begin();

       // we just need one element to determine all the properties (due to uniform refinement)!
       IntersectionIteratorType endnit = auxGridPart.iend(*micro_it);
       for(IntersectionIteratorType nit = auxGridPart.ibegin(*micro_it); nit != endnit ; ++nit)
         {
           FaceQuadratureType faceQuadrature( auxGridPart, *nit, 1 , FaceQuadratureType::INSIDE);
           const FaceGeometryType& faceGeometry = nit->geometry();

           if ( ref_edge_length > faceGeometry.volume() )
            { ref_edge_length = faceGeometry.volume(); }

         }

       // number of boundary faces per (\epsilon/\delta-scaled) cube edge:
       int num_boundary_faces_per_direction = int( ((epsilon_estimated/delta) / ref_edge_length) + 0.2 );
       // (+0.2 to avoid rounding errors)

       // generalized jump up/down
       RangeType jump_up_down[ num_boundary_faces_per_direction ];

       // generalized jump left/right
       RangeType jump_left_right[ num_boundary_faces_per_direction ];

       for( int id = 0; id < num_boundary_faces_per_direction; ++id )
         {
          jump_up_down[ id ] = 0.0;
          jump_left_right[ id ] = 0.0;
         }

       // did you find a boundary edge of the \eps-\delta-cube? (you must find it!!)
       bool eps_delta_boundary_edge_found = false;

       // procedure for computing the generalized jumps:
       // 1. compute the center of the current face E: (x_E,y_E)
       // 2. one and only one of these values fulfiles abs()=1/2
       //    (i.e. we either have 'abs(x_E)=1/2' or 'abs(y_E)=1/2')
       //    without loss of generality we assume 'abs(y_E)=1/2', then
       //    we are in the setting of 'jump_up_down'
       // 3. Any 'jump_up_down' recieves a unique ID by
       //     jump up down over (x,-1/2) and (x,1/2) = jump_up_down[ (num_boundary_faces_per_direction/2) + (( x - (edge_length/2) ) / edge_length) ]
       //     (num_boundary_faces_per_direction/2) + (( x - (edge_length/2) ) / edge_length) is the corresponding ID
       // 4. the situation is identical for 'abs(x_E)=1/2'.


       // iterator over the elements of the periodic micro grid:
       IteratorType p_endit = auxiliaryDiscreteFunctionSpace_.end();
       for( IteratorType p_it = auxiliaryDiscreteFunctionSpace_.begin(); p_it != p_endit; ++p_it )
        {

          // --------- the 'inner entity' (micro grid) ------------

          const EntityType &micro_entity = *p_it;

          // one point quadrature formula ( A_h^{\eps}(y)=A^{\eps}(y_s) )
          EntityQuadratureType one_point_quadrature( micro_entity , 0 );

          // Q_h(u_H)(x_T,y) on the micro entity:
          PeriodicLocalFunctionType loc_Q_u_H_x_T = corrector_u_H_on_entity.localFunction(micro_entity);
          JacobianRangeType gradient_Q_u_H_x_T(0.);
          loc_Q_u_H_x_T.jacobian(one_point_quadrature[ 0 ], gradient_Q_u_H_x_T);

          // S denotes the micro grid element (i.e. 'micro_entity')
          const EntityGeometryType& geometry_S = micro_entity.geometry();
          // y_S denotes the barycenter of the micro grid element S:
          const DomainType &y_S = geometry_S.global(one_point_quadrature.point(0));

          // to evaluate A^{\epsilon}_h (in center of current inner entity):
          DomainType globalPoint;
          JacobianRangeType direction_of_diffusion;
          for( int k = 0; k < dimension; ++k )
            {
             direction_of_diffusion[ 0 ][ k ] = gradient_u_H[ 0 ][ k ] + gradient_Q_u_H_x_T[ 0 ][ k ];
             globalPoint[ k ] = ( delta * y_S[ k ] ) + x_T[ k ];
            }

          JacobianRangeType diffusive_flux_y_S;
          diffusion_.diffusiveFlux( globalPoint,
                                    direction_of_diffusion,
                                    diffusive_flux_y_S );

          if ( (fabs(y_S[1]) <= ((0.5*(epsilon_estimated/delta)))) &&
               (fabs(y_S[0]) <= ((0.5*(epsilon_estimated/delta))))    )
          {
          // ----------------------------------------------------

          IntersectionIteratorType p_endnit = auxGridPart.iend(micro_entity);
          for(IntersectionIteratorType p_nit = auxGridPart.ibegin(micro_entity); p_nit != p_endnit ; ++p_nit)
            {

               // Note: we are on the zero-centered unit cube! (That's why everything works!)

               FaceQuadratureType faceQuadrature( auxGridPart, *p_nit, 0, FaceQuadratureType::INSIDE );
               const FaceGeometryType& faceGeometry = p_nit->geometry();

               const RangeType edge_length = faceGeometry.volume();

               DomainType unitOuterNormal =
                       p_nit->unitOuterNormal(faceQuadrature.localPoint(0));

               const DomainType &edge_center = geometry_S.global(faceQuadrature.point(0));

               if ( (fabs(edge_center[0]) == ((0.5*(epsilon_estimated/delta)))) || 
                    (fabs(edge_center[1]) == ((0.5*(epsilon_estimated/delta)))) )
                {

                  //we use the generalized gradient jumps.

                  if ( (fabs(edge_center[0]) == ((0.5*(epsilon_estimated/delta)))) &&
                       (fabs(edge_center[1]) < ((0.5*(epsilon_estimated/delta))))   )
                   {
                    // + 0.2 to avoid rounding errors!
                    int id = int( ( num_boundary_faces_per_direction / 2 ) + (( edge_center[1] - (edge_length / 2.0) ) / edge_length) + 0.2 );

                    // unit outer normal creates the correct sign!
                    for( int k = 0; k < dimension; ++k )
                       jump_up_down[ id ] += sqrt(edge_length) * ( diffusive_flux_y_S[ 0 ][ k ] * unitOuterNormal[ k ] );

                    //std :: cout << "edge_center = " << edge_center << std :: endl;
                    //std :: cout << "edge_length = " << edge_length << std :: endl;
                    //std :: cout << "unitOuterNormal = " << unitOuterNormal << std :: endl;
                    //std :: cout << "diffusive_flux_y_S = " << diffusive_flux_y_S[ 0 ] << std :: endl;
                    //std :: cout << "jump_up_down id = " << id << std :: endl;
                    //std :: cout << "jump_up_down[" << id << "] = " << jump_up_down[ id ] << std :: endl << std :: endl;

                    eps_delta_boundary_edge_found = true;
                    //std :: cout << "Found eps/delta boundary edge with center = " << edge_center << std :: endl;

                   }

                  if ( (fabs(edge_center[1]) == ((0.5*(epsilon_estimated/delta)))) &&
                       (fabs(edge_center[0]) < ((0.5*(epsilon_estimated/delta))))   )
                   {

                    // + 0.2 to avoid rounding errors!
                    int id = int( ( num_boundary_faces_per_direction / 2 ) + (( edge_center[0] - (edge_length / 2.0) ) / edge_length) + 0.2 );

                    // unit outer normal creates the correct sign!
                    for( int k = 0; k < dimension; ++k )
                       jump_left_right[ id ] += sqrt(edge_length) * ( diffusive_flux_y_S[ 0 ][ k ] * unitOuterNormal[ k ] );

                    //std :: cout << "edge_center = " << edge_center << std :: endl;
                    //std :: cout << "edge_length = " << edge_length << std :: endl;
                    //std :: cout << "unitOuterNormal = " << unitOuterNormal << std :: endl;
                    //std :: cout << "diffusive_flux_y_S = " << diffusive_flux_y_S[ 0 ] << std :: endl;
                    //std :: cout << "jump_left_right id = " << id << std :: endl;
                    //std :: cout << "jump_left_right[" << id << "] = " << jump_left_right[ id ] << std :: endl << std :: endl;

                    eps_delta_boundary_edge_found = true;
                    //std :: cout << "Found eps/delta boundary edge with center = " << edge_center << std :: endl;

                   }

                }

               if ( ( fabs(edge_center[0]) < (0.5*(epsilon_estimated/delta)) ) && 
                    ( fabs(edge_center[1]) < (0.5*(epsilon_estimated/delta)) )  )
                {
                   // in this situation, p_nit always has an outside neighbor entity
                   // (use the normal gradient jumps)


                   // --------- the 'outer entity' (micro grid) ------------
                   //                ( neighbor entity )

                   EntityPointerType outer_p_it = p_nit->outside();
                   const EntityType &outer_micro_entity = *outer_p_it;

                   // one point quadrature formula ( A_h^{\eps}(y)=A^{\eps}(y_s) )
                   EntityQuadratureType outer_one_point_quadrature( outer_micro_entity , 0 );

                   // Q_h(u_H)(x_T,y) on the neighbor entity:
                   PeriodicLocalFunctionType outer_loc_Q_u_H_x_T = corrector_u_H_on_entity.localFunction(outer_micro_entity);
                   JacobianRangeType gradient_outer_Q_u_H_x_T(0.);
                   outer_loc_Q_u_H_x_T.jacobian(outer_one_point_quadrature[ 0 ], gradient_outer_Q_u_H_x_T);

                   // S denotes the micro grid element (i.e. 'micro_entity')
                   const EntityGeometryType& outer_geometry_S = outer_micro_entity.geometry();
                   // outer_y_S denotes the barycenter of the neighbor micro grid element of S:
                   const DomainType &outer_y_S = outer_geometry_S.global(outer_one_point_quadrature.point(0));

                   // to evaluate A^{\epsilon}_h (in center of current outer entity):
                   DomainType outer_globalPoint;
                   JacobianRangeType direction_of_diffusion_outside;
                   for( int k = 0; k < dimension; ++k )
                     {
                      outer_globalPoint[ k ] = x_T[ k ] + (delta * outer_y_S[ k ]);
                      direction_of_diffusion_outside[ 0 ][ k ] = gradient_u_H[ 0 ][ k ] +  gradient_outer_Q_u_H_x_T[ 0 ][ k ];
                     }

                   JacobianRangeType diffusive_flux_y_S_outside;
                   diffusion_.diffusiveFlux( outer_globalPoint,
                                             direction_of_diffusion_outside,
                                             diffusive_flux_y_S_outside );

                   // ----------------------------------------------------

                   RangeType aux_value = 0.0;

                   for( int k = 0; k < dimension; ++k )
                     aux_value += ( diffusive_flux_y_S_outside[ 0 ][ k ] - diffusive_flux_y_S[ 0 ][ k ] ) * unitOuterNormal[ k ] ;

                   local_indicator += edge_length * pow( aux_value , 2.0 );
                }

            } // end Intersection iterator
        } // endif: "y_S \in \eps/\delta Y"

        }

       for( int id = 0; id < num_boundary_faces_per_direction; ++id )
         {
          local_indicator += ( pow( jump_up_down[ id ] , 2.0 ) + pow( jump_left_right[ id ] , 2.0 ) );
         }

       if ( eps_delta_boundary_edge_found == false )
        {
           std :: cout << "Error! Make sure that the restriction of the Y-triangulation on 'eps/delta Y' is a complete periodic triangulation of 'eps/delta Y' on its own. (for instance: delta = 2 epsilon should work)" << std :: endl; 
           std :: abort();
        }

       local_indicator *= entityVolume;

       return local_indicator;

     } // end of method



#if 1

    // NOTE: This method ONLY works for uniformly refined micro-grid and GRIDDIM=2!
    // (even though it might be extended easily to other cases, here we use the cheapest strategy, which just works for the case described above!)
    // here, uniform also means that we have the same number of elements in every direction!)
    // This indicator does not exist in theory. It is is additionally multiplied with h^3 to fit the other orders of convergence. In this setting it is more suitable for a comparison to capture the effect of boundary jumps in the case of a wrong boundary condition
    RangeType indicator_effective_tfr( const EntityType &entity,
                                       DiscreteFunctionType &u_H,
                                       const PeriodicDiscreteFunctionType &corrector_u_H_on_entity )
     {

       RangeType local_indicator(0.0);

       Problem::ModelProblemData model_info;
       const double delta = model_info.getDelta();
       const double epsilon_estimated = model_info.getEpsilonEstimated();

       EntityQuadratureType entityQuadrature( entity , 0 ); // 0 = polynomial order
       // the global quadrature (quadrature on the macro element T)

       const EntityGeometryType& globalEntityGeometry = entity.geometry();

       const DomainType &x_T = globalEntityGeometry.global(entityQuadrature.point(0));

       const RangeType entityVolume = entityQuadrature.weight(0) * 
               globalEntityGeometry.integrationElement(entityQuadrature.point(0));

       // \nabla u_H(x_T)
       LocalFunctionType u_H_local = u_H.localFunction(entity);
       JacobianRangeType gradient_u_H(0.);
       u_H_local.jacobian(entityQuadrature[ 0 ], gradient_u_H);


       if ( dimension != 2 )
        {
          std :: cout << "The error indicator 'indicator_tfr_1' is not implemented for dimension!=2 and only works for uniformly refined micro-grids!" << std :: endl;
        }


       // edge length of a boundary face
       RangeType ref_edge_length = 1.0;

       GridPartType auxGridPart = auxiliaryDiscreteFunctionSpace_.gridPart();

       IteratorType micro_it = auxiliaryDiscreteFunctionSpace_.begin();

       // we just need one element to determine all the properties (due to uniform refinement)!
       IntersectionIteratorType endnit = auxGridPart.iend(*micro_it);
       for(IntersectionIteratorType nit = auxGridPart.ibegin(*micro_it); nit != endnit ; ++nit)
         {
           FaceQuadratureType faceQuadrature( auxGridPart, *nit, 1 , FaceQuadratureType::INSIDE);
           const FaceGeometryType& faceGeometry = nit->geometry();

           if ( ref_edge_length > faceGeometry.volume() )
            { ref_edge_length = faceGeometry.volume(); }

         }

       // number of boundary faces per (\epsilon/\delta-scaled) cube edge:
       int num_boundary_faces_per_direction = int( ((epsilon_estimated/delta) / ref_edge_length) + 0.2 );
       // (+0.2 to avoid rounding errors)

       // generalized jump up/down
       RangeType jump_up_down[ num_boundary_faces_per_direction ];

       // generalized jump left/right
       RangeType jump_left_right[ num_boundary_faces_per_direction ];

       for( int id = 0; id < num_boundary_faces_per_direction; ++id )
         {
          jump_up_down[ id ] = 0.0;
          jump_left_right[ id ] = 0.0;
         }

       // did you find a boundary edge of the \eps-\delta-cube? (you must find it!!)
       bool eps_delta_boundary_edge_found = false;

       // procedure for computing the generalized jumps:
       // 1. compute the center of the current face E: (x_E,y_E)
       // 2. one and only one of these values fulfiles abs()=1/2
       //    (i.e. we either have 'abs(x_E)=1/2' or 'abs(y_E)=1/2')
       //    without loss of generality we assume 'abs(y_E)=1/2', then
       //    we are in the setting of 'jump_up_down'
       // 3. Any 'jump_up_down' recieves a unique ID by
       //     jump up down over (x,-1/2) and (x,1/2) = jump_up_down[ (num_boundary_faces_per_direction/2) + (( x - (edge_length/2) ) / edge_length) ]
       //     (num_boundary_faces_per_direction/2) + (( x - (edge_length/2) ) / edge_length) is the corresponding ID
       // 4. the situation is identical for 'abs(x_E)=1/2'.


       // iterator over the elements of the periodic micro grid:
       IteratorType p_endit = auxiliaryDiscreteFunctionSpace_.end();
       for( IteratorType p_it = auxiliaryDiscreteFunctionSpace_.begin(); p_it != p_endit; ++p_it )
        {

          // --------- the 'inner entity' (micro grid) ------------

          const EntityType &micro_entity = *p_it;

          // one point quadrature formula ( A_h^{\eps}(y)=A^{\eps}(y_s) )
          EntityQuadratureType one_point_quadrature( micro_entity , 0 );

          // Q_h(u_H)(x_T,y) on the micro entity:
          PeriodicLocalFunctionType loc_Q_u_H_x_T = corrector_u_H_on_entity.localFunction(micro_entity);
          JacobianRangeType gradient_Q_u_H_x_T(0.);
          loc_Q_u_H_x_T.jacobian(one_point_quadrature[ 0 ], gradient_Q_u_H_x_T);

          // S denotes the micro grid element (i.e. 'micro_entity')
          const EntityGeometryType& geometry_S = micro_entity.geometry();
          // y_S denotes the barycenter of the micro grid element S:
          const DomainType &y_S = geometry_S.global(one_point_quadrature.point(0));

          // to evaluate A^{\epsilon}_h (in center of current inner entity):
          DomainType globalPoint;
          JacobianRangeType direction_of_diffusion;
          for( int k = 0; k < dimension; ++k )
           {
            direction_of_diffusion[ 0 ][ k ] = gradient_u_H[ 0 ][ k ] + gradient_Q_u_H_x_T[ 0 ][ k ];
            globalPoint[ k ] = x_T[ k ] + (delta * y_S[ k ]);
           }


          JacobianRangeType diffusive_flux_y_S;
          diffusion_.diffusiveFlux( globalPoint,
                                    direction_of_diffusion,
                                    diffusive_flux_y_S );

          if ( (fabs(y_S[1]) <= ((0.5*(epsilon_estimated/delta)))) &&
               (fabs(y_S[0]) <= ((0.5*(epsilon_estimated/delta))))    )
          {
          // ----------------------------------------------------

          IntersectionIteratorType p_endnit = auxGridPart.iend(micro_entity);
          for(IntersectionIteratorType p_nit = auxGridPart.ibegin(micro_entity); p_nit != p_endnit ; ++p_nit)
            {

               // Note: we are on the zero-centered unit cube! (That's why everything works!)

               FaceQuadratureType faceQuadrature( auxGridPart, *p_nit, 0, FaceQuadratureType::INSIDE );
               const FaceGeometryType& faceGeometry = p_nit->geometry();

               const RangeType edge_length = faceGeometry.volume();

               DomainType unitOuterNormal =
                       p_nit->unitOuterNormal(faceQuadrature.localPoint(0));

               const DomainType &edge_center = geometry_S.global(faceQuadrature.point(0));

               if ( (fabs(edge_center[0]) == ((0.5*(epsilon_estimated/delta)))) || 
                    (fabs(edge_center[1]) == ((0.5*(epsilon_estimated/delta)))) )
                {

                  //we use the generalized gradient jumps.

                  if ( (fabs(edge_center[0]) == ((0.5*(epsilon_estimated/delta)))) &&
                       (fabs(edge_center[1]) < ((0.5*(epsilon_estimated/delta))))   )
                   {
                    // + 0.2 to avoid rounding errors!
                    int id = int( ( num_boundary_faces_per_direction / 2 ) + (( edge_center[1] - (edge_length / 2.0) ) / edge_length) + 0.2 );

                    // unit outer normal creates the correct sign!
                    for( int k = 0; k < dimension; ++k )
                       jump_up_down[ id ] += pow( edge_length , 2.0 ) * ( diffusive_flux_y_S[ 0 ][ k ] * unitOuterNormal[ k ] );

                    //std :: cout << "edge_center = " << edge_center << std :: endl;
                    //std :: cout << "edge_length = " << edge_length << std :: endl;
                    //std :: cout << "unitOuterNormal = " << unitOuterNormal << std :: endl;
                    //std :: cout << "diffusive_flux_y_S = " << diffusive_flux_y_S[ 0 ] << std :: endl;
                    //std :: cout << "jump_up_down id = " << id << std :: endl;
                    //std :: cout << "jump_up_down[" << id << "] = " << jump_up_down[ id ] << std :: endl << std :: endl;

                    eps_delta_boundary_edge_found = true;
                    //std :: cout << "Found eps/delta boundary edge with center = " << edge_center << std :: endl;

                   }

                  if ( (fabs(edge_center[1]) == ((0.5*(epsilon_estimated/delta)))) &&
                       (fabs(edge_center[0]) < ((0.5*(epsilon_estimated/delta))))   )
                   {

                    // + 0.2 to avoid rounding errors!
                    int id = int( ( num_boundary_faces_per_direction / 2 ) + (( edge_center[0] - (edge_length / 2.0) ) / edge_length) + 0.2 );

                    // unit outer normal creates the correct sign!
                    for( int k = 0; k < dimension; ++k )
                       jump_left_right[ id ] += pow( edge_length , 2.0 ) * ( diffusive_flux_y_S[ 0 ][ k ] * unitOuterNormal[ k ] );

                    //std :: cout << "edge_center = " << edge_center << std :: endl;
                    //std :: cout << "edge_length = " << edge_length << std :: endl;
                    //std :: cout << "unitOuterNormal = " << unitOuterNormal << std :: endl;
                    //std :: cout << "diffusive_flux_y_S = " << diffusive_flux_y_S[ 0 ] << std :: endl;
                    //std :: cout << "jump_left_right id = " << id << std :: endl;
                    //std :: cout << "jump_left_right[" << id << "] = " << jump_left_right[ id ] << std :: endl << std :: endl;

                    eps_delta_boundary_edge_found = true;
                    //std :: cout << "Found eps/delta boundary edge with center = " << edge_center << std :: endl;

                   }

                }

               if ( ( fabs(edge_center[0]) < (0.5*(epsilon_estimated/delta)) ) && 
                    ( fabs(edge_center[1]) < (0.5*(epsilon_estimated/delta)) )  )
                {
                   // in this situation, p_nit always has an outside neighbor entity
                   // (use the normal gradient jumps)


                   // --------- the 'outer entity' (micro grid) ------------
                   //                ( neighbor entity )

                   EntityPointerType outer_p_it = p_nit->outside();
                   const EntityType &outer_micro_entity = *outer_p_it;

                   // one point quadrature formula ( A_h^{\eps}(y)=A^{\eps}(y_s) )
                   EntityQuadratureType outer_one_point_quadrature( outer_micro_entity , 0 );

                   // Q_h(u_H)(x_T,y) on the neighbor entity:
                   PeriodicLocalFunctionType outer_loc_Q_u_H_x_T = corrector_u_H_on_entity.localFunction(outer_micro_entity);
                   JacobianRangeType gradient_outer_Q_u_H_x_T(0.);
                   outer_loc_Q_u_H_x_T.jacobian(outer_one_point_quadrature[ 0 ], gradient_outer_Q_u_H_x_T);

                   // S denotes the micro grid element (i.e. 'micro_entity')
                   const EntityGeometryType& outer_geometry_S = outer_micro_entity.geometry();
                   // outer_y_S denotes the barycenter of the neighbor micro grid element of S:
                   const DomainType &outer_y_S = outer_geometry_S.global(outer_one_point_quadrature.point(0));

                   // to evaluate A^{\epsilon}_h (in center of current outer entity):
                   DomainType outer_globalPoint;
                   JacobianRangeType direction_of_diffusion_outside;
                   for( int k = 0; k < dimension; ++k )
                     {
                      outer_globalPoint[ k ] = x_T[ k ] + (delta * outer_y_S[ k ]);
                      direction_of_diffusion_outside[ 0 ][ k ] = gradient_u_H[ 0 ][ k ] +  gradient_outer_Q_u_H_x_T[ 0 ][ k ];
                     }

                   JacobianRangeType diffusive_flux_y_S_outside;
                   diffusion_.diffusiveFlux( outer_globalPoint,
                                             direction_of_diffusion_outside,
                                             diffusive_flux_y_S_outside );

                   // ----------------------------------------------------

                   RangeType aux_value = 0.0;

                   for( int k = 0; k < dimension; ++k )
                     aux_value += ( diffusive_flux_y_S_outside[ 0 ][ k ] - diffusive_flux_y_S[ 0 ][ k ] ) * unitOuterNormal[ k ] ;

                   local_indicator += pow( edge_length , 4.0 ) * pow( aux_value , 2.0 );
                }

            } // end Intersection iterator
        } // endif: "y_S \in \eps/\delta Y"

        }

       for( int id = 0; id < num_boundary_faces_per_direction; ++id )
         {
          local_indicator += ( pow( jump_up_down[ id ] , 2.0 ) + pow( jump_left_right[ id ] , 2.0 ) );
         }

       if ( eps_delta_boundary_edge_found == false )
        {
           std :: cout << "Error! Make sure that the restriction of the Y-triangulation on 'eps/delta Y' is a complete periodic triangulation of 'eps/delta Y' on its own. (for instance: delta = 2 epsilon should work)" << std :: endl; 
           std :: abort();
        }

       local_indicator *= entityVolume;

       return local_indicator;

     } // end of method
#endif

#endif

    // adaptive_refinement
    void adaptive_refinement( GridType &coarse_grid,
                              const DiscreteFunctionType& msfem_solution,
                              const DiscreteFunctionType& msfem_coarse_part,
                              const DiscreteFunctionType& msfem_fine_part,
                              std :: ofstream& data_file,
                              std :: string path )
    {
       std :: cout << "Starting error estimation..." << std :: endl;

       const DiscreteFunctionSpaceType& coarseDiscreteFunctionSpace = specifier_.coarseSpace();
       const LeafIndexSetType& coarseGridLeafIndexSet = coarseDiscreteFunctionSpace.gridPart().grid().leafIndexSet();
       
       // Coarse Entity Iterator 
       const IteratorType coarse_grid_end = coarseDiscreteFunctionSpace.end();
       for( IteratorType coarse_grid_it = coarseDiscreteFunctionSpace.begin(); coarse_grid_it != coarse_grid_end; ++coarse_grid_it )
        {
          int global_index_entity = coarseGridLeafIndexSet.index( *coarse_grid_it );
	}
#if 0





      // the coarse grid element T:
      const CoarseEntity &coarse_grid_entity = *coarse_grid_it;
      const CoarseGeometry &coarse_grid_geometry = coarse_grid_entity.geometry();
      assert( coarse_grid_entity.partitionType() == InteriorEntity );



      LocalMatrix local_matrix = global_matrix.localMatrix( coarse_grid_entity, coarse_grid_entity );

      const CoarseBaseFunctionSet &coarse_grid_baseSet = local_matrix.domainBaseFunctionSet();
      const unsigned int numMacroBaseFunctions = coarse_grid_baseSet.numBaseFunctions();
#endif
#if 0

          // the sub grid U(T) that belongs to the coarse_grid_entity T
          SubGridType& sub_grid_U_T = subgrid_list_.getSubGrid( global_index_entity );
          SubGridPart subGridPart( sub_grid_U_T );

          LocalDiscreteFunctionSpace localDiscreteFunctionSpace( subGridPart );

      LocalDiscreteFunction local_problem_solution_e0( "Local problem Solution e_0", localDiscreteFunctionSpace );
      local_problem_solution_e0.clear();

      LocalDiscreteFunction local_problem_solution_e1( "Local problem Solution e_1", localDiscreteFunctionSpace );
      local_problem_solution_e1.clear();

      // --------- load local solutions -------

      char location_lps[50];
      sprintf( location_lps, "/local_problems/_localProblemSolutions_%d", global_index_entity );
      std::string location_lps_s( location_lps );

      std :: string local_solution_location;

      // the file/place, where we saved the solutions of the cell problems
      local_solution_location = path_ + location_lps_s;

      bool reader_is_open = false;
      // reader for the cell problem data file:
      DiscreteFunctionReader discrete_function_reader( (local_solution_location).c_str() );
      reader_is_open = discrete_function_reader.open();

      if (reader_is_open)
        { discrete_function_reader.read( 0, local_problem_solution_e0 ); }

      if (reader_is_open)
        { discrete_function_reader.read( 1, local_problem_solution_e1 ); }



#endif

#if 0
      // 1 point quadrature!! We only need the gradient of the base function,
      // which is constant on the whole entity.
      CoarseQuadrature one_point_quadrature( coarse_grid_entity, 0 );

      // the barycenter of the macro_grid_entity
      const typename CoarseQuadrature::CoordinateType &local_coarse_point 
           = one_point_quadrature.point( 0 /*=quadraturePoint*/ );
      DomainType coarse_entity_barycenter = coarse_grid_geometry.global( local_coarse_point );

      // transposed of the the inverse jacobian
      const FieldMatrix< double, dimension, dimension > &inverse_jac
          = coarse_grid_geometry.jacobianInverseTransposed( local_coarse_point );

      for( unsigned int i = 0; i < numMacroBaseFunctions; ++i )
        {
          // jacobian of the base functions, with respect to the reference element
          typename CoarseBaseFunctionSet::JacobianRangeType gradient_Phi_ref_element;
          coarse_grid_baseSet.jacobian( i, one_point_quadrature[ 0 ], gradient_Phi_ref_element );

          // multiply it with transpose of jacobian inverse to obtain the jacobian with respect to the real entity
          inverse_jac.mv( gradient_Phi_ref_element[ 0 ], gradient_Phi[ i ][ 0 ] );
        }

      for( unsigned int i = 0; i < numMacroBaseFunctions; ++i )
        {

          for( unsigned int j = 0; j < numMacroBaseFunctions; ++j )
           {

            RangeType local_integral = 0.0;
   
            // iterator for the micro grid ( grid for the reference element T_0 )
            const LocalGridIterator local_grid_end = localDiscreteFunctionSpace.end();
            for( LocalGridIterator local_grid_it = localDiscreteFunctionSpace.begin(); local_grid_it != local_grid_end; ++local_grid_it )
              {
		
                const LocalGridEntity &local_grid_entity = *local_grid_it;
		
		// check if "local_grid_entity" (which is an entity of U(T)) is in T:
		// -------------------------------------------------------------------

                FineEntityPointer father_of_loc_grid_ent = localDiscreteFunctionSpace.grid().template getHostEntity<0>( local_grid_entity );

                for (int lev = 0; lev < specifier_.getLevelDifference() ; ++lev)
                       father_of_loc_grid_ent = father_of_loc_grid_ent->father();

                bool father_found = coarseGridLeafIndexSet.contains( *father_of_loc_grid_ent );
                while ( father_found == false )
                 {
                   father_of_loc_grid_ent = father_of_loc_grid_ent->father();
                   father_found = coarseGridLeafIndexSet.contains( *father_of_loc_grid_ent );
                 }

                bool entities_identical = true;
                int number_of_nodes = (*coarse_grid_it).template count<2>();
                for ( int k = 0; k < number_of_nodes; k += 1 )
                  {
                    if ( !(coarse_grid_it->geometry().corner(k) == father_of_loc_grid_ent->geometry().corner(k)) )
                      { entities_identical = false; }
                  }

                if ( entities_identical == false )
                  {
                   // std :: cout << "coarse_grid_it->geometry().corner(0) = " << coarse_grid_it->geometry().corner(0) << std :: endl;
                   // std :: cout << "coarse_grid_it->geometry().corner(1) = " << coarse_grid_it->geometry().corner(1) << std :: endl;
                   // std :: cout << "coarse_grid_it->geometry().corner(2) = " << coarse_grid_it->geometry().corner(2) << std :: endl;
                   // std :: cout << "father_of_loc_grid_ent->geometry().corner(0) = " << father_of_loc_grid_ent->geometry().corner(0) << std :: endl;
                   // std :: cout << "father_of_loc_grid_ent->geometry().corner(1) = " << father_of_loc_grid_ent->geometry().corner(1) << std :: endl;
                   // std :: cout << "father_of_loc_grid_ent->geometry().corner(2) = " << father_of_loc_grid_ent->geometry().corner(2) << std :: endl << std :: endl;
                   continue; 
                  }

                // -------------------------------------------------------------------

                const LocalGridGeometry &local_grid_geometry = local_grid_entity.geometry();
                assert( local_grid_entity.partitionType() == InteriorEntity );

                // higher order quadrature, since A^{\epsilon} is highly variable
                LocalGridQuadrature local_grid_quadrature( local_grid_entity, 2*localDiscreteFunctionSpace.order()+2 );
                const size_t numQuadraturePoints = local_grid_quadrature.nop();

                for( size_t localQuadraturePoint = 0; localQuadraturePoint < numQuadraturePoints; ++localQuadraturePoint )
                 {
                    // local (barycentric) coordinates (with respect to entity)
                    const typename LocalGridQuadrature::CoordinateType &local_subgrid_point = local_grid_quadrature.point( localQuadraturePoint );
		    
		    DomainType global_point_in_U_T = local_grid_geometry.global( local_subgrid_point );
		    
                    const double weight_local_quadrature 
                       = local_grid_quadrature.weight(  localQuadraturePoint ) * local_grid_geometry.integrationElement( local_subgrid_point );
		       
                    LocalGridLocalFunction localized_local_problem_solution_e0 = local_problem_solution_e0.localFunction( local_grid_entity );
                    LocalGridLocalFunction localized_local_problem_solution_e1 = local_problem_solution_e1.localFunction( local_grid_entity );

                    // grad coorector for e_0 and e_1
                    typename LocalGridBaseFunctionSet::JacobianRangeType grad_loc_sol_e0, grad_loc_sol_e1;
                    localized_local_problem_solution_e0.jacobian( local_grid_quadrature[ localQuadraturePoint ], grad_loc_sol_e0 );
                    localized_local_problem_solution_e1.jacobian( local_grid_quadrature[ localQuadraturePoint ], grad_loc_sol_e1 );
		    
		    // ∇ Phi_H + ∇ Q( Phi_H ) = ∇ Phi_H + ∂_x1 Phi_H Q( e_1 ) + ∂_x2 Phi_H Q( e_2 )
                    JacobianRangeType direction_of_diffusion( 0.0 );
                    for( int k = 0; k < dimension; ++k )
                      {
                        direction_of_diffusion[ 0 ][ k ] += gradient_Phi[ i ][ 0 ][ 0 ] * grad_loc_sol_e0[ 0 ][ k ];
                        direction_of_diffusion[ 0 ][ k ] += gradient_Phi[ i ][ 0 ][ 1 ] * grad_loc_sol_e1[ 0 ][ k ];
                        direction_of_diffusion[ 0 ][ k ] += gradient_Phi[ i ][ 0 ][ k ];
                      }

                    JacobianRangeType diffusive_flux( 0.0 );
                    diffusion_operator_.diffusiveFlux( global_point_in_U_T, direction_of_diffusion, diffusive_flux );

                    // if not Petrov-Galerkin:
                    #ifndef PGF
                    JacobianRangeType reconstruction_grad_phi_j( 0.0 );
                    for( int k = 0; k < dimension; ++k )
                      {
                        reconstruction_grad_phi_j[ 0 ][ k ] += gradient_Phi[ j ][ 0 ][ 0 ] * grad_loc_sol_e0[ 0 ][ k ];
                        reconstruction_grad_phi_j[ 0 ][ k ] += gradient_Phi[ j ][ 0 ][ 1 ] * grad_loc_sol_e1[ 0 ][ k ];
                        reconstruction_grad_phi_j[ 0 ][ k ] += gradient_Phi[ j ][ 0 ][ k ];
                      }

                    local_integral += weight_local_quadrature * ( diffusive_flux[ 0 ] *  reconstruction_grad_phi_j[ 0 ]);
                    #else
                    local_integral += weight_local_quadrature * ( diffusive_flux[ 0 ] * gradient_Phi[ j ][ 0 ]);
                    #endif

		  }
	       }

            // add entries
            local_matrix.add( j, i, local_integral );
           }

        }

    }
  
      
#endif
    }

}; // end of class MsFEMErrorEstimator

} // end namespace 

#endif
