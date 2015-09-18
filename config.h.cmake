/* begin dune-multiscale */

/* Define to the version of dune-multiscale */
#define DUNE_MULTISCALE_VERSION "${DUNE_MULTISCALE_VERSION}"

/* Define to the major version of dune-multiscale */
#define DUNE_MULTISCALE_VERSION_MAJOR ${DUNE_MULTISCALE_VERSION_MAJOR}

/* Define to the minor version of dune-multiscale */
#define DUNE_MULTISCALE_VERSION_MINOR ${DUNE_MULTISCALE_VERSION_MINOR}

/* Define to the revision of dune-multiscale */
#define DUNE_MULTISCALE_VERSION_REVISION ${DUNE_MULTISCALE_VERSION_REVISION}

#undef COMMIT

/* the git tag / commit we build from */
#define COMMIT "@COMMIT@"

#define ALBERTA_DIM WORLDDIM
#cmakedefine @ENABLE_ALUGRID@ 
#cmakedefine @ENABLE_PETSC@ 
#cmakedefine @ENABLE_ABERTA@
#define PROBLEM_NINE_ONLY @PROBLEM_NINE_ONLY@

#define DUNE_MULTISCALE_USE_ISTL @USE_ISTL_BACKEND@
#define DUNE_MULTISCALE_WITH_DUNE_FEM @USE_FEM_BACKEND@
#define HAVE_RANDOM_PROBLEM @HAVE_RANDOM_PROBLEM@

#define DUNE_COMMON_FIELDVECTOR_SIZE_IS_METHOD 1

#ifndef HAVE_SIONLIB
    #define HAVE_SIONLIB 0
#endif

#if defined(NDEBUG) && not defined(DNDEBUG)
  #define DNDEBUG 1
#endif

#ifdef MS_TIMED_LOGGER
    #define MS_LOG         Dune::Stuff::Common::TimedLogger().get("msfem")
#else
    #define MS_LOG         Dune::Stuff::Common::Logger()
#endif

#define MS_LOG_INFO    MS_LOG.info()
#define MS_LOG_DEBUG   MS_LOG.debug()
#define MS_LOG_ERROR   MS_LOG.warn()
#define MS_LOG_DEVNULL DSC_LOG.devnull()
#define MS_LOG_INFO_0  \
    (Dune::MPIHelper::getCollectiveCommunication().rank() == 0 ? MS_LOG_INFO  : DSC_LOG.devnull())
#define MS_LOG_DEBUG_0 \
    (Dune::MPIHelper::getCollectiveCommunication().rank() == 0 ? MS_LOG_DEBUG : DSC_LOG.devnull())
#define MS_LOG_ERROR_0 \
    (Dune::MPIHelper::getCollectiveCommunication().rank() == 0 ? MS_LOG_ERROR : DSC_LOG.devnull())

#ifndef HAVE_DUNE_MULTISCALE_STATIC_DATA
    #include <string>
    static const std::string st_testdata_directory = "${CMAKE_CURRENT_SOURCE_DIR}/dune/multiscale/test";
    static constexpr unsigned int st_lagrangespace_order = 1;
    static constexpr size_t st_grid_dim = GRIDDIM;
    #define HAVE_DUNE_MULTISCALE_STATIC_DATA
#endif

#include <dune/stuff/common/timedlogging.hh>

/* end dune-multiscale */
