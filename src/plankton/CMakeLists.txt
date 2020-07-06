
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
	logic.cpp
	verify.cpp
	verify_interference.cpp
	# util
	util/conjoin.cpp
	util/contains.cpp
	util/copy.cpp
	util/equal.cpp
	util/flatten.cpp
	util/print.cpp
	util/replace.cpp
	# post
	solver/encoder.cpp
	solver/mk_rules.cpp
	solver/mk_solver.cpp
	solver/solver.cpp
	solver/solverimpl.cpp
	solver/scopes.cpp
	solver/invariants.cpp
	solver/implication.cpp
	solver/join.cpp
	solver/post.cpp
)

add_library(Plankton ${SOURCES})
# add_dependencies(Plankton CoLa)
target_link_libraries(Plankton CoLa ${Z3_LIBRARY})
