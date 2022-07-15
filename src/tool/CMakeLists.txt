
################################
####### setting up TCLAP #######
################################

# TODO: update tclap version

set(TCLAP_REPO_URL ${CMAKE_CURRENT_SOURCE_DIR}/tclap/tclap-1.2.4.tar.gz)
set(TCLAP_REPO_HASH 733b26a2dae2ae8acc46fc37974d6d76)

LIST(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/tclap)
include(ExternalTclap)


################################
####### setting up build #######
################################

add_executable(${TOOL_NAME} main.cpp)
target_link_libraries(${TOOL_NAME} Programs Logics Engine Parser Tclap)
install(TARGETS ${TOOL_NAME} DESTINATION ${INSTALL_FOLDER})
