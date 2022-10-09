
################################
####### setting up TCLAP #######
################################

set(TCLAP_REPO_URL ${CMAKE_CURRENT_SOURCE_DIR}/tclap/tclap-1.4.0-rc1.tar.bz2)
set(TCLAP_REPO_HASH 3e669714a4dba6e5451bb401bb4e3d54)

LIST(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/tclap)
include(ExternalTclap)


################################
####### setting up build #######
################################

add_executable(${TOOL_NAME} main.cpp)
target_link_libraries(${TOOL_NAME} Programs Logics Engine Parser Tclap)
install(TARGETS ${TOOL_NAME} DESTINATION ${INSTALL_FOLDER})

add_executable(krill main2.cpp)
target_link_libraries(krill Programs Logics Engine Parser Tclap)
install(TARGETS krill DESTINATION ${INSTALL_FOLDER})
