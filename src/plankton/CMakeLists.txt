
################################
######## setting up z3 #########
################################

# TODO: check version
find_library(Z3_LIBRARY NAMES z3 REQUIRED)
find_path(Z3_INCLUDE NAMES "z3++.h" REQUIRED)
include_directories(${Z3_INCLUDE})


################################
####### setting up build #######
################################

set(SOURCES
	config.cpp
	error.cpp
	solver.cpp

	# verify
	verifyimpl/access.cpp
	verifyimpl/effects.cpp
	verifyimpl/interference.cpp
	verifyimpl/proof.cpp
	verifyimpl/renaming.cpp
	verifyimpl/verify.cpp

	# solver
	solverimpl/linsolver.cpp
	solverimpl/make.cpp
	solverimpl/join.cpp
	solverimpl/scopes.cpp
	solverimpl/post/info.cpp
	solverimpl/post/assume.cpp
	solverimpl/post/malloc.cpp
	solverimpl/post/assign.cpp
	solverimpl/post/assign_var.cpp
	solverimpl/post/assign_deref.cpp

	# backend
)

add_library(Plankton ${SOURCES})
# add_dependencies(Plankton CoLa)
target_link_libraries(Plankton CoLa HeaL ${Z3_LIBRARY})
