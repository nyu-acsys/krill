#include "plankton/post.hpp"

#include <iostream> // TODO: remove
#include <sstream>
#include <type_traits>
#include "cola/util.hpp"
#include "plankton/util.hpp"
#include "plankton/error.hpp"

using namespace cola;
using namespace plankton;


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<Annotation> plankton::post_full(std::unique_ptr<Annotation> /*pre*/, const Assume& /*cmd*/) {
	throw std::logic_error("not yet implemented: plankton::post(std::unique_ptr<Annotation>, const Assume&)");
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<Annotation> plankton::post_full(std::unique_ptr<Annotation> /*pre*/, const Malloc& /*cmd*/) {
	// TODO: extend_current_annotation(... knowledge about all members of new object, flow...)
	// TODO: destroy knowledge about current lhs (and everything that is not guaranteed to survive the assignment)
	throw std::logic_error("not yet implemented: plankton::post(std::unique_ptr<Annotation>, const Malloc&)");
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

struct OwnershipChecker : public LogicNonConstVisitor {
	const Expression& prune_expr;
	bool prune_child = false;
	bool prune_value = true;
	OwnershipChecker(const Expression& expr) : prune_expr(expr) {}

	template<typename T>
	void handle_formula(std::unique_ptr<T>& formula) {
		formula->accept(*this);
		if (prune_child) {
			formula = std::make_unique<ExpressionFormula>(std::make_unique<BooleanValue>(prune_value));
			prune_child = false;
		}
	}

	void visit(ConjunctionFormula& formula) override {
		for (auto& conjunct : formula.conjuncts) {
			handle_formula(conjunct);
		}
	}
	
	void visit(NegatedFormula& formula) override {
		bool old_value = prune_value;
		prune_value = !prune_value;
		handle_formula(formula.formula);
		prune_value = old_value;
	}

	void visit(ImplicationFormula& formula) override {
		// do not touch formula.precondition
		handle_formula(formula.conclusion);
	}

	void visit(OwnershipFormula& formula) override {
		if (plankton::syntactically_equal(*formula.expr, prune_expr)) {
			prune_child = true;
		}
	}

	void visit(ExpressionFormula& /*formula*/) override { /* do nothing */ }
	void visit(LogicallyContainedFormula& /*formula*/) override { /* do nothing */ }
	void visit(FlowFormula& /*formula*/) override { /* do nothing */ }
	void visit(ObligationFormula& /*formula*/) override { /* do nothing */ }
	void visit(FulfillmentFormula& /*formula*/) override { /* do nothing */ }
	void visit(PastPredicate& /*formula*/) override { throw std::logic_error("Unexpected invocation: OwnershipChecker::visit(PastPredicate&)"); }
	void visit(FuturePredicate& /*formula*/) override { throw std::logic_error("Unexpected invocation: OwnershipChecker::visit(FuturePredicate&)"); }
	void visit(Annotation& /*formula*/) override { throw std::logic_error("Unexpected invocation: OwnershipChecker::visit(Annotation&)"); }
};

std::unique_ptr<Formula> destroy_ownership(std::unique_ptr<Formula> formula, const Expression& expr) {
	// remove ownership of 'expr' in 'uptrcontainer'
	OwnershipChecker visitor(expr);
	visitor.handle_formula(formula);
	return formula;
}

template<typename T>
T container_search_and_destroy(T&& uptrcontainer, const Expression& destroy) {
	// remove knowledge about 'destroy' in 'uptrcontainer'
	T new_container;
	for (auto& elem : uptrcontainer) {
		if (!plankton::contains_expression(*elem, destroy)) {
			new_container.push_back(std::move(elem));
		}
	}
	return std::move(new_container);
}

template<typename T>
T container_search_and_inline(T&& uptrcontainer, const Expression& search, const Expression& replacement) {
	// copy knowledge about 'search' in 'uptrcontainer' over to 'replacement'
	T new_elements;
	for (auto& elem : uptrcontainer) {
		if (plankton::contains_expression(*elem, search) && !plankton::contains_expression_within_obligation(*elem, search)) {
			new_elements.push_back(replace_expression(plankton::copy(*elem), search, replacement));
		}
	}
	uptrcontainer.insert(uptrcontainer.end(), std::make_move_iterator(new_elements.begin()), std::make_move_iterator(new_elements.end()));
	return std::move(uptrcontainer);
}

std::unique_ptr<Annotation> search_and_destroy_and_inline_var(std::unique_ptr<Annotation> pre, const VariableExpression& lhs, const Expression& rhs) {
	// destroy knowledge about lhs
	auto pruned = destroy_ownership(std::move(pre->now), rhs);
	auto flat = plankton::flatten(std::move(pruned));
	flat->conjuncts = container_search_and_destroy(std::move(flat->conjuncts), lhs);
	pre->time = container_search_and_destroy(std::move(pre->time), lhs);

	// copy knowledge about rhs over to lhs
	flat->conjuncts = container_search_and_inline(std::move(flat->conjuncts), rhs, lhs);
	pre->time = container_search_and_inline(std::move(pre->time), rhs, lhs); // TODO important: are those really freely duplicable?

	// add new knowledge
	auto equality = std::make_unique<ExpressionFormula>(std::make_unique<BinaryExpression>(BinaryExpression::Operator::EQ, cola::copy(lhs), cola::copy(rhs)));
	flat->conjuncts.push_back(std::move(equality));
	pre->now = std::move(flat);

	// done
	return pre;
}

inline void check_purity(const VariableExpression& lhs) {
	if (lhs.decl.is_shared) {
		// assignment to 'lhs' potentially impure (may require obligation/fulfillment transformation)
		// TODO: implement
		throw UnsupportedConstructError("assignment to shared variable '" + lhs.decl.name + "'; assignment potentially impure.");
	}
}

std::unique_ptr<Annotation> post_full_assign_var_expr(std::unique_ptr<Annotation> pre, const Assignment& /*cmd*/, const VariableExpression& lhs, const Expression& rhs) {
	check_purity(lhs);
	return search_and_destroy_and_inline_var(std::move(pre), lhs, rhs);
}

std::unique_ptr<Annotation> post_full_assign_var_derefptr(std::unique_ptr<Annotation> pre, const Assignment& /*cmd*/, const VariableExpression& lhs, const Dereference& rhs, const VariableExpression& /*rhsVar*/) {
	check_purity(lhs);
	auto post = search_and_destroy_and_inline_var(std::move(pre), lhs, rhs);
	// TODO: infer flow/contains and put it into a past predicate
	// TODO: no self loops? Add lhs != rhs?
	// TODO: use invariant to find out stuff?
	throw std::logic_error("not yet implemented: post_full_assign_var_derefptr");
}

std::unique_ptr<Annotation> post_full_assign_var_derefdata(std::unique_ptr<Annotation> pre, const Assignment& /*cmd*/, const VariableExpression& lhs, const Dereference& rhs, const VariableExpression& /*rhsVar*/) {
	check_purity(lhs);
	// TODO: use invariant to find out stuff?
	return search_and_destroy_and_inline_var(std::move(pre), lhs, rhs);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

struct DereferenceExtractor : public BaseVisitor, public LogicVisitor {
	using BaseVisitor::visit;
	using LogicVisitor::visit;

	std::string search_field;
	std::deque<std::unique_ptr<Expression>> result;
	DereferenceExtractor(std::string search) : search_field(search) {}

	static std::deque<std::unique_ptr<Expression>> extract(const Formula& formula, std::string fieldname) {
		DereferenceExtractor visitor(fieldname);
		formula.accept(visitor);
		return std::move(visitor.result);
	}

	void visit(const Dereference& node) override {
		node.expr->accept(*this);
		result.push_back(cola::copy(node));
	}
	void visit(const BooleanValue& /*node*/) override { /* do nothing */ }
	void visit(const NullValue& /*node*/) override { /* do nothing */ }
	void visit(const EmptyValue& /*node*/) override { /* do nothing */ }
	void visit(const MaxValue& /*node*/) override { /* do nothing */ }
	void visit(const MinValue& /*node*/) override { /* do nothing */ }
	void visit(const NDetValue& /*node*/) override { /* do nothing */ }
	void visit(const VariableExpression& /*node*/) override { /* do nothing */ }
	void visit(const NegatedExpression& node) override { node.expr->accept(*this); }
	void visit(const BinaryExpression& node) override { node.lhs->accept(*this); node.rhs->accept(*this); }
	void visit(const ExpressionFormula& formula) override { formula.expr->accept(*this); }
	void visit(const OwnershipFormula& formula) override { formula.expr->accept(*this); }
	void visit(const LogicallyContainedFormula& formula) override { formula.expr->accept(*this); }
	void visit(const FlowFormula& formula) override { formula.expr->accept(*this); }
	void visit(const ObligationFormula& formula) override { formula.spec->key->accept(*this); }
	void visit(const FulfillmentFormula& formula) override { formula.spec->key->accept(*this); }
	void visit(const NegatedFormula& formula) override { formula.formula->accept(*this); }
	void visit(const ConjunctionFormula& formula) override {
		for (const auto& conjunct : formula.conjuncts) {
			conjunct->accept(*this);
		}
	}
	void visit(const ImplicationFormula& formula) override { formula.premise->accept(*this); formula.conclusion->accept(*this); }
	void visit(const PastPredicate& /*formula*/) override { throw std::logic_error("Unexpected invocation: DereferenceExtractor::visit(const PastPredicate&)"); }
	void visit(const FuturePredicate& /*formula*/) override { throw std::logic_error("Unexpected invocation: DereferenceExtractor::visit(const FuturePredicate&)"); }
	void visit(const Annotation& /*formula*/) override { throw std::logic_error("Unexpected invocation: DereferenceExtractor::visit(const Annotation&)"); }
};

std::unique_ptr<ConjunctionFormula> search_and_destroy_derefs(std::unique_ptr<ConjunctionFormula> now, const Dereference& lhs) {
	auto expr = std::make_unique<BinaryExpression>(
		BinaryExpression::Operator::NEQ, std::make_unique<Dereference>(std::make_unique<NullValue>(), lhs.fieldname), cola::copy(lhs)
	);
	auto& uptr = expr->lhs;
	auto inequality = std::make_unique<ExpressionFormula>(std::move(expr));
	

	// find all dereferences that may be affected (are not guaranteed to be destroyed)
	auto dereferences = DereferenceExtractor::extract(*now, lhs.fieldname);
	for (auto& deref : dereferences) {
		uptr = std::move(deref);
		deref.reset();
		if (!plankton::implies(*now, *inequality)) {
			deref = std::move(uptr);
		}
	}

	// delete knowlege about potentially affected dereferences
	for (auto& deref : dereferences) {
		if (deref) {
			now->conjuncts = container_search_and_destroy(std::move(now->conjuncts), *deref);
		}
	}

	return now;
}

std::unique_ptr<Annotation> search_and_destroy_and_inline_deref(std::unique_ptr<Annotation> pre, const Dereference& lhs, const Expression& rhs) {
	// destroy knowledge about lhs (do not modify TimePredicates)
	auto pruned = destroy_ownership(std::move(pre->now), rhs);
	auto flat = plankton::flatten(std::move(pruned));
	flat = search_and_destroy_derefs(std::move(flat), lhs);
	// do not modify 

	// copy knowledge about rhs over to lhs (do not modify TimePredicates)
	flat->conjuncts = container_search_and_inline(std::move(flat->conjuncts), rhs, lhs);

	// add new knowledge
	auto equality = std::make_unique<ExpressionFormula>(std::make_unique<BinaryExpression>(BinaryExpression::Operator::EQ, cola::copy(lhs), cola::copy(rhs)));
	flat->conjuncts.push_back(std::move(equality));
	pre->now = std::move(flat);

	// done
	return pre;
}

struct PurityChecker {
	std::deque<std::unique_ptr<VariableDeclaration>> dummy_decls;
	std::deque<std::unique_ptr<Formula>> dummy_formulas;

	const Type dummy_data_type = Type("post#dummy-data-type", Sort::DATA);
	const VariableDeclaration dummy_search_key = VariableDeclaration("post#dummy-search-key", dummy_data_type, false);

	using triple_t = std::tuple<const Type*, const Type*, const Type*>;
	using quintuple_t = std::tuple<const VariableDeclaration*, const VariableDeclaration*, const VariableDeclaration*, const VariableDeclaration*, const VariableDeclaration*>;
	std::map<triple_t, quintuple_t> types2decls;
	std::map<triple_t, const ConjunctionFormula*> types2premise;
	std::map<triple_t, const ImplicationFormula*> types2pure;
	std::map<triple_t, const ImplicationFormula*> types2inserted;
	std::map<triple_t, const ImplicationFormula*> types2deleted;


	static std::unique_ptr<Formula> from_expr(std::unique_ptr<Expression>&& expr) {
		return std::make_unique<ExpressionFormula>(std::move(expr));
	}

	static std::unique_ptr<Expression> from_decl(const VariableDeclaration& decl) {
		return std::make_unique<VariableExpression>(decl);
	}

	static std::unique_ptr<Expression> from_decl(const VariableDeclaration& decl, std::string field) {
		return std::make_unique<Dereference>(from_decl(decl), field);
	}

	static triple_t get_triple(const Type& keyType, const Type& nodeType, std::string fieldname) {
		auto fieldType = nodeType.field(fieldname);
		assert(fieldType);
		return std::make_tuple(&keyType, &nodeType, fieldType);
	}

	static triple_t get_triple(const VariableDeclaration& searchKey, const Dereference& deref) {
		return get_triple(searchKey.type, deref.expr->type(), deref.fieldname);
	}

	static std::unique_ptr<ImplicationFormula> make_implication(std::unique_ptr<Formula> premise, std::unique_ptr<Formula> conclusion) {
		return std::make_unique<ImplicationFormula>(std::move(premise), std::move(conclusion));
	}

	static std::unique_ptr<NegatedFormula> make_negated_implication(std::unique_ptr<Formula> premise, std::unique_ptr<Formula> conclusion) {
		// !(A => B)  |=|  A && !B
		return std::make_unique<NegatedFormula>(make_implication(std::move(premise), std::move(conclusion)));
	}

	static std::unique_ptr<ConjunctionFormula> copy_conjunction(const ConjunctionFormula& formula) {
		auto result = std::make_unique<ConjunctionFormula>();
		for (const auto& conjunct : formula.conjuncts) {
			result->conjuncts.push_back(plankton::copy(*conjunct));
		}
		return result;
	}

	static std::unique_ptr<ImplicationFormula> copy_implication(const ImplicationFormula& formula) {
		return std::make_unique<ImplicationFormula>(plankton::copy(*formula.premise), plankton::copy(*formula.conclusion));
	}

	quintuple_t get_decls(triple_t mapKey) {
		auto find = types2decls.find(mapKey);
		if (find != types2decls.end()) {
			return find->second;
		}

		auto [keyType, nodeType, valueType] = mapKey;
		auto keyNow = std::make_unique<VariableDeclaration>("tmp#searchKey-now", *keyType, false);
		auto keyNext = std::make_unique<VariableDeclaration>("tmp#searchKey-next", *keyType, false);
		auto nodeNow = std::make_unique<VariableDeclaration>("tmp#node-now", *nodeType, false);
		auto nodeNext = std::make_unique<VariableDeclaration>("tmp#node-next", *nodeType, false);
		auto value = std::make_unique<VariableDeclaration>("tmp#value", *valueType, false);

		auto mapValue = std::make_tuple(keyNow.get(), keyNext.get(), nodeNow.get(), nodeNext.get(), value.get());
		dummy_decls.push_back(std::move(keyNow));
		dummy_decls.push_back(std::move(keyNext));
		dummy_decls.push_back(std::move(nodeNow));
		dummy_decls.push_back(std::move(nodeNext));
		dummy_decls.push_back(std::move(value));
		types2decls[mapKey] = mapValue;

		return mapValue;
	}

	std::pair<quintuple_t, std::unique_ptr<ConjunctionFormula>> get_dummy_premise_base(triple_t mapKey, std::string field) {
		auto quint = get_decls(mapKey);

		// see if we have something prepared already
		auto find = types2premise.find(mapKey);
		if (find != types2premise.end()) {
			return std::make_pair(quint, copy_conjunction(*find->second));
		}

		// prepare something new
		auto [keyNow, keyNext, nodeNow, nodeNext, value] = quint;
		auto premise = std::make_unique<ConjunctionFormula>();
		premise->conjuncts.push_back(from_expr( // nodeNow = nodeNext
			std::make_unique<BinaryExpression>(BinaryExpression::Operator::EQ, from_decl(*nodeNow), from_decl(*nodeNext))
		));
		premise->conjuncts.push_back(from_expr( // nodeNext->{field} = value
			std::make_unique<BinaryExpression>(BinaryExpression::Operator::EQ, from_decl(*nodeNext, field), from_decl(*value))
		));
		for (auto pair : nodeNow->type.fields) {
			auto name = pair.second.get().name;
			if (name == field) continue;
			premise->conjuncts.push_back(from_expr( // nodeNow->{name} = nodeNext->{name}
				std::make_unique<BinaryExpression>(BinaryExpression::Operator::EQ, from_decl(*nodeNow, name), from_decl(*nodeNext, name))
			));	
		}

		// return
		auto result = std::make_pair(quint, copy_conjunction(*premise));
		types2premise.insert(std::make_pair(mapKey, premise.get()));
		dummy_formulas.push_back(std::move(premise));
		return result;
	}

	std::unique_ptr<Formula> get_key_formula(quintuple_t decls, BinaryExpression::Operator op) {
		auto& keyNow = *std::get<0>(decls);
		auto& keyNext = *std::get<1>(decls);
		return std::make_unique<ExpressionFormula>(std::make_unique<BinaryExpression>(op, from_decl(keyNow), from_decl(keyNext)));
	}

	std::pair<quintuple_t, std::unique_ptr<ConjunctionFormula>> get_dummy_premise(triple_t mapKey, std::string fieldname, BinaryExpression::Operator op=BinaryExpression::Operator::EQ) {
		auto result = get_dummy_premise_base(mapKey, fieldname);
		result.second->conjuncts.push_back(get_key_formula(result.first, op));
		return result;
	}

	std::unique_ptr<Formula> make_contains_predicate(const VariableDeclaration& /*node*/, const VariableDeclaration& /*key*/) {
		// return contains(node, key)
		throw std::logic_error("not yet implemented: make_contains_next");
	}

	std::unique_ptr<Formula> make_pure_conclusion(quintuple_t decls) {
		auto& keyNext = *std::get<1>(decls);
		auto& nodeNow = *std::get<2>(decls);
		auto& nodeNext = *std::get<3>(decls);

		auto make_equality = [](auto formula, auto other) {
			auto result = std::make_unique<ConjunctionFormula>();
			result->conjuncts.push_back(make_implication(plankton::copy(*formula), plankton::copy(*other)));
			result->conjuncts.push_back(make_implication(std::move(other), std::move(formula)));
			return result;
		};

		return make_equality(make_contains_predicate(nodeNow, keyNext), make_contains_predicate(nodeNext, keyNext));
	}

	std::unique_ptr<ConjunctionFormula> make_satisfaction_conclusion(quintuple_t decls, bool deletion) {
		auto& keyNow = *std::get<0>(decls);
		auto& nodeNow = *std::get<2>(decls);
		auto& nodeNext = *std::get<3>(decls);

		auto result = std::make_unique<ConjunctionFormula>();
		result->conjuncts.push_back(make_pure_conclusion(decls));

		auto impLhs = make_contains_predicate(nodeNow, keyNow);
		auto impRhs = make_contains_predicate(nodeNext, keyNow);
		if (deletion) {
			impLhs.swap(impRhs);
		}
		result->conjuncts.push_back(make_negated_implication(std::move(impLhs), std::move(impRhs)));

		return result;
	}

	template<typename M, typename F>
	std::unique_ptr<ImplicationFormula> get_or_create_dummy(M& map, triple_t mapKey, F makeNew) {
		auto find = map.find(mapKey);
		if (find != map.end()) {
			return copy_implication(*find->second);

		} else {
			auto newElem = makeNew();
			auto result = copy_implication(*newElem);
			map.insert(std::make_pair(mapKey, newElem.get()));
			dummy_formulas.push_back(std::move(newElem));
			return result;
		}
	}

	std::unique_ptr<ImplicationFormula> get_dummy_pure(triple_t mapKey, std::string fieldname) {
		return get_or_create_dummy(types2pure, mapKey, [&,this](){
			auto [decls, premise] = get_dummy_premise_base(mapKey, fieldname); // do not restrict keyNext
			auto conclusion = make_pure_conclusion(decls);
			return make_implication(std::move(premise), std::move(conclusion));
		});
	}

	std::unique_ptr<ImplicationFormula> get_dummy_satisfies_insert(triple_t mapKey, std::string fieldname) {
		return get_or_create_dummy(types2inserted, mapKey, [&,this](){
			auto [decls, premise] = get_dummy_premise(mapKey, fieldname);
			auto conclusion = make_satisfaction_conclusion(decls, false);
			return make_implication(std::move(premise), std::move(conclusion));
		});
	}

	std::unique_ptr<ImplicationFormula> get_dummy_satisfies_delete(triple_t mapKey, std::string fieldname) {
		return get_or_create_dummy(types2deleted, mapKey, [&,this](){
			auto [decls, premise] = get_dummy_premise(mapKey, fieldname);
			auto conclusion = make_satisfaction_conclusion(decls, true);
			return make_implication(std::move(premise), std::move(conclusion));
		});
	}


	std::unique_ptr<ImplicationFormula> extend_dummy_with_state(std::unique_ptr<ImplicationFormula> dummy, const ConjunctionFormula& state, triple_t mapKey, const VariableDeclaration& searchKey, const VariableDeclaration& node, const Expression& value) {
		auto premise = copy_conjunction(state); // TODO: one could probably go without copy the state formula here
		premise->conjuncts.push_back(std::move(dummy->premise));
		auto [dummyKeyNow, dummyKeyNext, dummyNodeNow, dummyNodeNext, dummyValue] = get_decls(mapKey);
		premise->conjuncts.push_back(from_expr(
			std::make_unique<BinaryExpression>(BinaryExpression::Operator::EQ, from_decl(*dummyNodeNow), from_decl(node))
		));
		premise->conjuncts.push_back(from_expr(
			std::make_unique<BinaryExpression>(BinaryExpression::Operator::EQ, from_decl(*dummyKeyNow), from_decl(searchKey))
		));
		premise->conjuncts.push_back(from_expr(
			std::make_unique<BinaryExpression>(BinaryExpression::Operator::EQ, from_decl(*dummyValue), cola::copy(value))
		));
		dummy->premise = std::move(premise);
		return dummy;
	}

	std::unique_ptr<ImplicationFormula> get_pure(const ConjunctionFormula& state, const Dereference& deref, const VariableDeclaration& derefNode, const Expression& value) {
		auto mapKey = get_triple(dummy_search_key, deref);
		auto implication = get_dummy_pure(mapKey, deref.fieldname);
		return extend_dummy_with_state(std::move(implication), state, mapKey, dummy_search_key, derefNode, value);
	}

	std::unique_ptr<ImplicationFormula> get_satisfies_insert(const ConjunctionFormula& state, const VariableDeclaration& searchKey, const Dereference& deref, const VariableDeclaration& derefNode, const Expression& value) {
		auto mapKey = get_triple(searchKey, deref);
		auto implication = get_dummy_satisfies_insert(mapKey, deref.fieldname);
		return extend_dummy_with_state(std::move(implication), state, mapKey, searchKey, derefNode, value);
	}

	std::unique_ptr<ImplicationFormula> get_satisfies_delete(const ConjunctionFormula& state, const VariableDeclaration& searchKey, const Dereference& deref, const VariableDeclaration& derefNode, const Expression& value) {
		auto mapKey = get_triple(searchKey, deref);
		auto implication = get_dummy_satisfies_delete(mapKey, deref.fieldname);
		return extend_dummy_with_state(std::move(implication), state, mapKey, searchKey, derefNode, value);
	}


	static bool implication_holds(std::unique_ptr<ImplicationFormula> implication) {
		return plankton::implies(*implication->premise, *implication->conclusion);
	}

	bool is_pure(const ConjunctionFormula& state, const Dereference& deref, const VariableDeclaration& derefNode, const Expression& value) {
		return implication_holds(get_pure(state, deref, derefNode, value));
	}

	bool satisfies_insert(const ConjunctionFormula& state, const VariableDeclaration& searchKey, const Dereference& deref, const VariableDeclaration& derefNode, const Expression& value) {
		return implication_holds(get_satisfies_insert(state, searchKey, deref, derefNode, value));
	}

	bool satisfies_delete(const ConjunctionFormula& state, const VariableDeclaration& searchKey, const Dereference& deref, const VariableDeclaration& derefNode, const Expression& value) {
		return implication_holds(get_satisfies_delete(state, searchKey, deref, derefNode, value));
	}

} thePurityChecker;


bool is_owned(const Formula& now, const VariableExpression& var) {
	OwnershipFormula ownership(std::make_unique<VariableExpression>(var.decl));
	return plankton::implies(now, ownership);
}

bool has_no_flow(const Formula& /*now*/, const VariableExpression& /*var*/) {
	// always returning false would not harm soundness
	throw std::logic_error("not yet implemented: has_no_flow");
}

bool has_enabled_future_predicate(const Formula& now, const Annotation& time, const Assignment& cmd) {
	for (const auto& pred : time.time) {
		auto [is_future, future_pred] = plankton::is_future(*pred);
		if (!is_future) continue;
		if (!plankton::syntactically_equal(*future_pred->command->lhs, *cmd.lhs)) continue;
		if (!plankton::syntactically_equal(*future_pred->command->rhs, *cmd.rhs)) continue;
		if (plankton::implies(now, *future_pred->pre)) {
			return true;
		}
	}
	return false;
}

bool keyset_contains(const ConjunctionFormula& /*now*/, const VariableExpression& /*node*/, const VariableDeclaration& /*searchKey*/) {
	throw std::logic_error("not yet implemented: keyset_contains");
}

std::pair<bool, std::unique_ptr<FulfillmentFormula>> try_fulfill_impure_obligation(const ConjunctionFormula& now, const ObligationFormula& obligation, const Dereference& lhs, const VariableExpression& lhsVar, const Expression& rhs) {
	std::pair<bool, std::unique_ptr<FulfillmentFormula>> result;
	if (obligation.spec->kind == Specification::Kind::CONTAINS) {
		return result;
	}

	auto& searchKey = obligation.spec->key->decl;
	if (keyset_contains(now, lhsVar, searchKey)) {
		// 'searchKey' in keyset of 'lhsVar.decl' ==> we are guaranteed that 'lhsVar' is the correct node to modify
		result.first = (obligation.spec->kind == Specification::Kind::INSERT && thePurityChecker.satisfies_insert(now, searchKey, lhs, lhsVar.decl, rhs))
		            || (obligation.spec->kind == Specification::Kind::DELETE && thePurityChecker.satisfies_delete(now, searchKey, lhs, lhsVar.decl, rhs));
	}

	if (result.first) {
		result.second = std::make_unique<FulfillmentFormula>(obligation.spec->copy(), true);
	}

	return result;
}

std::pair<bool, std::unique_ptr<ConjunctionFormula>> check_impure_specification(std::unique_ptr<ConjunctionFormula> now, const Dereference& lhs, const VariableExpression& lhsVar, const Expression& rhs) {
	for (auto& conjunct : now->conjuncts) {
		auto [is_obligation, obligation] = plankton::is_obligation(*conjunct);
		if (is_obligation) {
			auto [success, fulfillment] = try_fulfill_impure_obligation(*now, *obligation, lhs, lhsVar, rhs);
			if (success) {
				conjunct = std::move(fulfillment);
				return std::make_pair(true, std::move(now));
			}
		}
	}
	return std::make_pair(false, std::move(now));
}

std::unique_ptr<Annotation> handle_purity_ptr(std::unique_ptr<Annotation> /*pre*/, const Assignment& /*cmd*/, const Dereference& /*lhs*/, const VariableExpression& /*lhsVar*/, const Expression& /*rhs*/) {
	// assignment may be impure -> obligation/fulfillment transformation requrired
	throw std::logic_error("not yet implemented: handle_purity_ptr");
}

std::unique_ptr<Annotation> handle_purity_data(std::unique_ptr<Annotation> pre, const Assignment& cmd, const Dereference& lhs, const VariableExpression& lhsVar, const Expression& rhs) {
	// assignment may be impure ==> obligation/fulfillment transformation requrired
	// ASSUMPTION: assignments to data members do not affect the flow
	//             ==> only the logical content of 'lhsVar' may change

	auto now = plankton::flatten(std::move(pre->now));
	bool success = false;

	// the content of 'lhs.expr' is intersected with the empty flow ==> changes are irrelevant
	success = is_owned(*now, lhsVar) || has_no_flow(*now, lhsVar);

	// pure assignment ==> do nothing
	success = success || thePurityChecker.is_pure(*now, lhs, lhsVar.decl, rhs);

	// check spec if necessary
	if (!success) {
		auto result = check_impure_specification(std::move(now), lhs, lhsVar, rhs);
		success |= result.first;
		now = std::move(result.second);
	}

	// the existence of a future predicate guarantees purity
	success = success || has_enabled_future_predicate(*now, *pre, cmd);

	if (!success) {
		throw VerificationError("Impure assignment violates specification or proof obligation.");
	}

	pre->now = std::move(now);
	return pre;
}

std::unique_ptr<Annotation> post_full_assign_derefptr_varimmi(std::unique_ptr<Annotation> pre, const Assignment& cmd, const Dereference& lhs, const VariableExpression& lhsVar, const Expression& rhs) {
	auto post = handle_purity_ptr(std::move(pre), cmd, lhs, lhsVar, rhs);
	post = search_and_destroy_and_inline_deref(std::move(pre), lhs, rhs);
	return post;
}

std::unique_ptr<Annotation> post_full_assign_derefdata_varimmi(std::unique_ptr<Annotation> pre, const Assignment& cmd, const Dereference& lhs, const VariableExpression& lhsVar, const Expression& rhs) {
	auto post = handle_purity_data(std::move(pre), cmd, lhs, lhsVar, rhs);
	post = search_and_destroy_and_inline_deref(std::move(pre), lhs, rhs);
	return post;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

struct FormChecker : public LogicVisitor {
	static void check(const Annotation& formula) {
		FormChecker visitor;
		formula.accept(visitor);
	}
	void visit(const FlowFormula& /*formula*/) override {
		throw std::logic_error("Malformed annotation: 'FlowFormula' not expected here."); // TODO: we have to allow those, but prune them if not guaranteed to survive command
	}
	void visit(const LogicallyContainedFormula& /*formula*/) override {
		throw std::logic_error("Malformed annotation: 'LogicallyContainedFormula' not expected here."); // TODO: we have to allow those, but prune them if not guaranteed to survive command
	}

	void visit(const ConjunctionFormula& formula) override {
		for (const auto& conjunct : formula.conjuncts) {
			conjunct->accept(*this);
		}
	}
	void visit(const ImplicationFormula& formula) override { formula.premise->accept(*this); formula.conclusion->accept(*this); }
	void visit(const Annotation& annotation) override { annotation.now->accept(*this); }
	void visit(const NegatedFormula& formula) override { formula.formula->accept(*this); }
	void visit(const ExpressionFormula& /*formula*/) override { /* do nothing */ }
	void visit(const OwnershipFormula& /*formula*/) override { /* do nothing */ }
	void visit(const ObligationFormula& /*formula*/) override { /* do nothing */ }
	void visit(const FulfillmentFormula& /*formula*/) override { /* do nothing */ }
	void visit(const PastPredicate& /*formula*/) override { /* do nothing */ }
	void visit(const FuturePredicate& /*formula*/) override { /* do nothing */ }
};

struct AssignmentExpressionAnalyser : public BaseVisitor {
	const VariableExpression* decl = nullptr;
	const Dereference* deref = nullptr;
	bool derefNested = false;

	void reset() {
		decl = nullptr;
		deref = nullptr;
		derefNested = false;
	}

	void visit(const VariableExpression& node) override {
		decl = &node;
	}

	void visit(const Dereference& node) override {
		if (!deref) {
			deref = &node;
			node.expr->accept(*this);
		} else {
			derefNested = true;
		}
	}

	void visit(const NullValue& /*node*/) override { if (deref) derefNested = true; }
	void visit(const BooleanValue& /*node*/) override {if (deref) derefNested = true; }
	void visit(const EmptyValue& /*node*/) override { if (deref) derefNested = true; }
	void visit(const MaxValue& /*node*/) override { if (deref) derefNested = true; }
	void visit(const MinValue& /*node*/) override { if (deref) derefNested = true; }
	void visit(const NDetValue& /*node*/) override { if (deref) derefNested = true; }
	void visit(const NegatedExpression& /*node*/) override { if (deref) derefNested = true; }
	void visit(const BinaryExpression& /*node*/) override { if (deref) derefNested = true; }

};

inline void throw_unsupported(std::string where, const Expression& expr) {
	std::stringstream msg;
	msg << "compound expression '";
	cola::print(expr, msg);
	msg << "' as " << where << "-hand side of assignment not supported.";
	throw UnsupportedConstructError(msg.str());
}

std::unique_ptr<Annotation> plankton::post_full(std::unique_ptr<Annotation> pre, const Assignment& cmd) {
	std::cout << "Post for assignment:  ";
	cola::print(cmd, std::cout);
	std::cout << "under:" << std::endl;
	plankton::print(*pre, std::cout);
	std::cout << std::endl << std::endl;
	FormChecker::check(*pre);
	// TODO: make the following 🤮 nicer

	AssignmentExpressionAnalyser analyser;
	cmd.lhs->accept(analyser);
	const VariableExpression* lhsVar = analyser.decl;
	const Dereference* lhsDeref = analyser.deref;
	bool lhsDerefNested = analyser.derefNested;
	analyser.reset();
	cmd.rhs->accept(analyser);
	const VariableExpression* rhsVar = analyser.decl;
	const Dereference* rhsDeref = analyser.deref;
	bool rhsDerefNested = analyser.derefNested;

	if (lhsDerefNested || (lhsDeref == nullptr && lhsVar == nullptr)) {
		// lhs is a compound expression
		std::stringstream msg;
		msg << "compound expression '";
		cola::print(*cmd.lhs, msg);
		msg << "' as left-hand side of assignment not supported.";
		throw UnsupportedConstructError(msg.str());

	} else if (lhsDeref) {
		// lhs is a dereference
		assert(lhsVar);
		if (!rhsDeref) {
			if (lhsDeref->sort() == Sort::PTR) {
				return post_full_assign_derefptr_varimmi(std::move(pre), cmd, *lhsDeref, *lhsVar, *cmd.rhs);
			} else {
				return post_full_assign_derefdata_varimmi(std::move(pre), cmd, *lhsDeref, *lhsVar, *cmd.rhs);
			}
		} else {
			// deref on both sides not supported
			std::stringstream msg;
			msg << "right-hand side '";
			cola::print(*cmd.rhs, msg);
			msg << "' of assignment not supported with '";
			cola::print(*cmd.lhs, msg);
			msg << "' as left-hand side.";
			throw UnsupportedConstructError(msg.str());
		}

	} else if (lhsVar) {
		// lhs is a variable or an immidiate value
		if (rhsDeref) {
			if (rhsDerefNested) {
				// lhs is a compound expression
				std::stringstream msg;
				msg << "compound expression '";
				cola::print(*cmd.rhs, msg);
				msg << "' as right-hand side of assignment not supported.";
				throw UnsupportedConstructError(msg.str());

			} else if (rhsDeref->sort() == Sort::PTR) {
				return post_full_assign_var_derefptr(std::move(pre), cmd, *lhsVar, *rhsDeref, *rhsVar);
			} else {
				return post_full_assign_var_derefdata(std::move(pre), cmd, *lhsVar, *rhsDeref, *rhsVar);
			}
		} else {
			return post_full_assign_var_expr(std::move(pre), cmd, *lhsVar, *cmd.rhs);
		}
	}

	throw std::logic_error("Conditional was expected to be complete.");
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

bool plankton::post_maintains_formula(const Formula& /*pre*/, const Formula& /*maintained*/, const cola::Assignment& /*cmd*/) {
	throw std::logic_error("not yet implemented: plankton::post_maintains_formula(const Formula&, const Formula&, const cola::Assignment&)");
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

bool plankton::post_maintains_invariant(const Annotation& /*pre*/, const Formula& /*invariant*/, const cola::Assignment& /*cmd*/) {
	throw std::logic_error("not yet implemented: plankton::post_maintains_invariant(const Annotation&, const Formula&, const cola::Assignment&)");
}
