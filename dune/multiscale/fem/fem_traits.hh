// dune-multiscale
// Copyright Holders: Patrick Henning, Rene Milk
// License: BSD 2-Clause License (http://opensource.org/licenses/BSD-2-Clause)

#ifndef DUNE_FEM_TYPES_HH
#define DUNE_FEM_TYPES_HH

#include <dune/multiscale/common/traits.hh>
#include <dune/common/tuples.hh>

#include <dune/multiscale/fem/elliptic_fem_matrix_assembler.hh>
#include <dune/multiscale/common/righthandside_assembler.hh>

#include <dune/fem/space/common/adaptmanager.hh>
#include <dune/fem/io/file/dataoutput.hh>

#include <dune/fem/solver/petscsolver.hh>

namespace Dune {
namespace Multiscale {
namespace FEM {

//! Type constructions for the FEM problem
struct FEMTraits {



  typedef Dune::Fem::PetscInverseOperator< typename CommonTraits::DiscreteFunctionType,
                                           typename CommonTraits::FEMMatrix >
    InverseOperatorType;
  /** \brief --------------- solver for the linear system of equations ----------------------------
     * use Bi CG Stab [OEMBICGSTABOp] or GMRES [OEMGMRESOp] for non-symmetric matrices and CG [CGInverseOp] for symmetric
     ****ones. GMRES seems to be more stable, but is extremely slow!
     */


  //! --------------- the discrete operators (standard FEM) ----------------------------------
  //! discrete elliptic operator (corresponds with FEM Matrix)
  typedef DiscreteEllipticOperator<typename CommonTraits::DiscreteFunctionType, typename CommonTraits::DiffusionType>
       EllipticOperatorType;
  //! ----------------------------------------------------------------------------------------

}; // struct  FEMTraits

} //namespace FEM {
} //namespace Multiscale {
} //namespace Dune {

#endif // DUNE_FEM_TYPES_HH
