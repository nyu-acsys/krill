
################################
####### setting up antlr #######
################################

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/antlr)

set(ANTLR_EXECUTABLE ${CMAKE_CURRENT_SOURCE_DIR}/antlr/antlr-4.10.1-complete.jar)
set(ANTLR4_ZIP_REPOSITORY ${CMAKE_CURRENT_SOURCE_DIR}/antlr/antlr4-4.10.1.zip)
set(ANTLR4_ZIP_HASH 7a217c4de4d400163e12b554dbb5cf1e)

add_definitions(-DANTLR4CPP_STATIC)
include(ExternalAntlr4Cpp)
include_directories(${ANTLR4_INCLUDE_DIRS})

find_package(ANTLR REQUIRED)
antlr_target(PlanktonParser Plankton.g4 LEXER PARSER PACKAGE plankton VISITOR)
include_directories(${ANTLR_PlanktonParser_OUTPUT_DIR})


################################
######### setting build ########
################################

set(SOURCES
        parse.cpp

        builder/builder.cpp
        builder/config.cpp
        builder/expr.cpp
        builder/function.cpp
        builder/invariant.cpp
        builder/lookup.cpp
        builder/program.cpp
        builder/stmt.cpp
        builder/type.cpp
        builder/graphs.cpp
)

add_library(Parser ${SOURCES} ${ANTLR_PlanktonParser_CXX_OUTPUTS})
target_link_libraries(Parser Programs Logics Engine antlr4_static)
