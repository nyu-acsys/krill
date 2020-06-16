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
		if (plankton::contains_expression(*elem, search)) {
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
	void visit(const ObligationFormula& /*formula*/) override { throw std::logic_error("not yet implemented: DereferenceExtractor::visit(const ObligationFormula&)"); }
	void visit(const FulfillmentFormula& /*formula*/) override { throw std::logic_error("not yet implemented: DereferenceExtractor::visit(const FulfillmentFormula&)"); }
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

bool is_owned(const Annotation& /*annotation*/, const VariableExpression& /*var*/) {
	throw std::logic_error("not yet implemented: is_owned");
}

bool has_no_flow(const Annotation& /*annotation*/, const VariableExpression& /*var*/) {
	throw std::logic_error("not yet implemented: has_no_flow");
}

struct PurityCheckFormulaFactory {
	std::deque<unique_ptr<VariableDeclaration>> dummy_decls;
	std::deque<unique_ptr<Formula>> dummy_formulas;

	using triple_t = std::tuple<const Type*, const Type*, const Type*>;
	using quintuple_t = std::tuple<const VariableDeclaration*, const VariableDeclaration*, const VariableDeclaration*, const VariableDeclaration*, const VariableDeclaration*>;
	std::map<triple_t, quintuple_t> types2decls;
	std::map<triple_t, const ConjunctionFormula*> types2premise;
	std::map<triple_t, const ImplicationFormula*> types2others;
	std::map<triple_t, const ImplicationFormula*> types2unchanged;
	std::map<triple_t, const ImplicationFormula*> types2inserted;
	std::map<triple_t, const ImplicationFormula*> types2deleted;


	static std::unique_ptr<Formula> from_expr(std::unique_ptr<Expression>&& expr) {
		return std::make_unique<ExpressionFormula>(std::move(expr));
	}

	static std::unique_ptr<Expression> from_decl(const VariableDeclaration& decl) {
		return std::unique_ptr<VariableExpression>(decl);
	}

	static std::unique_ptr<Expression> from_decl(const VariableDeclaration& decl, std::string field) {
		return std::unique_ptr<Dereference>(from_decl(decl), field);
	}

	static triple_t get_triple(const Type& keyType, const Type& nodeType, std::string fieldname) {
		const Type& fieldType = nodeType.fields.at(fieldname).get();
		return std::make_tuple(&keyType, &nodeType, &fieldType);
	}

	static std::unique_ptr<Formula> make_implication(std::unique_ptr<Formula> premise, std::unique_ptr<Formula> conclusion, bool negated=false) {
		// !(A => B)  |=|  A && !B
		auto result = std::make_unique<ImplicationFormula>(std::move(premise), std::move(conclusion));
		if (negated) {
			result = std::make_unique<NegatedFormula>(std::move(result));
		}
		return result;
	}

	static std::unique_ptr<Formula> make_equality(std::unique_ptr<Formula> formula, std::unique_ptr<Formula> other) {
		auto result = std::make_unique<ConjunctionFormula>();
		result->conjuncts.push_back(make_implication(plankton::copy(*formula), plankton::copy(*other)));
		result->conjuncts.push_back(make_implication(std::move(other), std::move(formula)));
	}

	static std::unique_ptr<ConjunctionFormula> copy_conjunction(const ConjunctionFormula& formula) {
		auto result = std::make_unique<ConjunctionFormula>();
		for (const auto& conjunct : formula.conjuncts) {
			result->conjuncts.push_back(plankton::copy(*conjunct));
		}
		return result;
	}

	static std::unique_ptr<ImplicationFormula> copy_implication(const ImplicationFormula& formula) {
		return std::make_unique<ImplicationFormula>(plankton::copy(formula.premise), plankton::copy(formula.conclusion))
	}

	quintuple_t get_decls(triple_t mapKey) {
		auto find = types2decls.find(mapKey);
		if (find != types2decls.end()) {
			return find->second;
		}

		auto keyNow = std::make_unique<VariableDeclaration>("tmp#searchKey-now", keyType, false);
		auto keyNext = std::make_unique<VariableDeclaration>("tmp#searchKey-next", keyType, false);
		auto nodeNow = std::make_unique<VariableDeclaration>("tmp#node-now", nodeType, false);
		auto nodeNext = std::make_unique<VariableDeclaration>("tmp#node-next", nodeType, false);
		auto value = std::make_unique<VariableDeclaration>("tmp#value", fieldType, false);

		auto mapValue = std:;make_tuple(keyNow.get(), keyNext.get(), nodeNow.get(), nodeNext.get(), value.get());
		dummy_decls.push_back(std::move(keyNow));
		dummy_decls.push_back(std::move(keyNext));
		dummy_decls.push_back(std::move(nodeNow));
		dummy_decls.push_back(std::move(nodeNext));
		dummy_decls.push_back(std::move(value));
		types2decls[mapKey] = mapValue;

		return mapValue;
	}

	std::pair<quintuple_t, std::unique_ptr<ConjunctionFormula>> get_dummy_premise_base(triple_t mapKey) {
		auto quint = get_decls(mapKey);

		// see if we have something prepared already
		auto find = types2premise.find(mapKey);
		if (find != types2premise.end()) {
			return std::make_pair(quint, copy_conjunction(*find->second));
		}

		// prepare something new
		auto [keyNow, keyNext, nodeNow, nodeNext, value] = quint;
		auto premise = std::make_unique<ConjunctionFormula>();
		premise.push_back(from_expr( // nodeNow = nodeNext
			std::make_unique<BinaryExpression>(OP_EQ, from_decl(*nodeNow), from_decl(*nodeNext));
		));
		premise.push_back(from_expr( // nodeNext->{field} = value
			std::make_unique<BinaryExpression>(OP_EQ, from_decl(*nodeNext, field), from_decl(*value));
		));
		for (auto name : type.fields) {
			if (name == field) continue;
			premise.push_back(from_expr( // nodeNow->{name} = nodeNext->{name}
				std::make_unique<BinaryExpression>(OP_EQ, from_decl(*nodeNow, name), from_decl(*nodeNext, name));
			));	
		}

		// return
		auto result = std::make_pair(quint, plankton::copy(premise));
		types2premise.insert(std::make_pair(mapKey, premise.get()));
		dummy_formulas.push_back(std::move(premise))
		return result;
	}

	std::unique_ptr<Formula> get_key_formula(quintuple_t decls, BinaryExpression::Operator op) {
		auto& keyNow = *std::get<0>(decls);
		auto& keyNext = *std::get<1>(decls);
		return std::make_unique<ExpressionFormula>(std::make_unique<BinaryExpression>(op, from_decl(keyNow), from_decl(keyNext)));
	}

	std::unique_ptr<Formula> make_contains_now(quintuple_t decls) {
		auto& keyNext = *std::get<1>(decls);
		auto& nodeNow = *std::get<2>(decls);
		// return contains(nodeNow, keyNext)
		throw std::logic_error("not yet implemented: make_contains_now");
	}

	std::unique_ptr<Formula> make_contains_next(quintuple_t decls) {
		auto& keyNext = *std::get<1>(decls);
		auto& nodeNext = *std::get<3>(decls);
		// return contains(nodeNext, keyNext)
		throw std::logic_error("not yet implemented: make_contains_next");
	}

	std::pair<quintuple_t, std::unique_ptr<ConjunctionFormula>> get_dummy_premise(triple_t mapKey, BinaryExpression::Operator op=BinaryExpression::Operator::EQ) {
		auto result = get_dummy_premise_base(mapKey);
		result.second->conjuncts.push_back(get_key_formula(result.first, op));
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
			map.insert(std::make_pair(mapKey, std::move(newElem)));
			return result;
		}
	}

	std::unique_ptr<ImplicationFormula> get_dummy_other_keys_unchanged(triple_t mapKey) {
		return get_or_create_dummy(types2others, mapKey, [&mapKey](){
			auto [decls, premise] = get_dummy_premise(mapKey, BinaryExpression::Operator::NEQ);
			auto conclusion = make_equality(make_contains_now(decls), make_contains_next(decls));
			return make_implication(std::move(premise), std::move(conclusion));
		});
	}

	std::unique_ptr<ImplicationFormula> get_dummy_search_key_unchanged(triple_t mapKey) {
		return types2unchanged(types2unchanged, mapKey, [&mapKey](){
			auto [decls, premise] = get_dummy_premise(mapKey);
			auto conclusion = make_equality(make_contains_now(decls), make_contains_next(decls));
			return make_implication(std::move(premise), std::move(conclusion));
		});
	}

	std::unique_ptr<ImplicationFormula> get_dummy_search_key_inserted(triple_t mapKey) {
		return types2inserted(types2inserted, mapKey, [&mapKey](){
			auto [decls, premise] = get_dummy_premise(mapKey);
			auto conclusion = make_implication(make_contains_now(decls), make_contains_next(decls), true);
			return make_implication(std::move(premise), std::move(conclusion));
		});
	}

	std::unique_ptr<ImplicationFormula> get_dummy_search_key_deleted(triple_t mapKey) {
		return types2deleted(types2deleted, mapKey, [&mapKey](){
			auto [decls, premise] = get_dummy_premise(mapKey);
			auto conclusion = make_equality(make_contains_next(decls), make_contains_now(decls), true);
			return make_implication(std::move(premise), std::move(conclusion));
		});
	}


	std::unique_ptr<ImplicationFormula> extend_dummy_with_state(std::unique_ptr<ImplicationFormula> dummy, const ConjunctionFormula& state, triple_t mapKey, const VariableDeclaration& searchKey, const VariableDeclaration& node, const Expression& value) {
		auto premise = copy_conjunction(state);
		premise->conjuncts.push_back(std::move(dummy->premise));
		auto [dummyKeyNow, dummyKeyNext, dummyNodeNow, dummyNodeNext, dummyValue] = get_decls(mapKey);
		premise->conjuncts.push_back(from_expr(
			std::make_unique<BinaryExpression>(BinaryExpression::Operator::EQ, from_decl(dummyNodeNow), from_decl(node));
		));
		premise->conjuncts.push_back(from_expr(
			std::make_unique<BinaryExpression>(BinaryExpression::Operator::EQ, from_decl(dummyKeyNow), from_decl(searchKey));
		));
		premise->conjuncts.push_back(from_expr(
			std::make_unique<BinaryExpression>(BinaryExpression::Operator::EQ, from_decl(dummyValue), cola::copy(value));
		));
		dummy->premise = std::move(premise);
		return dummy;
	}

	static triple_t get_triple(const VariableDeclaration& searchKey, const Dereference& deref) {
		return get_triple(searchKey.type, deref.expr->type(), deref.type());
	}

	std::unique_ptr<ImplicationFormula> get_other_keys_unchanged(const ConjunctionFormula& state, const VariableDeclaration& searchKey, const Dereference& deref, const VariableDeclaration& derefNode, const Expression& value) {
		auto mapKey = get_triple(searchKey, deref);
		auto implication = get_dummy_other_keys_unchanged(mapKey);
		return extend_dummy_with_state(std::move(implication), state, mapKey, searchKey, derefNode, value);
	}

	std::unique_ptr<ImplicationFormula> get_search_key_unchanged(const ConjunctionFormula& state, const VariableDeclaration& searchKey, const Dereference& deref, const VariableDeclaration& derefNode, const Expression& value) {
		auto mapKey = get_triple(searchKey, deref);
		auto implication = get_dummy_search_key_unchanged(mapKey);
		return extend_dummy_with_state(std::move(implication), state, mapKey, searchKey, derefNode, value);
	}

	std::unique_ptr<ImplicationFormula> get_search_key_inserted(const ConjunctionFormula& state, const VariableDeclaration& searchKey, const Dereference& deref, const VariableDeclaration& derefNode, const Expression& value) {
		auto mapKey = get_triple(searchKey, deref);
		auto implication = get_dummy_search_key_inserted(mapKey);
		return extend_dummy_with_state(std::move(implication), state, mapKey, searchKey, derefNode, value);
	}

	std::unique_ptr<ImplicationFormula> get_search_key_deleted(const ConjunctionFormula& state, const VariableDeclaration& searchKey, const Dereference& deref, const VariableDeclaration& derefNode, const Expression& value) {
		auto mapKey = get_triple(searchKey, deref);
		auto implication = get_dummy_search_key_deleted(mapKey);
		return extend_dummy_with_state(std::move(implication), state, mapKey, searchKey, derefNode, value);
	}
};



