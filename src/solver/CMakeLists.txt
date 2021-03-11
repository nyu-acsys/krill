
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
        default_solver.cpp
        default_solver.hpp
        encoder.cpp

        # backend
        z3encoder/z3error.cpp
        z3encoder/z3expr.cpp
        z3encoder/z3interface.cpp
        z3encoder/z3encoder.cpp
        z3encoder/z3visitor.cpp
        z3encoder/z3chkimp.cpp

        # post
#        post/info.cpp
#        post/assume.cpp
#        post/malloc.cpp
#        post/assign.cpp
#        post/assign_var.cpp
#        post/assign_deref.cpp
)

add_library(Solver ${SOURCES})
target_link_libraries(Solver CoLa HeaL ${Z3_LIBRARY})
