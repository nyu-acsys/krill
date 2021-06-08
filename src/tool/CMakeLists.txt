
################################
####### setting up TCLAP #######
################################

set(TCLAP_REPO ${CMAKE_CURRENT_SOURCE_DIR}/tclap/tclap-1.2.2.tar.gz)

LIST(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/tclap)
include(ExternalTclap)


################################
####### setting up build #######
################################

add_executable(${TOOL_NAME} Main.cpp)
target_link_libraries(${TOOL_NAME} CoLa HeaL Prover Tclap)

add_executable(Test Test.cpp)
target_link_libraries(Test CoLa HeaL Solver)

add_executable(ToolTest ToolTest.cpp)
target_link_libraries(ToolTest CoLa HeaL Solver Prover)


################################
######### installation #########
################################

install(TARGETS ${TOOL_NAME} DESTINATION ${INSTALL_FOLDER})
