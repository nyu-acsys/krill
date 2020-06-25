#pragma once
#ifndef PLANKTON_CONFIG
#define PLANKTON_CONFIG


namespace plankton {

	struct PlanktonConfig {
		bool conjoin_simplify = true;
		bool implies_holistic_check = true;
		bool z3_handle_unknown_result = false;
		// bool post_simplify = true;
		// bool interference_mover_optimization = true;
		// bool interference_insertion_at_begin = true;
	};

	extern PlanktonConfig config;

} // namespace plankton

#endif