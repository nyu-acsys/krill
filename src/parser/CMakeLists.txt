
################################
####### setting up antlr #######
################################

#set(ANTLR4_ZIP_REPOSITORY ${CMAKE_CURRENT_SOURCE_DIR}/antlr/antlr4-4.9.2.zip)
#set(ANTLR_EXECUTABLE ${CMAKE_CURRENT_SOURCE_DIR}/antlr/antlr-4.9.2-complete.jar)
#
## building runtime
#list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/antlr)
#add_definitions(-DANTLR4CPP_STATIC)
#include(ExternalAntlr4Cpp)
#include_directories(${ANTLR4_INCLUDE_DIRS})
#
## creating parser from grammar
#find_package(ANTLR REQUIRED)
#antlr_target(PlanktonParser Plankton.g4 LEXER PARSER PACKAGE plankton VISITOR)
#include_directories(${ANTLR_PlanktonParser_OUTPUT_DIR})


# minimum required CMAKE version
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/extern)

# required if linking to static library
add_definitions(-DANTLR4CPP_STATIC)

# Specify the version of the antlr4 library needed for this project.
# By default the latest version of antlr4 will be used.  You can specify a
# specific, stable version by setting a repository tag value or a link
# to a zip file containing the libary source.
# set(ANTLR4_TAG 4.10.1)
# set(ANTLR4_ZIP_REPOSITORY https://github.com/antlr/antlr4/archive/refs/tags/4.10.1.zip)
set(ANTLR4_ZIP_REPOSITORY ${CMAKE_CURRENT_SOURCE_DIR}/extern/antlr4-4.10.1.zip)

# add external build for antlrcpp
include(ExternalAntlr4Cpp)
# add antrl4cpp artifacts to project environment
include_directories(${ANTLR4_INCLUDE_DIRS})

# set variable pointing to the antlr tool that supports C++
# this is not required if the jar file can be found under PATH environment
set(ANTLR_EXECUTABLE ${CMAKE_CURRENT_SOURCE_DIR}/extern/antlr-4.10.1-complete.jar)
# add macros to generate ANTLR Cpp code from grammar
find_package(ANTLR REQUIRED)

# Call macro to add lexer and grammar to your build dependencies.
#antlr_target(SampleGrammarLexer TLexer.g4 LEXER
#        PACKAGE antlrcpptest)
#antlr_target(SampleGrammarParser TParser.g4 PARSER
#        PACKAGE antlrcpptest
#        DEPENDS_ANTLR SampleGrammarLexer
#        COMPILE_FLAGS -lib ${ANTLR_SampleGrammarLexer_OUTPUT_DIR})
antlr_target(PlanktonParser Plankton.g4 LEXER PARSER PACKAGE plankton VISITOR)

# include generated files in project environment
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
)

add_library(Parser ${SOURCES} ${ANTLR_PlanktonParser_CXX_OUTPUTS})
target_link_libraries(Parser Programs Logics Engine antlr4_static)
