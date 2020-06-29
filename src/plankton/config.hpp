#pragma once
#ifndef PLANKTON_CONFIG
#define PLANKTON_CONFIG

#include <memory>
#include <vector>
#include "cola/ast.hpp"
#include "plankton/logic.hpp"
#include "plankton/properties.hpp"

namespace plankton {

	struct IncompleteConfigurationError : public std::exception {
		const std::string cause;
		IncompleteConfigurationError(std::string cause_) : cause(std::move(cause_)) {}
		virtual const char* what() const noexcept { return cause.c_str(); }
	};


	struct PlanktonConfig {
		bool conjoin_simplify = true;
		bool implies_holistic_check = true;
		bool z3_handle_unknown_result = false;

		virtual ~PlanktonConfig() = default;


		/** Returns true if updating the member field 'fieldname' of a node of type 'nodeType' does
		  * not change the flow sent out by the changed node on member fields other than 'fieldname'.
		  */
		virtual bool is_flow_field_local(const cola::Type& nodeType, std::string fieldname) = 0;

		/** Returns true if inserting a new node before an existing node, i.e., updating the heap from 'n1->{fieldname}=n3'
		  * to 'n1->{fieldname}=n2 /\ n2->{fieldname}=n3', inserts the keys of 'n2' into the data structure and leaves
		  * all other nodes unchanged, provided the keys contained in 'n2' are all strictly larger than the ones in 'n1'
		  * and strictly smaller than the ones in 'n3'.
		  */ 
		virtual bool is_flow_insertion_inbetween_local(const cola::Type& nodeType, std::string fieldname) = 0;

		/** Returns true if unlinking a node, i.e., updating the heap from 'n1->{fieldname}=n2 /\ n2->{fieldname}=n3'
		  * to 'n1->{fieldname}=n3', deletes the keys of 'n2' from the data structure and leaves all other nodes unchaged,
		  * provided the keys contained in 'n2' are all strictly larger than the ones in 'n1' and strictly smaller than
		  * the ones in 'n3'.
		  */
		virtual bool is_flow_unlinking_local(const cola::Type& nodeType, std::string fieldname) = 0;


		/** Returns all pairs of types T and selectors S of T where S is a pointer selector that may cause
		  * flow being transmitted to its successor.
		  */
		virtual const std::vector<std::pair<std::reference_wrapper<const cola::Type>, std::string>>& get_flow_transmitting_selectors() = 0;

		/** Returns a predicate 'pred(n,k)' deciding whether or not key 'k' is in the outflow of node 'n' (via any selector),
		  * assuming that 'k' is the flow of 'n'.
		  */
		virtual const Predicate& get_outflow_contains_key() = 0;

		/** Returns a predicate 'pred(n,k)' deciding whether or not key 'k' is in the outflow of node 'n' via selector 'fieldname',
		  * assuming that 'k' is the flow of 'n'. Requires the caller to provide the type of node 'n'.
		  */
		virtual const Predicate& get_outflow_contains_key(const cola::Type& nodeType, std::string fieldname) = 0;

		/** Returns a predicate 'pred(n,k)' deciding whether or not key 'k' is logically contained in node 'n'.
		  */
		virtual const Predicate& get_logically_contains_key() = 0;


		/** Returns the global node invariant.
		  */
		virtual const Invariant& get_invariant() = 0;
	};


	struct BaseConfig : public PlanktonConfig {
		virtual bool is_flow_field_local(const cola::Type&, std::string) {
			throw IncompleteConfigurationError("Flow field locality not configured.");
		}

		virtual bool is_flow_insertion_inbetween_local(const cola::Type&, std::string) {
			throw IncompleteConfigurationError("Flow insertion locality not configured.");
		}

		virtual bool is_flow_unlinking_local(const cola::Type&, std::string) {
			throw IncompleteConfigurationError("Flow unlinking locality not configured.");
		}

		virtual const std::vector<std::pair<std::reference_wrapper<const cola::Type>, std::string>>& get_flow_transmitting_selectors() {
			throw IncompleteConfigurationError("Flow transmitting selectors not configured.");
		}

		virtual const Predicate& get_outflow_contains_key() {
			throw IncompleteConfigurationError("Outflow not configured.");
		}

		virtual const Predicate& get_outflow_contains_key(const cola::Type&, std::string) {
			throw IncompleteConfigurationError("Outflow per selector not configured.");
		}

		virtual const Predicate& get_logically_contains_key() {
			throw IncompleteConfigurationError("Logical contains not configured.");
		}

		virtual const Invariant& get_invariant() {
			throw IncompleteConfigurationError("Invariant not configured.");
		}
	};


	extern std::unique_ptr<PlanktonConfig> config;

} // namespace plankton

#endif