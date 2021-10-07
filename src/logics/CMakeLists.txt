
set(SOURCES
        ast.cpp
        visitors.cpp

        util/collect.cpp
        util/copy.cpp
        util/equal.cpp
        util/memory.cpp
        util/normalize.cpp
        util/print.cpp
        util/rename.cpp
        util/simplify.cpp
)

add_library(Logics ${SOURCES})
target_link_libraries(Logics Programs)
