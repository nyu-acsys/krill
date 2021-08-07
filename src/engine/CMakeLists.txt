
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
        solver/post_assign_var.cpp
        solver/post_assign_read.cpp
        solver/post_assign_write.cpp
        solver/post_assume.cpp
        solver/post_malloc.cpp
        solver/scope.cpp
        solver/solver.cpp

        proof/common.cpp
        proof/api.cpp
        proof/cmd.cpp
        proof/stmt.cpp

        util_eval.cpp
        util_symbolic.cpp

        linearizability.cpp
)

add_library(Engine ${SOURCES})
target_link_libraries(Engine CoLa HeaL ${Z3_LIBRARY})
