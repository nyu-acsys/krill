
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
	util/contains.cpp
	util/copy.cpp
	util/equal.cpp
	util/instantiate.cpp
	util/print.cpp
	util/replace.cpp
	util/solving.cpp
	verify.cpp
	verify_interference.cpp
	post.cpp
)

add_library(Plankton ${SOURCES})
# add_dependencies(Plankton CoLa)
target_link_libraries(Plankton CoLa ${Z3_LIBRARY})
