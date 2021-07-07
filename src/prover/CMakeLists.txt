
################################
######## setting up z3 #########
################################

## TODO: check version
#find_library(Z3_LIBRARY NAMES z3 REQUIRED)
#find_path(Z3_INCLUDE NAMES "z3++.h" REQUIRED)
#include_directories(${Z3_INCLUDE})


################################
####### setting up build #######
################################

set(SOURCES
	proof.cpp
	verify.cpp
)

add_library(Prover ${SOURCES})
target_link_libraries(Prover Solver CoLa HeaL)
#target_link_libraries(Prover CoLa HeaL ${Z3_LIBRARY})
