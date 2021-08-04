
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
#        solver/

        proof/common.cpp
        proof/api.cpp
        proof/cmd.cpp
        proof/stmt.cpp

        linearizability.cpp
)

add_library(Engine ${SOURCES})
target_link_libraries(Engine CoLa HeaL ${Z3_LIBRARY})
