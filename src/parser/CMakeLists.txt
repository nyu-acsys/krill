
## minimum required CMAKE version
#CMAKE_MINIMUM_REQUIRED(VERSION 3.7 FATAL_ERROR)
#
#list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/antlr)
#
## compiler must be 11 or 14
#set(CMAKE_CXX_STANDARD 17)
#
## required if linking to static library
#add_definitions(-DANTLR4CPP_STATIC)
#
## add external build for antlrcpp
#include(ExternalAntlr4Cpp)
## add antrl4cpp artifacts to project environment
#include_directories(${ANTLR4_INCLUDE_DIRS})
#
## set variable pointing to the antlr tool that supports C++
## this is not required if the jar file can be found under PATH environment
#set(ANTLR4_ZIP_REPOSITORY ${CMAKE_CURRENT_SOURCE_DIR}/antlr/antlr4-4.9.2.zip)
## add macros to generate ANTLR Cpp code from grammar
#find_package(ANTLR REQUIRED)
#
## Call macro to add lexer and grammar to your build dependencies.
#antlr_target(SampleGrammarLexer TLexer.g4 LEXER
#        PACKAGE antlrcpptest)
#antlr_target(SampleGrammarParser TParser.g4 PARSER
#        PACKAGE antlrcpptest
#        DEPENDS_ANTLR SampleGrammarLexer
#        COMPILE_FLAGS -lib ${ANTLR_SampleGrammarLexer_OUTPUT_DIR})
#
## include generated files in project environment
#include_directories(${ANTLR_SampleGrammarLexer_OUTPUT_DIR})
#include_directories(${ANTLR_SampleGrammarParser_OUTPUT_DIR})
#
## add generated grammar to demo binary target
#add_executable(demo main.cpp
#        ${ANTLR_SampleGrammarLexer_CXX_OUTPUTS}
#        ${ANTLR_SampleGrammarParser_CXX_OUTPUTS})
#target_link_libraries(demo antlr4_static)



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
        builder/cmd.cpp
        builder/function.cpp
        builder/lookup.cpp
        builder/program.cpp
        builder/stmt.cpp
        builder/type.cpp
        builder/variable.cpp
)

add_library(Parser ${SOURCES} ${ANTLR_PlanktonParser_CXX_OUTPUTS})
target_link_libraries(Parser Programs Logics antlr4_static)
#add_dependencies(Parser antlr4_static)
#target_link_libraries(Parser PRIVATE antlr4_static)
