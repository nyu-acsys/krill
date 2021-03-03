
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
	util.cpp
	flow.cpp
	solver.cpp

	# verify
	verifyimpl/access.cpp
	verifyimpl/effects.cpp
	verifyimpl/interference.cpp
	verifyimpl/proof.cpp
	verifyimpl/renaming.cpp
	verifyimpl/verify.cpp

	# backend
	backend/z3error.cpp
	backend/z3expr.cpp
	backend/z3interface.cpp
	backend/z3encoder.cpp
	backend/z3visitor.cpp
	backend/z3chkimp.cpp

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
)

add_library(Prover ${SOURCES})
target_link_libraries(Prover CoLa HeaL ${Z3_LIBRARY})
