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

	/** Returns true if inserting a new node before an existing node, i.e., updating the heap from 'n1->{fieldname}=n3'
	  * to 'n1->{fieldname}=n2 /\ n2->{fieldname}=n3', inserts the keys of 'n2' into the data structure and leaves
	  * all other nodes unchanged, provided the keys contained in 'n2' are all strictly larger than the ones in 'n1'
	  * and strictly smaller than the ones in 'n3'.
	  */ 
	inline bool is_flow_insertion_inbetween_local(const cola::Type& /*nodeType*/, std::string /*fieldname*/) {
		throw std::logic_error("not yet implemented: plankton::is_flow_insertion_inbetween_local");
	}

	/** Returns true if unlinking a node, i.e., updating the heap from 'n1->{fieldname}=n2 /\ n2->{fieldname}=n3'
	  * to 'n1->{fieldname}=n3', deletes the keys of 'n2' from the data structure and leaves all other nodes unchaged,
	  * provided the keys contained in 'n2' are all strictly larger than the ones in 'n1' and strictly smaller than
	  * the ones in 'n3'.
	  */
	inline bool is_flow_unlinking_local(const cola::Type& /*nodeType*/, std::string /*fieldname*/) {
		throw std::logic_error("not yet implemented: plankton::is_flow_unlinking_local");
	}

} // namespace plankton

#endif