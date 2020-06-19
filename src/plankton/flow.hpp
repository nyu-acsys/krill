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

	/** Returns true if updating the member field 'fieldname' of a node of type 'nodeType' does
	  * not change the flow sent out by the changed node on member fields other than 'fieldname'.
	  */
	inline bool is_flow_field_local(const cola::Type& /*nodeType*/, std::string /*fieldname*/) {
		throw std::logic_error("not yet implemented: plankton::is_flow_field_local");
	}

} // namespace plankton

#endif