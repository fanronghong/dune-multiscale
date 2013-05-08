dnl -*- autoconf -*-
# Macros needed to find dune-multiscale and dependent libraries.  They are called by
# the macros in ${top_src_dir}/dependencies.m4, which is generated by
# "dunecontrol autogen"

# Additional checks needed to build dune-multiscale
# This macro should be invoked by every module which depends on dune-multiscale, as
# well as by dune-multiscale itself
AC_DEFUN([DUNE_MULTISCALE_CHECKS],
[
	DUNE_BOOST_BASE(1.48, [DUNE_BOOST_SYSTEM] , [] )

  AX_BOOST_FILESYSTEM([1.48.0])
  AX_BOOST_SYSTEM([1.48.0])
  AX_BOOST_TIMER([1.48.0])
  AX_BOOST_CHRONO([1.48.0])

  AC_LANG([C++])
  AX_CXX_COMPILE_STDCXX_11([noext],[mandatory])
])

# Additional checks needed to find dune-multiscale
# This macro should be invoked by every module which depends on dune-multiscale, but
# not by dune-multiscale itself
AC_DEFUN([DUNE_MULTISCALE_CHECK_MODULE],
[
  DUNE_CHECK_MODULES([dune-multiscale],[multiscale/problems/base.hh])
])
