
################################
####### setting up TCLAP #######
################################

set(TCLAP_REPO ${CMAKE_CURRENT_SOURCE_DIR}/tclap/tclap-1.2.2.tar.gz)

LIST(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/tclap)
include(ExternalTclap)


################################
####### setting up build #######
################################

#add_executable(${TOOL_NAME} Main.cpp)
#target_link_libraries(${TOOL_NAME} CoLa HeaL Prover Tclap)
#
#add_executable(Test Test.cpp)
#target_link_libraries(Test CoLa HeaL Solver)

set(BENCHMARKS
        VechevYahavDCAS
        ORVYY
        ORVYYmod
        VechevYahavCAS
        Michael
)

foreach (ENTRY IN LISTS BENCHMARKS)
    set(NAME benchmark_${ENTRY})
    add_executable(${NAME} ${ENTRY}.cpp)
    target_link_libraries(${NAME} CoLa HeaL Solver Prover)
    install(TARGETS ${NAME} DESTINATION ${INSTALL_FOLDER})
endforeach()

#add_executable(VechevYahavDCAS VechevYahavDCAS.cpp)
#target_link_libraries(VechevYahavDCAS CoLa HeaL Solver Prover)
#install(TARGETS VechevYahavDCAS DESTINATION ${INSTALL_FOLDER})


################################
######### installation #########
################################

#install(TARGETS ${TOOL_NAME} DESTINATION ${INSTALL_FOLDER})
