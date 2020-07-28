
################################
####### setting up build #######
################################

set(SOURCES
	logic.cpp
	visitors.cpp

	# util
	utilimpl/conjoin.cpp
	utilimpl/construct.cpp
	utilimpl/contains.cpp
	utilimpl/copy.cpp
	utilimpl/equal.cpp
	utilimpl/flatten.cpp
	utilimpl/print.cpp
	utilimpl/remove.cpp
	utilimpl/replace.cpp
)

add_library(HeaL ${SOURCES})
# add_dependencies(HeaL CoLa)
target_link_libraries(HeaL CoLa ${Z3_LIBRARY})
