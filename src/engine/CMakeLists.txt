
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
        solver/access.cpp
        solver/interference.cpp
        solver/join.cpp
        solver/post_assign_var.cpp
        solver/post_assign_read.cpp
        solver/post_assign_write.cpp
        solver/post_assume.cpp
        solver/post_malloc.cpp
        solver/scope.cpp
        solver/solver.cpp
        solver/stability.cpp

        proof/common.cpp
        proof/api.cpp
        proof/cmd.cpp
        proof/stmt.cpp

        util_effects.cpp
        util_eval.cpp
        util_memory.cpp
        util_stack.cpp
        util_symbolic.cpp

        encoding.cpp
        encoding_encode.cpp
        encoding_solve.cpp

        flowgraph.cpp
        flowgraph_make.cpp

        linearizability.cpp
        static.cpp
)

add_library(Engine ${SOURCES})
target_link_libraries(Engine CoLa HeaL ${Z3_LIBRARY})
