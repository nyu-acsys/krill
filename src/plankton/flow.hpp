#pragma once
#ifndef PLANKTON_FLOW
#define PLANKTON_FLOW


#include <ostream>


namespace plankton {

	struct FlowValue {
		static FlowValue empty() { throw std::logic_error("not yet implemented: FlowValue::empty()"); }
	};
	
	inline void print(const FlowValue& /*formula*/, std::ostream& stream) {
		stream << "?";
	}

} // namespace plankton

#endif