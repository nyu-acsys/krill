#pragma once
#ifndef PLANKTON_FLOW
#define PLANKTON_FLOW


#include <ostream>


namespace plankton {

	struct FlowValue {
	};
	
	inline void print(const FlowValue& /*formula*/, std::ostream& stream) {
		stream << "?";
	}

} // namespace plankton

#endif