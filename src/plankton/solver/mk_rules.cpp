#include "plankton/solver/encoder.hpp"

using namespace cola;
using namespace plankton;


void plankton::AddSolvingRulesToSolver(z3::solver /*solver*/) {
	throw std::logic_error("not yet implemented: plankton::AddSolvingRulesToSolver");
}


// using encoded_outflow_t = std::vector<std::tuple<z3::expr, z3::expr, z3::expr, z3::expr, z3::expr>>;

// inline encoded_outflow_t get_encoded_outflow(EncodingInfo& info) {
// 	Encoder encoder(info, false);
// 	encoded_outflow_t result;

// 	for (const auto& out : plankton::config->get_outflow_info()) {
// 		VariableDeclaration param_node("ptr", out.contains_key.vars.at(0)->type, false);
// 		VariableDeclaration param_key("key", out.contains_key.vars.at(1)->type, false);
// 		VariableDeclaration dummy_successor("suc", *out.node_type.field(out.fieldname), false);

// 		auto node = info.get_var(param_node, true);
// 		auto key = info.get_var(param_key, true);
// 		auto selector = info.get_sel(out.node_type, out.fieldname);
// 		auto successor = info.get_var(dummy_successor, true);
// 		auto key_flows_out = encoder.encode(*out.contains_key.instantiate(param_node, param_key));

// 		result.push_back(std::make_tuple(node, key, selector, successor, key_flows_out));
// 	}

// 	return result;
// }
// inline void add_rules_flow(EncodingInfo& info, const encoded_outflow_t& encoding) {
// 	for (auto [node, key, selector, successor, key_flows_out] : encoding) {
// 		// forall node,key,successor:  node->{selector}=successor /\ key in flow(node) /\ key_flows_out(node, key) ==> key in flow(successor)
// 		info.solver.add(z3::forall(node, key, successor, z3::implies(
// 			((info.heap(node, selector) == successor) && (info.flow(node, key) == info.mk_bool_val(true)) && key_flows_out),
// 			(info.flow(successor, key) == info.mk_bool_val(true))
// 		)));
// 	}
// }

// inline std::pair<z3::expr_vector, z3::expr> make_rules_keyset(EncodingInfo& info, const encoded_outflow_t& encoding) {
// 	// key in keyset(node)  :<==>  key in flow(node) /\ key notin outflow(node)
// 	auto node = std::get<0>(encoding.at(0));
// 	auto key = std::get<1>(encoding.at(0));

// 	z3::expr_vector conjuncts(info.context);
// 	for (const auto& elem : encoding) {
// 		auto src = info.mk_vector({ std::get<0>(elem), std::get<1>(elem) });
// 		auto dst = info.mk_vector({ node, key });
// 		auto renamed = z3::expr(std::get<4>(elem)).substitute(src, dst);
// 		conjuncts.push_back(!(renamed));
// 	}

// 	auto blueprint = (info.flow(node, key) == info.mk_bool_val(true)) && info.mk_and(conjuncts); // TODO: should be mk_or
// 	z3::expr_vector vars(info.context);
// 	vars.push_back(node);
// 	vars.push_back(key);

// 	return std::make_pair(vars, blueprint);
// }

// inline std::pair<z3::expr_vector, z3::expr> make_rules_contents(EncodingInfo& info, const z3::expr_vector& ks_vars, const z3::expr& ks_expr) {
// 	// key in DS content  :<==>  (exists node:  key in keyset(node) /\ contains(node, key))
// 	Encoder encoder(info, false);
// 	auto& property = plankton::config->get_logically_contains_key();

// 	VariableDeclaration param_node("ptr", property.vars.at(0)->type, false);
// 	VariableDeclaration param_key("key", property.vars.at(1)->type, false);

// 	auto node = info.get_var(param_node, true);
// 	auto key = info.get_var(param_key, true);
// 	auto logicallycontains = encoder.encode(*property.instantiate(param_node, param_key));

// 	z3::expr_vector replacement(info.context);
// 	replacement.push_back(node);
// 	replacement.push_back(key);
// 	auto keysetcontains = z3::expr(ks_expr).substitute(ks_vars, replacement);

// 	auto blueprint = z3::exists(node, keysetcontains && logicallycontains);
// 	z3::expr_vector vars(info.context);
// 	vars.push_back(key);

// 	return std::make_pair(vars, blueprint);
// }

// std::tuple<z3::expr_vector, z3::expr_vector, z3::expr, z3::expr> make_flow_builder_initializer(EncodingInfo& info) {
// 	// enable 'model based quantifier instantiation'
// 	// see: https://github.com/Z3Prover/z3/blob/master/examples/c%2B%2B/example.cpp#L366
// 	z3::params params(info.context);
// 	params.set("mbqi", true);
// 	info.solver.set(params);

// 	// encode
// 	auto encoded_outflow = get_encoded_outflow(info);

// 	// add flow rules to solver
// 	add_rules_flow(info, encoded_outflow);

// 	// get keyset and content rules as blueprint
// 	auto [ks_vars, ks_expr] = make_rules_keyset(info, encoded_outflow);
// 	auto [ds_vars, ds_expr] = make_rules_contents(info, ks_vars, ks_expr);

// 	// done
// 	return std::make_tuple(ks_vars, ds_vars, ks_expr, ds_expr);
// }

// FlowBuilder::FlowBuilder(EncodingInfo& info) : FlowBuilder(make_flow_builder_initializer(info)) {
// }