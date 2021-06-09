
################################
####### setting up build #######
################################

set(SOURCES
	logic.cpp
	visitors.cpp

	# util
	util/conjoin.cpp
#	util/construct.cpp
#	util/contains.cpp
	util/copy.cpp
#	util/equal.cpp
	util/simplify.cpp
	util/print.cpp
#	util/replace.cpp
	util/symbolic.cpp
)

add_library(HeaL ${SOURCES})
target_link_libraries(HeaL CoLa)