const VariableDeclaration& get_search_key() {
	throw std::logic_error("not yet implemented: get_search_key");
}

std::unique_ptr<Annotation> handle_purity_ptr(std::unique_ptr<Annotation> /*pre*/, const Dereference& /*lhs*/, const VariableExpression& /*lhsVar*/, const Expression& /*rhs*/) {
	// assignment may be impure -> obligation/fulfillment transformation requrired
	throw std::logic_error("not yet implemented: handle_purity_ptr");
}

std::unique_ptr<Annotation> handle_purity_data(std::unique_ptr<Annotation> pre, const Dereference& lhs, const VariableExpression& lhsVar, const Expression& rhs) {
	// assignment may be impure ==> obligation/fulfillment transformation requrired
	// ASSUMPTION: assignments to data members do not affect the flow
	//             ==> only the logical content of 'lhsVar' may change

	if (is_owned(*pre, lhsVar) || has_no_flow(*pre, lhsVar)) {
		// the content of 'lhs.expr' is intersected with the empty flow ==> changes are irrelevant
		return pre;
	}

	// check purity
	static PurityCheckFormulaFactory factory;
	auto& searchKey = get_search_key();
	auto otherKeysUnchanged = factory.get_other_keys_unchanged(*pre->now, searchKey, lhs, lhsVar, rhs);
	auto searchKeyUnchanged = factory.get_search_key_unchanged(*pre->now, searchKey, lhs, lhsVar, rhs);
	auto searchKeyInserted = factory.get_search_key_inserted(*pre->now, searchKey, lhs, lhsVar, rhs);
	auto searchKeyDeleted = factory.get_search_key_deleted(*pre->now, searchKey, lhs, lhsVar, rhs);

	auto impHolds = [](auto imp){
		return plankton::implies(*imp->premise, *imp->conclusion);
	}

	if (!impHolds(otherKeysUnchanged)) {
		throw VerificationError("Impure action does not satisfy specification");

	} else if (impHolds(searchKeyUnchanged)) {
		// pure
		throw std::logic_error("not yet implemented: handle_purity_data");

	} else if (impHolds(searchKeyInserted)) {
		// inserted
		throw std::logic_error("not yet implemented: handle_purity_data");
		
	} else if (impHolds(searchKeyDeleted)) {
		// deleted
		throw std::logic_error("not yet implemented: handle_purity_data");
		
	} else {
		throw VerificationError("Could not show action pure nor prove it satisfies its specification.");

	}
}

std::unique_ptr<Annotation> post_full_assign_derefptr_varimmi(std::unique_ptr<Annotation> pre, const Assignment& /*cmd*/, const Dereference& lhs, const VariableExpression& lhsVar, const Expression& rhs) {
	auto post = handle_purity_ptr(std::move(pre), lhs, lhsVar, rhs);
	post = search_and_destroy_and_inline_deref(std::move(pre), lhs, rhs);
	return post;
}

std::unique_ptr<Annotation> post_full_assign_derefdata_varimmi(std::unique_ptr<Annotation> pre, const Assignment& /*cmd*/, const Dereference& lhs, const VariableExpression& lhsVar, const Expression& rhs) {
	auto post = handle_purity_ptr(std::move(pre), lhs, lhsVar, rhs);
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
		throw std::logic_error("Malformed annotation: 'FlowFormula' not expected here.");
	}
	void visit(const LogicallyContainedFormula& /*formula*/) override {
		throw std::logic_error("Malformed annotation: 'LogicallyContainedFormula' not expected here.");
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
