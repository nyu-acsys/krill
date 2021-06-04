
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
        solver.cpp
        default/encoder.cpp
        default/eval.cpp
        default/post.cpp
        default/post_assign.cpp
        default/join.cpp
        default/interference.cpp
        default/flowgraph.cpp
        default/scope.cpp
        default/helpers.cpp
)

add_library(Solver ${SOURCES})
target_link_libraries(Solver CoLa HeaL ${Z3_LIBRARY})
