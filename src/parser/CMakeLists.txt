
################################
####### setting up antlr #######
################################

set(ANTLR4_ZIP_REPOSITORY ${CMAKE_CURRENT_SOURCE_DIR}/antlr/antlr4-4.9.2.zip)
set(ANTLR_EXECUTABLE ${CMAKE_CURRENT_SOURCE_DIR}/antlr/antlr-4.9.2-complete.jar)

# building runtime
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/antlr)
add_definitions(-DANTLR4CPP_STATIC)
include(ExternalAntlr4Cpp)
include_directories(${ANTLR4_INCLUDE_DIRS})

# creating parser from grammar
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
        builder/formula.cpp
        builder/function.cpp
        builder/lookup.cpp
        builder/program.cpp
        builder/stmt.cpp
        builder/type.cpp
)

add_library(Parser ${SOURCES} ${ANTLR_PlanktonParser_CXX_OUTPUTS})
target_link_libraries(Parser Programs Logics Engine antlr4_static)

add_executable(ParserTest test.cpp)
target_link_libraries(ParserTest Parser)