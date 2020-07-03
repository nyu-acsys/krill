#include "plankton/post.hpp"

#include <iostream> // TODO: remove
#include <sstream>
#include <type_traits>
#include "cola/util.hpp"
#include "plankton/util.hpp"
#include "plankton/error.hpp"
#include "plankton/config.hpp"
#include "plankton/properties.hpp"

using namespace cola;
using namespace plankton;


struct POST_PTR_T {};
struct POST_DATA_T {};

template<typename T>
std::string to_string(const T& obj) {
	// make string
	std::stringstream stream;
	cola::print(obj, stream);
	std::string result = stream.str();
	// trim // TODO: properly trim (does not delete new lines currently)
	std::stringstream trimmer;
	trimmer << result;
	result.clear();
	trimmer >> result;
	// done
	return result;
}

void check_config_unchanged() {
	static const PlanktonConfig* for_config = plankton::config.get();
	if (for_config != plankton::config.get()) {
		throw std::logic_error("Critical error: plankton::config changed unexpectedly.");
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<ConjunctionFormula> remove_copies(std::unique_ptr<ConjunctionFormula> now) {
	auto result = std::make_unique<ConjunctionFormula>();
	for (auto& conjunct : now->conjuncts) {
		if (!plankton::syntactically_contains_conjunct(*result, *conjunct)) {
			result->conjuncts.push_back(std::move(conjunct));
		}
	}
	return result;
}

inline bool is_false(const Axiom& axiom) {
	if (auto expr = dynamic_cast<const ExpressionAxiom*>(&axiom)) {
		if (auto binary = dynamic_cast<const BinaryExpression*>(expr->expr.get())) {
			if (binary->op == BinaryExpression::Operator::NEQ && syntactically_equal(*binary->lhs, *binary->rhs)) {
				return true;
			}
		}
	}
	return false;
}

inline bool is_useless(const SimpleFormula& formula) {
	if (auto imp = dynamic_cast<const ImplicationFormula*>(&formula)) {
		for (const auto& premise : imp->premise->conjuncts) {
			if (is_false(*premise)) {
				return true;
			}
		}
	}
	return false;
}

inline std::unique_ptr<ConjunctionFormula> simply(std::unique_ptr<ConjunctionFormula> now) {
	if (!plankton::config->post_simplify) {
		return now;
	}

	auto result = std::make_unique<ConjunctionFormula>();
	for (auto& conjunct : now->conjuncts) {
		if (!is_useless(*conjunct)) {
			result->conjuncts.push_back(std::move(conjunct));
		}
	}

	return remove_copies(std::move(result));
}

struct DataNormalizer : public BaseVisitor, public BaseLogicVisitor {
	using BaseVisitor::visit;
	using BaseLogicVisitor::visit;

	std::unique_ptr<ConjunctionFormula> normalized = std::make_unique<ConjunctionFormula>();

	void add_with_changed_op(const BinaryExpression& expr, BinaryExpression::Operator new_op) {
		normalized->conjuncts.push_back(std::make_unique<ExpressionAxiom>(
			std::make_unique<BinaryExpression>(new_op, cola::copy(*expr.lhs), cola::copy(*expr.rhs))
		));
	}
	void add_with_changed_op(const BinaryExpression& expr, BinaryExpression::Operator new_op, BinaryExpression::Operator more_new_op) {
		add_with_changed_op(expr, new_op);
		add_with_changed_op(expr, more_new_op);
	}
	bool normalize_expr(const BinaryExpression& expr) {
		if (expr.op == BinaryExpression::Operator::AND) {
			auto lhs = std::make_unique<ExpressionAxiom>(cola::copy(*expr.lhs));
			auto rhs = std::make_unique<ExpressionAxiom>(cola::copy(*expr.rhs));
			normalized->conjuncts.push_back(std::move(lhs));
			normalized->conjuncts.push_back(std::move(rhs));
			return true;
		
		}

		if (expr.lhs->sort() == Sort::DATA && expr.rhs->sort() == Sort::DATA) {
			switch (expr.op) {
				case BinaryExpression::Operator::EQ: add_with_changed_op(expr, BinaryExpression::Operator::LEQ, BinaryExpression::Operator::GEQ); return false;
				case BinaryExpression::Operator::LT: add_with_changed_op(expr, BinaryExpression::Operator::LEQ); return false;
				case BinaryExpression::Operator::GT: add_with_changed_op(expr, BinaryExpression::Operator::GEQ); return false;
				default: return false;
			}
		}

		return false;
	}

	void visit(const BooleanValue& /*node*/) override { /* do nothing */ }
	void visit(const NullValue& /*node*/) override { /* do nothing */ }
	void visit(const EmptyValue& /*node*/) override { /* do nothing */ }
	void visit(const MaxValue& /*node*/) override { /* do nothing */ }
	void visit(const MinValue& /*node*/) override { /* do nothing */ }
	void visit(const NDetValue& /*node*/) override { /* do nothing */ }
	void visit(const VariableExpression& /*node*/) override { /* do nothing */ }
	void visit(const Dereference& /*node*/) override { /* do nothing */ }
	void visit(const NegatedExpression& /*node*/) override { /* do nothing */ }
	void visit(const BinaryExpression& node) override {
		auto descende = normalize_expr(node);
		if (descende) {
			node.lhs->accept(*this);
			node.rhs->accept(*this);
		}
	}
	
	void visit(const ImplicationFormula& /*formula*/) override { /* do nothing */ }
	void visit(const NegatedAxiom& /*formula*/) override { /* do nothing */ }
	void visit(const OwnershipAxiom& /*formula*/) override { /* do nothing */ }
	void visit(const LogicallyContainedAxiom& /*formula*/) override { /* do nothing */ }
	void visit(const KeysetContainsAxiom& /*formula*/) override { /* do nothing */ }
	void visit(const HasFlowAxiom& /*formula*/) override { /* do nothing */ }
	void visit(const FlowContainsAxiom& /*formula*/) override { /* do nothing */ }
	void visit(const ObligationAxiom& /*formula*/) override { /* do nothing */ }
	void visit(const FulfillmentAxiom& /*formula*/) override { /* do nothing */ }
	void visit(const Annotation& formula) override { formula.now->accept(*this); }
	void visit(const ConjunctionFormula& formula) override {
		for (const auto& conjunct : formula.conjuncts) {
			conjunct->accept(*this);
		}
	}
	void visit(const ExpressionAxiom& formula) override {
		formula.expr->accept(*this);
	}
};

struct ExpressionNormalizer : public BaseNonConstVisitor, public DefaultNonConstListener {
	using BaseNonConstVisitor::visit;
	using DefaultNonConstListener::visit;
	using DefaultNonConstListener::enter;

	void enter(ExpressionAxiom& formula) override {
		formula.expr->accept(*this);
	}
	
	void visit(BooleanValue& /*node*/) override { /* do nothing */ }
	void visit(NullValue& /*node*/) override { /* do nothing */ }
	void visit(EmptyValue& /*node*/) override { /* do nothing */ }
	void visit(MaxValue& /*node*/) override { /* do nothing */ }
	void visit(MinValue& /*node*/) override { /* do nothing */ }
	void visit(NDetValue& /*node*/) override { /* do nothing */ }
	void visit(VariableExpression& /*node*/) override { /* do nothing */ }
	void visit(NegatedExpression& node) override { node.expr->accept(*this); }
	void visit(Dereference& node) override { node.expr->accept(*this); }
	void visit(BinaryExpression& node) override {
		node.lhs->accept(*this);
		node.rhs->accept(*this);

		switch (node.op) {
			case BinaryExpression::Operator::GT:
				node.lhs.swap(node.rhs);
				node.op = BinaryExpression::Operator::LT;
				break;

			case BinaryExpression::Operator::GEQ:
				node.lhs.swap(node.rhs);
				node.op = BinaryExpression::Operator::LEQ;
				break;

			default: break;;
		}
	}
};

std::unique_ptr<Annotation> post_process(std::unique_ptr<Annotation> annotation) {
	DataNormalizer normalizer;
	annotation->accept(normalizer);
	annotation->now = plankton::conjoin(std::move(annotation->now), std::move(normalizer.normalized));

	ExpressionNormalizer visitor;
	annotation->accept(visitor);

	annotation->now = simply(std::move(annotation->now));	
	return annotation;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

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

template<typename R>
struct AssignmentComputer {
	virtual ~AssignmentComputer() = default;
	virtual R var_expr(const Assignment& cmd, const VariableExpression& lhs, const Expression& rhs) = 0;
	virtual R var_derefptr(const Assignment& cmd, const VariableExpression& lhs, const Dereference& rhs, const VariableExpression& rhsVar) = 0;
	virtual R var_derefdata(const Assignment& cmd, const VariableExpression& lhs, const Dereference& rhs, const VariableExpression& rhsVar) = 0;
	virtual R derefptr_varimmi(const Assignment& cmd, const Dereference& lhs, const VariableExpression& lhsVar, const Expression& rhs) = 0;
	virtual R derefdata_varimmi(const Assignment& cmd, const Dereference& lhs, const VariableExpression& lhsVar, const Expression& rhs) = 0;
};

template<typename R>
R compute_assignment_switch(AssignmentComputer<R>& computer, const Assignment& cmd) {
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
				return computer.derefptr_varimmi(cmd, *lhsDeref, *lhsVar, *cmd.rhs);
			} else {
				return computer.derefdata_varimmi(cmd, *lhsDeref, *lhsVar, *cmd.rhs);
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
				return computer.var_derefptr(cmd, *lhsVar, *rhsDeref, *rhsVar);
			} else {
				return computer.var_derefdata(cmd, *lhsVar, *rhsDeref, *rhsVar);
			}
		} else {
			return computer.var_expr(cmd, *lhsVar, *cmd.rhs);
		}
	}

	throw std::logic_error("Conditional was expected to be complete.");
}

std::unique_ptr<Annotation> combine_to_annotation(std::unique_ptr<ConjunctionFormula> formula, std::unique_ptr<ConjunctionFormula> other) {
	return std::make_unique<Annotation>(plankton::conjoin(std::move(formula), std::move(other)));
}

std::unique_ptr<Annotation> combine_to_annotation(const ConjunctionFormula& formula, std::unique_ptr<ConjunctionFormula> other) {
	return combine_to_annotation(plankton::copy(formula), std::move(other));
}

std::unique_ptr<Annotation> combine_to_annotation(const ConjunctionFormula& formula, const ConjunctionFormula& other) {
	return combine_to_annotation(formula, plankton::copy(other));
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

struct PurityChecker {
	std::deque<std::unique_ptr<VariableDeclaration>> dummy_decls;
	std::deque<std::unique_ptr<Formula>> dummy_formulas;

	const Type dummy_data_type = Type("post#dummy-data-type", Sort::DATA);
	const VariableDeclaration dummy_search_key = VariableDeclaration("post#dummy-search-key", dummy_data_type, false); // TODO: just use Type::data_type()?

	using tuple_t = std::pair<std::unique_ptr<ConjunctionFormula>, std::unique_ptr<ConjunctionFormula>>;
	using triple_t = std::tuple<const Type*, const Type*, const Type*>;
	using quintuple_t = std::tuple<const VariableDeclaration*, const VariableDeclaration*, const VariableDeclaration*, const VariableDeclaration*, const VariableDeclaration*>;
	std::map<triple_t, quintuple_t> types2decls;
	std::map<triple_t, const ConjunctionFormula*> types2premise;
	std::map<triple_t, std::pair<const ConjunctionFormula*, const ConjunctionFormula*>> types2pure;
	std::map<triple_t, std::pair<const ConjunctionFormula*, const ConjunctionFormula*>> types2inserted;
	std::map<triple_t, std::pair<const ConjunctionFormula*, const ConjunctionFormula*>> types2deleted;
	std::map<std::pair<const VariableDeclaration*, const VariableDeclaration*>, const Axiom*> vars2contains;


	static std::unique_ptr<Axiom> from_expr(std::unique_ptr<Expression>&& expr) {
		return std::make_unique<ExpressionAxiom>(std::move(expr));
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

	static std::unique_ptr<ImplicationFormula> make_implication(std::unique_ptr<Axiom> premise, std::unique_ptr<Axiom> conclusion) {
		auto result = std::make_unique<ImplicationFormula>();
		result->premise->conjuncts.push_back(std::move(premise));
		result->conclusion->conjuncts.push_back(std::move(conclusion));
		return result;
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
			return std::make_pair(quint, plankton::copy(*find->second));
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
		auto result = std::make_pair(quint, plankton::copy(*premise));
		types2premise.insert(std::make_pair(mapKey, premise.get()));
		dummy_formulas.push_back(std::move(premise));
		return result;
	}

	std::unique_ptr<Axiom> get_key_formula(quintuple_t decls) {
		auto& keyNow = *std::get<0>(decls);
		auto& keyNext = *std::get<1>(decls);
		return from_expr(std::make_unique<BinaryExpression>(BinaryExpression::Operator::EQ, from_decl(keyNow), from_decl(keyNext)));
	}

	std::pair<quintuple_t, std::unique_ptr<ConjunctionFormula>> get_dummy_premise(triple_t mapKey, std::string fieldname) {
		auto result = get_dummy_premise_base(mapKey, fieldname);
		result.second->conjuncts.push_back(get_key_formula(result.first));
		return result;
	}

	std::unique_ptr<Axiom> make_contains_predicate(const VariableDeclaration& node, const VariableDeclaration& key) {
		// return contains(node, key)

		// see if we already created it
		auto mapKey = std::make_pair(&node, &key);
		auto find = vars2contains.find(mapKey);
		if (find != vars2contains.end()) {
			return plankton::copy(*find->second);
		}

		// instatiate predicate
		auto axiom = plankton::config->get_logically_contains_key().instantiate(node, key);
		// auto axiom = plankton::instantiate_property(logically_contains_predicate, { node, key });
		auto result = plankton::copy(*axiom);
		vars2contains.insert(std::make_pair(mapKey, axiom.get()));
		dummy_formulas.push_back(std::move(axiom));
		return result;
	}

	std::unique_ptr<ConjunctionFormula> make_pure_conclusion(quintuple_t decls) {
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

		auto result = make_pure_conclusion(decls);

		auto lhs = make_contains_predicate(nodeNow, keyNow);
		auto rhs = make_contains_predicate(nodeNext, keyNow);
		if (!deletion) {
			lhs.swap(rhs);
		}
		result->conjuncts.push_back(std::move(lhs));
		result->conjuncts.push_back(std::make_unique<NegatedAxiom>(std::move(rhs)));

		return result;
	}

	template<typename M, typename F>
	tuple_t get_or_create_dummy(M& map, triple_t mapKey, F makeNew) {
		auto find = map.find(mapKey);
		if (find != map.end()) {
			auto [premise, conclusion] = find->second;
			return std::make_pair(plankton::copy(*premise), plankton::copy(*conclusion));

		} else {
			auto [premise, conclusion] = makeNew();
			auto result = std::make_pair(plankton::copy(*premise), plankton::copy(*conclusion));
			map[mapKey] = std::make_pair(premise.get(), conclusion.get());
			dummy_formulas.push_back(std::move(premise));
			dummy_formulas.push_back(std::move(conclusion));
			return result;
		}
	}

	tuple_t get_dummy_pure(triple_t mapKey, std::string fieldname) {
		return get_or_create_dummy(types2pure, mapKey, [&,this](){
			auto [decls, premise] = get_dummy_premise_base(mapKey, fieldname); // do not restrict keyNext
			auto conclusion = make_pure_conclusion(decls);
			return std::make_pair(std::move(premise), std::move(conclusion));
		});
	}

	tuple_t get_dummy_satisfies_insert(triple_t mapKey, std::string fieldname) {
		return get_or_create_dummy(types2inserted, mapKey, [&,this](){
			auto [decls, premise] = get_dummy_premise(mapKey, fieldname);
			auto conclusion = make_satisfaction_conclusion(decls, false);
			return std::make_pair(std::move(premise), std::move(conclusion));
		});
	}

	tuple_t get_dummy_satisfies_delete(triple_t mapKey, std::string fieldname) {
		return get_or_create_dummy(types2deleted, mapKey, [&,this](){
			auto [decls, premise] = get_dummy_premise(mapKey, fieldname);
			auto conclusion = make_satisfaction_conclusion(decls, true);
			return std::make_pair(std::move(premise), std::move(conclusion));
		});
	}


	std::unique_ptr<ConjunctionFormula> extend_dummy_with_state(std::unique_ptr<ConjunctionFormula> dummy, const ConjunctionFormula& state, triple_t mapKey, const VariableDeclaration& searchKey, const VariableDeclaration& node, const Expression& value) {
		auto premise = plankton::copy(state); // TODO: one could probably avoid this copy somehow
		for (auto& conjunct : dummy->conjuncts) {
			premise->conjuncts.push_back(std::move(conjunct));
		}

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

		return premise;
	}

	tuple_t get_pure(const ConjunctionFormula& state, const Dereference& deref, const VariableDeclaration& derefNode, const Expression& value) {
		auto mapKey = get_triple(dummy_search_key, deref);
		auto [premise, conclusion] = get_dummy_pure(mapKey, deref.fieldname);
		premise = extend_dummy_with_state(std::move(premise), state, mapKey, dummy_search_key, derefNode, value);
		return std::make_pair(std::move(premise), std::move(conclusion));
	}

	tuple_t get_satisfies_insert(const ConjunctionFormula& state, const VariableDeclaration& searchKey, const Dereference& deref, const VariableDeclaration& derefNode, const Expression& value) {
		auto mapKey = get_triple(searchKey, deref);
		auto [premise, conclusion] = get_dummy_satisfies_insert(mapKey, deref.fieldname);
		premise = extend_dummy_with_state(std::move(premise), state, mapKey, searchKey, derefNode, value);
		return std::make_pair(std::move(premise), std::move(conclusion));
	}

	tuple_t get_satisfies_delete(const ConjunctionFormula& state, const VariableDeclaration& searchKey, const Dereference& deref, const VariableDeclaration& derefNode, const Expression& value) {
		auto mapKey = get_triple(searchKey, deref);
		auto [premise, conclusion] = get_dummy_satisfies_delete(mapKey, deref.fieldname);
		premise = extend_dummy_with_state(std::move(premise), state, mapKey, searchKey, derefNode, value);
		return std::make_pair(std::move(premise), std::move(conclusion));
	}


	static bool implication_holds(tuple_t&& implication) {
		return plankton::implies(*implication.first, *implication.second);
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

};

PurityChecker& get_purity_checker() {
	check_config_unchanged();

	static PurityChecker checker;
	return checker;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

template<bool prune_local>
struct Pruner : public BaseLogicNonConstVisitor { // TODO: is this thingy correct?
	const Expression& prune_expr;
	bool prune_child = false;
	bool prune_value = true;
	Pruner(const Expression& expr) : prune_expr(expr) {}

	template<typename T> std::unique_ptr<T> make_pruned() {
		return std::make_unique<ExpressionAxiom>(std::make_unique<BooleanValue>(prune_value));
	}
	
	template<> std::unique_ptr<AxiomConjunctionFormula> make_pruned<AxiomConjunctionFormula>() {
		auto result = std::make_unique<AxiomConjunctionFormula>();
		result->conjuncts.push_back(std::make_unique<ExpressionAxiom>(std::make_unique<BooleanValue>(prune_value)));
		return result;
	}

	template<typename T>
	void handle_formula(std::unique_ptr<T>& formula) {
		formula->accept(*this);
		if (prune_child) {
			formula = make_pruned<T>();
			prune_child = false;
		}
	}

	void visit(ConjunctionFormula& formula) override {
		for (auto& conjunct : formula.conjuncts) {
			handle_formula(conjunct);
		}
	}

	void visit(AxiomConjunctionFormula& formula) override {
		for (auto& conjunct : formula.conjuncts) {
			handle_formula(conjunct);
		}
	}

	void visit(NegatedAxiom& formula) override {
		bool old_value = prune_value;
		prune_value = !prune_value;
		handle_formula(formula.axiom);
		prune_value = old_value;
	}

	void visit(ImplicationFormula& formula) override {
		assert(prune_value);
		handle_formula(formula.conclusion);
	}

	void visit(OwnershipAxiom& formula) override {
		if (!prune_value) return;
		prune_child = plankton::syntactically_equal(*formula.expr, prune_expr);
	}

	void visit(LogicallyContainedAxiom& /*formula*/) override { prune_child = prune_local; }
	void visit(KeysetContainsAxiom& /*formula*/) override { prune_child = prune_local; }
	void visit(HasFlowAxiom& /*formula*/) override { prune_child = prune_local; }
	void visit(FlowContainsAxiom& /*formula*/) override { prune_child = prune_local; }

	void visit(ExpressionAxiom& /*formula*/) override { prune_child = false; }
	void visit(ObligationAxiom& /*formula*/) override { prune_child = false; }
	void visit(FulfillmentAxiom& /*formula*/) override { prune_child = false; }
};

template<bool prune_local>
inline std::unique_ptr<ConjunctionFormula> apply_pruner(std::unique_ptr<ConjunctionFormula> formula, const Expression& expr) {
	Pruner<prune_local> visitor(expr);
	formula->accept(visitor);
	return formula;
}

std::unique_ptr<ConjunctionFormula> destroy_ownership(std::unique_ptr<ConjunctionFormula> formula, const Expression& expr) {
	// remove ownership of 'expr' in 'formula'
	return apply_pruner<false>(std::move(formula), expr);
}

std::unique_ptr<ConjunctionFormula> destroy_ownership_and_non_local_knowledge(std::unique_ptr<ConjunctionFormula> formula, const Expression& expr) {
	// remove ownership of 'expr' in 'formula' as well as all non-local knowledge (like flow, keysets, node contents)
	return apply_pruner<true>(std::move(formula), expr);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

template<typename T>
T container_search_and_destroy(T&& uptrcontainer, const Expression& destroy) {
	// TODO important: are we are deleting to much -> should we avoid deleting things in the premise of implications?

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
	// TODO important: must we avoid inlining in the premise of implications?

	// copy knowledge about 'search' in 'uptrcontainer' over to 'replacement'
	T new_elements;
	for (auto& elem : uptrcontainer) {
		auto [contained, contained_within_obligation] = plankton::contains_expression_obligation(*elem, search);
		if (contained && !contained_within_obligation) {
			new_elements.push_back(replace_expression(plankton::copy(*elem), search, replacement));
		}
	}
	uptrcontainer.insert(uptrcontainer.end(), std::make_move_iterator(new_elements.begin()), std::make_move_iterator(new_elements.end()));
	return std::move(uptrcontainer);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

std::deque<std::unique_ptr<TimePredicate>> container_search_and_destroy_time(std::deque<std::unique_ptr<TimePredicate>>&& time, const VariableExpression& destroy) {
	// split
	std::deque<std::unique_ptr<TimePredicate>> result;
	std::deque<std::unique_ptr<PastPredicate>> pasts;
	for (auto& pred : time) {
		if (!is_of_type<PastPredicate>(*pred).first) {
			result.push_back(std::move(pred));
		} else {
			auto raw = static_cast<PastPredicate*>(pred.release());
			// std::unique_ptr<PastPredicate> casted(check.second);
			// pasts.emplace_back(std::move(casted));
			pasts.emplace_back(raw);
		}
	}

	// apply search and destroy
	result = container_search_and_destroy(std::move(result), destroy);
	for (auto& past : pasts) {
		past->formula->conjuncts = container_search_and_destroy(std::move(past->formula->conjuncts), destroy);
	}

	// combine
	result.insert(result.end(), std::make_move_iterator(pasts.begin()), std::make_move_iterator(pasts.end()));
	return result;
}

std::unique_ptr<Annotation> search_and_destroy_and_inline_var(std::unique_ptr<Annotation> pre, const VariableExpression& lhs, const Expression& rhs) {
	// destroy knowledge about lhs
	assert(pre);
	auto now = destroy_ownership_and_non_local_knowledge(std::move(pre->now), rhs);
	now->conjuncts = container_search_and_destroy(std::move(now->conjuncts), lhs);
	pre->time = container_search_and_destroy_time(std::move(pre->time), lhs);

	// copy knowledge about rhs over to lhs
	now->conjuncts = container_search_and_inline(std::move(now->conjuncts), rhs, lhs);
	pre->time = container_search_and_inline(std::move(pre->time), rhs, lhs); // TODO important: are those really freely duplicable?

	// add new knowledge
	auto equality = std::make_unique<ExpressionAxiom>(std::make_unique<BinaryExpression>(BinaryExpression::Operator::EQ, cola::copy(lhs), cola::copy(rhs)));
	now->conjuncts.push_back(std::move(equality));
	pre->now = std::move(now);

	// done
	return pre;
}

inline void check_purity_var(const VariableExpression& lhs, bool check_purity=true) {
	if (!check_purity) {
		return;
	}

	if (lhs.decl.is_shared) {
		// assignment to 'lhs' potentially impure (may require obligation/fulfillment transformation)
		// TODO: implement
		throw UnsupportedConstructError("assignment to shared variable '" + lhs.decl.name + "'; assignment potentially impure");
	}
}

std::unique_ptr<Annotation> post_full_assign_var_expr(std::unique_ptr<Annotation> pre, const Assignment& /*cmd*/, const VariableExpression& lhs, const Expression& rhs, bool check_purity=true) {
	check_purity_var(lhs, check_purity);
	return search_and_destroy_and_inline_var(std::move(pre), lhs, rhs);
}

std::unique_ptr<Annotation> post_full_assign_var_derefptr(std::unique_ptr<Annotation> pre, const Assignment& /*cmd*/, const VariableExpression& lhs, const Dereference& rhs, const VariableExpression& /*rhsVar*/, bool check_purity=true) {
	check_purity_var(lhs, check_purity);
	// TODO: no self loops? Add lhs != rhs?
	// TODO important: infer flow/contains and put it into a past predicate
	return search_and_destroy_and_inline_var(std::move(pre), lhs, rhs);
}

std::unique_ptr<Annotation> post_full_assign_var_derefdata(std::unique_ptr<Annotation> pre, const Assignment& /*cmd*/, const VariableExpression& lhs, const Dereference& rhs, const VariableExpression& /*rhsVar*/, bool check_purity=true) {
	check_purity_var(lhs, check_purity);
	return search_and_destroy_and_inline_var(std::move(pre), lhs, rhs);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void store_deref(std::deque<std::unique_ptr<Dereference>>& container, const Dereference& deref) {
	container.push_back(std::make_unique<Dereference>(cola::copy(*deref.expr), deref.fieldname));
}

void store_deref(std::deque<const Dereference*>& container, const Dereference& deref) {
	container.push_back(&deref);
}

template<typename T>
struct DereferenceExtractor : public BaseVisitor, public DefaultListener {
	using BaseVisitor::visit;
	using DefaultListener::visit;
	using DefaultListener::enter;

	std::string search_field;
	std::deque<T> result;
	DereferenceExtractor(std::string search) : search_field(search) {}

	void visit(const Dereference& node) override {
		node.expr->accept(*this);
		if (node.fieldname == search_field) {
			store_deref(result, node);
		}
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
	
	void enter(const ExpressionAxiom& formula) override { formula.expr->accept(*this); }
	void enter(const OwnershipAxiom& formula) override { formula.expr->accept(*this); }
	void enter(const LogicallyContainedAxiom& formula) override { formula.expr->accept(*this); }
	void enter(const KeysetContainsAxiom& formula) override { formula.node->accept(*this); formula.value->accept(*this); }
	void enter(const HasFlowAxiom& formula) override { formula.expr->accept(*this); }
	void enter(const FlowContainsAxiom& formula) override { formula.expr->accept(*this); formula.low_value->accept(*this); formula.high_value->accept(*this); }
	void enter(const ObligationAxiom& formula) override { formula.key->accept(*this); }
	void enter(const FulfillmentAxiom& formula) override { formula.key->accept(*this); }
};

template<typename T = std::unique_ptr<Dereference>>
static std::deque<T> extract_dereferences(const Formula& formula, std::string fieldname) {
	DereferenceExtractor<T> visitor(fieldname);
	formula.accept(visitor);
	return std::move(visitor.result);
}

std::unique_ptr<ConjunctionFormula> search_and_destroy_derefs(std::unique_ptr<ConjunctionFormula> now, const Dereference& lhs) {
	assert(now);

	// check for whether or not a dereference is affected by an assignment to 'lhs'
	auto solver = plankton::make_solver_from_premise(*now);
	auto changed_expr = cola::copy(*lhs.expr);
	auto is_affected = [&solver,&changed_expr](Dereference& deref){
		// this lambda leaves 'deref' unchanged, however, temporarily steals its 'expr' member field for efficiency reasons (to avoid copying it)
		// this lambda leaves 'changed_expr' unchanged, however, temporarily steals it for efficiency reasons (to avoid copying it)
		assert(changed_expr);
		assert(deref.expr);

		BinaryExpression derefed_neq_lhs( // deref.expr != lhs
			BinaryExpression::Operator::NEQ, std::move(deref.expr), std::move(changed_expr)
		);
		auto result = !solver->implies(derefed_neq_lhs);
		deref.expr = std::move(derefed_neq_lhs.lhs);
		changed_expr = std::move(derefed_neq_lhs.rhs);
		return result;
	};

	// go through all dereferences in 'now' and find those that are affected
	auto dereferences = extract_dereferences(*now, lhs.fieldname);
	for (auto& deref : dereferences) {
		if (!is_affected(*deref)) {
			deref.reset(nullptr);
		}
	}

	// remove affected dereferences
	// NOTE: this must be done in a separate loop, because the removal might invalidate our 'solver'
	for (const auto& deref : dereferences) {
		if (deref) {
			now->conjuncts = container_search_and_destroy(std::move(now->conjuncts), *deref);
		}
	}

	return now;
}

std::unique_ptr<Annotation> search_and_destroy_and_inline_deref(std::unique_ptr<Annotation> pre, const Dereference& lhs, const Expression& rhs) {
	// destroy knowledge about lhs (do not modify TimePredicates)
	assert(pre);
	assert(pre->now);

	auto now = destroy_ownership_and_non_local_knowledge(std::move(pre->now), rhs);
	now = search_and_destroy_derefs(std::move(now), lhs);
	// do not modify 

	// copy knowledge about rhs over to lhs (do not modify TimePredicates)
	now->conjuncts = container_search_and_inline(std::move(now->conjuncts), rhs, lhs);

	// add new knowledge;
	now->conjuncts.push_back(std::make_unique<ExpressionAxiom>(
		std::make_unique<BinaryExpression>(BinaryExpression::Operator::EQ, cola::copy(lhs), cola::copy(rhs))
	));

	// done
	pre->now = std::move(now);
	return pre;
}

bool is_heap_homogeneous(const Dereference& deref) {
	return deref.type().field(deref.fieldname) != &deref.type();
}

inline bool check_ownership(const ConjunctionFormula& now, const VariableExpression& var, bool quick) {
	if (var.decl.is_shared) return false;
	OwnershipAxiom ownership(std::make_unique<VariableExpression>(var.decl));
	if (plankton::syntactically_contains_conjunct(now, ownership)) return true;
	if (quick) return false;
	return plankton::implies(now, ownership);
}

bool is_owned_quick(const ConjunctionFormula& now, const VariableExpression& var) {
	return check_ownership(now, var, true);
}

bool is_owned(const ConjunctionFormula& now, const VariableExpression& var) {
	return check_ownership(now, var, false);
}

bool has_no_flow(const ConjunctionFormula& now, const VariableExpression& var) {
	NegatedAxiom flow(std::make_unique<HasFlowAxiom>(cola::copy(var)));
	return plankton::implies(now, flow);
}

bool has_enabled_future_predicate(const ConjunctionFormula& now, const Annotation& time, const Assignment& cmd) {
	auto solver = plankton::make_solver_from_premise(now);
	for (const auto& pred : time.time) {
		auto [is_future, future_pred] = plankton::is_of_type<FuturePredicate>(*pred);
		if (!is_future) continue;
		if (!plankton::syntactically_equal(*future_pred->command->lhs, *cmd.lhs)) continue;
		if (!plankton::syntactically_equal(*future_pred->command->rhs, *cmd.rhs)) continue;
		if (solver->implies(*future_pred->pre)) {
			return true;
		}
	}
	return false;
}

bool keyset_contains(const ConjunctionFormula& now, const VariableExpression& node, const VariableDeclaration& searchKey) {
	KeysetContainsAxiom keyset(std::make_unique<VariableExpression>(node.decl), std::make_unique<VariableExpression>(searchKey));
	return plankton::implies(now, keyset);
}

template<typename T, typename R>
std::pair<bool, std::unique_ptr<ConjunctionFormula>> try_fulfill_obligation_mapper(std::unique_ptr<ConjunctionFormula> now, T mapper, const Dereference& lhs, const VariableExpression& lhsVar, const R& rhs) {
	for (auto& conjunct : now->conjuncts) {
		auto [is_obligation, obligation] = plankton::is_of_type<ObligationAxiom>(*conjunct);
		if (is_obligation) {
			auto [success, fulfillment] = mapper(*now, *obligation, lhs, lhsVar, rhs);
			if (success) {
				conjunct = std::move(fulfillment);
				return std::make_pair(true, std::move(now));
			}
		}
	}
	return std::make_pair(false, std::move(now));
}

std::pair<bool, std::unique_ptr<FulfillmentAxiom>> try_fulfill_impure_obligation_data(const ConjunctionFormula& now, const ObligationAxiom& obligation, const Dereference& lhs, const VariableExpression& lhsVar, const Expression& rhs) {
	std::pair<bool, std::unique_ptr<FulfillmentAxiom>> result;
	if (obligation.kind == ObligationAxiom::Kind::CONTAINS) {
		return result;
	}

	auto& searchKey = obligation.key->decl;
	if (keyset_contains(now, lhsVar, searchKey)) {
		// 'searchKey' in keyset of 'lhsVar.decl' ==> we are guaranteed that 'lhsVar' is the correct node to modify
		result.first = (obligation.kind == ObligationAxiom::Kind::INSERT && get_purity_checker().satisfies_insert(now, searchKey, lhs, lhsVar.decl, rhs))
		            || (obligation.kind == ObligationAxiom::Kind::DELETE && get_purity_checker().satisfies_delete(now, searchKey, lhs, lhsVar.decl, rhs));
	}

	if (result.first) {
		result.second = std::make_unique<FulfillmentAxiom>(obligation.kind, std::make_unique<VariableExpression>(obligation.key->decl), true);
	}

	return result;
}

std::pair<bool, std::unique_ptr<ConjunctionFormula>> ensure_pure_or_spec_data(std::unique_ptr<ConjunctionFormula> now, const Dereference& lhs, const VariableExpression& lhsVar, const Expression& rhs) {
	// we assume that the update is local to the node referenced by 'lhsVar'
	if (!plankton::config->is_flow_field_local(lhsVar.type(), lhs.fieldname)) {
		return std::make_pair(false, std::move(now));
	}

	// pure assignment ==> do nothing
	if (get_purity_checker().is_pure(*now, lhs, lhsVar.decl, rhs)) {
		return std::make_pair(true, std::move(now));
	}

	// check impure specification
	return try_fulfill_obligation_mapper(std::move(now), try_fulfill_impure_obligation_data, lhs, lhsVar, rhs);
}

std::pair<std::unique_ptr<ImplicationFormula>, std::unique_ptr<ImplicationFormula>> make_key_inbetween_formulas(const VariableDeclaration& left, const VariableDeclaration& middle, const VariableDeclaration& right, const VariableDeclaration& key, const VariableDeclaration& otherKey) {
	// content(left) < content(middle)
	auto implication = std::make_unique<ImplicationFormula>();
	implication->premise->conjuncts.push_back(
		get_purity_checker().make_contains_predicate(left, key)
	);
	implication->premise->conjuncts.push_back(
		get_purity_checker().make_contains_predicate(middle, otherKey)
	);
	implication->conclusion->conjuncts.push_back(std::make_unique<ExpressionAxiom>(std::make_unique<BinaryExpression>(
		BinaryExpression::Operator::LT,
		std::make_unique<VariableExpression>(key),
		std::make_unique<VariableExpression>(otherKey)
	)));

	// max-content(rhs) < min-content(successor)
	auto other = std::make_unique<ImplicationFormula>();
	other->premise->conjuncts.push_back(
		get_purity_checker().make_contains_predicate(middle, key)
	);
	other->premise->conjuncts.push_back(
		get_purity_checker().make_contains_predicate(right, otherKey)
	);
	other->conclusion->conjuncts.push_back(std::make_unique<ExpressionAxiom>(std::make_unique<BinaryExpression>(
		BinaryExpression::Operator::LT,
		std::make_unique<VariableExpression>(key),
		std::make_unique<VariableExpression>(otherKey)
	)));
	
	// done
	return std::make_pair(std::move(implication), std::move(other));
}

template<typename T> 
bool is_node_insertion_with_additional_checks(const ConjunctionFormula& now, const Dereference& lhs, const VariableExpression& lhsVar, const VariableExpression& rhs, T make_additional_checks) {
	if (rhs.type().field(lhs.fieldname) != &lhs.type()) {
		return false;
	}

	// Assume that the keys removed from the succesors keyset are not contained in the successor:
	//         max-content(lhs) < min-content(rhs) /\ max-content(rhs) < min-content(successor)
	//     ==> insertion of content(rhs) /\ rest remains unchanged
	if (!plankton::config->is_flow_insertion_inbetween_local(lhsVar.type(), lhs.fieldname)) {
		return false;
	}

	// create dummy variables for solving
	VariableDeclaration successor("post#dummy-ptr-insert", lhs.type(), false);
	VariableDeclaration key("post#dummy-key-insert", Type::data_type(), false); // TODO: is the type correct? or create dummy data type?
	VariableDeclaration otherKey("post#dummy-otherKey-insert", Type::data_type(), false);

	// solving premise: now /\ lhs = successor
	auto premise = plankton::copy(now);
	premise->conjuncts.push_back(std::make_unique<ExpressionAxiom>(std::make_unique<BinaryExpression>(
		BinaryExpression::Operator::EQ,
		cola::copy(lhs),
		std::make_unique<VariableExpression>(successor)
	)));

	// solving conclusion: additional_checks /\ rhs->{lhs.fieldname} = successor /\ "flow assumption premise"
	// key is a fresh variable ==> it is implicityly universally quantified
	auto conclusion = make_additional_checks(successor, key);
	premise->conjuncts.push_back(std::make_unique<ExpressionAxiom>(std::make_unique<BinaryExpression>(
		BinaryExpression::Operator::EQ,
		cola::copy(rhs),
		std::make_unique<Dereference>(std::make_unique<VariableExpression>(successor), lhs.fieldname)
	)));
	// content(lhs) < content(rhs)< content(successor)
	auto parts = make_key_inbetween_formulas(lhsVar.decl, rhs.decl, successor, key, otherKey);
	conclusion->conjuncts.push_back(std::move(parts.first));
	conclusion->conjuncts.push_back(std::move(parts.second));

	// solve
	// std::cout << std::endl << "SOLVE for is_node_insertion_with_additional_checks" << std::endl;
	// plankton::print(*premise, std::cout);
	// std::cout << std::endl << " => " << std::endl;
	// plankton::print(*conclusion, std::cout);
	// std::cout << std::endl;

	auto result = plankton::implies(*premise, *conclusion);
	// std::cout << " ~~ " << result << std::endl;
	return result;
}

bool is_pure_node_insertion(const ConjunctionFormula& now, const Dereference& lhs, const VariableExpression& lhsVar, const VariableExpression& rhs) {
	return is_node_insertion_with_additional_checks(now, lhs, lhsVar, rhs, [&rhs](const auto& /*successor*/, const auto& dummyKey){
		auto result = std::make_unique<ConjunctionFormula>();
		// additional check: content(rhs) subseteq keyset(rhs)
		result->conjuncts.push_back(std::make_unique<ImplicationFormula>(
			get_purity_checker().make_contains_predicate(rhs.decl, dummyKey),
			std::make_unique<KeysetContainsAxiom>(std::make_unique<VariableExpression>(rhs.decl), std::make_unique<VariableExpression>(dummyKey))
		));
		return result;
	});
}

bool is_impure_node_insertion(const ConjunctionFormula& now, const ObligationAxiom& obligation, const Dereference& lhs, const VariableExpression& lhsVar, const VariableExpression& rhs) {
	if (obligation.kind != ObligationAxiom::Kind::INSERT) {
		return false;
	}
	auto& searchKey = obligation.key->decl;
	return is_node_insertion_with_additional_checks(now, lhs, lhsVar, rhs, [&searchKey,&rhs](const auto& /*successor*/, const auto& dummyKey){
		auto result = std::make_unique<ConjunctionFormula>();
		// TODO: is this correct?
		// additional checks: [keyset(rhs)=empty \/ flow(rhs)=empty \/ owned(rhs)] /\ [forall k!=searchKey. !contains(rhs, k)]
		// note: a \/ b \/ c |=| (!a /\ !b) => c
		auto implication = std::make_unique<ImplicationFormula>();
		implication->premise->conjuncts.push_back(std::make_unique<KeysetContainsAxiom>( // ![forall k. k notin keyset(rhs)]
			std::make_unique<VariableExpression>(rhs.decl),
			std::make_unique<VariableExpression>(dummyKey)
		));
		implication->premise->conjuncts.push_back(
			std::make_unique<HasFlowAxiom>(std::make_unique<VariableExpression>(rhs.decl))
		);
		implication->conclusion->conjuncts.push_back(
			std::make_unique<OwnershipAxiom>(std::make_unique<VariableExpression>(rhs.decl))
		);
		result->conjuncts.push_back(std::move(implication));
		result->conjuncts.push_back(std::make_unique<ImplicationFormula>(
			std::make_unique<ExpressionAxiom>(std::make_unique<BinaryExpression>(
				BinaryExpression::Operator::NEQ,
				std::make_unique<VariableExpression>(dummyKey),
				std::make_unique<VariableExpression>(searchKey)
			)),
			std::make_unique<NegatedAxiom>(get_purity_checker().make_contains_predicate(rhs.decl, dummyKey))
		));
		return result;
	});
}

template<typename T>
bool is_node_unlinking_with_additional_checks(const ConjunctionFormula& now, const Dereference& lhs, const VariableExpression& lhsVar, const VariableExpression& rhs, T make_additional_checks) {
	if (!is_heap_homogeneous(lhs)) {
		return false;
	}

	// Assume that the keys inserted into the rhs keyset do not extend its logical content:
	//         max-content(lhs) < min-content(rhs) /\ max-content(rhs) < min-content(successor)
	//     ==> deletion of content(inbetween) /\ rest remains unchanged
	if (!plankton::config->is_flow_unlinking_local(lhsVar.type(), lhs.fieldname)) {
		return false;
	}

	// create dummy variables for solving
	VariableDeclaration inbetween("post#dummy-ptr-unklink", lhs.type(), false);
	VariableDeclaration key("post#dummy-key-unklink", Type::data_type(), false); // TODO: is the type correct? or create dummy data type?
	VariableDeclaration otherKey("post#dummy-otherKey-unlink", Type::data_type(), false);

	// solving premise: now /\ lhs = inbetween
	auto premise = plankton::copy(now);
	premise->conjuncts.push_back(std::make_unique<ExpressionAxiom>(std::make_unique<BinaryExpression>(
		BinaryExpression::Operator::EQ,
		cola::copy(lhs),
		std::make_unique<VariableExpression>(inbetween)
	)));

	// solving conclusion: additional_checks /\ inbetween->{lhs.fieldname} = rhs /\ "flow assumption premise"
	// key is a fresh variable ==> it is implicityly universally quantified
	auto conclusion = make_additional_checks(inbetween, key);
	conclusion->conjuncts.push_back(std::make_unique<ExpressionAxiom>(std::make_unique<BinaryExpression>(
		BinaryExpression::Operator::EQ,
		std::make_unique<Dereference>(std::make_unique<VariableExpression>(inbetween), lhs.fieldname),
		cola::copy(rhs)
	)));
	// content(lhs) < content(rhs)< content(successor)
	auto parts = make_key_inbetween_formulas(lhsVar.decl, inbetween, rhs.decl, key, otherKey);
	conclusion->conjuncts.push_back(std::move(parts.first));
	conclusion->conjuncts.push_back(std::move(parts.second));

	// solve
	return plankton::implies(*premise, *conclusion);
}

bool is_pure_node_unlinking(const ConjunctionFormula& now, const Dereference& lhs, const VariableExpression& lhsVar, const VariableExpression& rhs) {
	return is_node_unlinking_with_additional_checks(now, lhs, lhsVar, rhs, [](const auto& nodeInbetween, const auto& dummyKey){
		auto result = std::make_unique<ConjunctionFormula>();
		// addional check: forall key. !contains(nodeInbetween, key)
		result->conjuncts.push_back(
			std::make_unique<NegatedAxiom>(get_purity_checker().make_contains_predicate(nodeInbetween, dummyKey))
		);
		return result;
	});
}

bool is_impure_node_unlinking(const ConjunctionFormula& now, const ObligationAxiom& obligation, const Dereference& lhs, const VariableExpression& lhsVar, const VariableExpression& rhs) {
	if (obligation.kind != ObligationAxiom::Kind::DELETE) return false;
	auto& searchKey = obligation.key->decl;
	return is_node_unlinking_with_additional_checks(now, lhs, lhsVar, rhs, [&searchKey](const auto& nodeInbetween, const auto& dummyKey){
		auto result = std::make_unique<ConjunctionFormula>();
		// addional check: contains(nodeInbetween, searchKey) /\ [forall k!=searchKey. !contains(nodeInbetween, k)] /\ "k \in keyset(nodeInbetween)"
		result->conjuncts.push_back(
			get_purity_checker().make_contains_predicate(nodeInbetween, searchKey)
		);
		result->conjuncts.push_back(std::make_unique<ImplicationFormula>(
			std::make_unique<ExpressionAxiom>(std::make_unique<BinaryExpression>(
				BinaryExpression::Operator::NEQ,
				std::make_unique<VariableExpression>(dummyKey),
				std::make_unique<VariableExpression>(searchKey)
			)),
			std::make_unique<NegatedAxiom>(get_purity_checker().make_contains_predicate(nodeInbetween, dummyKey))
		));
		result->conjuncts.push_back(
			std::make_unique<KeysetContainsAxiom>(std::make_unique<VariableExpression>(nodeInbetween), std::make_unique<VariableExpression>(searchKey))
		);
		return result;
	});
}

std::pair<bool, std::unique_ptr<FulfillmentAxiom>> try_fulfill_impure_obligation_ptr(const ConjunctionFormula& now, const ObligationAxiom& obligation, const Dereference& lhs, const VariableExpression& lhsVar, const VariableExpression& rhs) {
	std::pair<bool, std::unique_ptr<FulfillmentAxiom>> result;
	result.first = is_impure_node_insertion(now, obligation, lhs, lhsVar, rhs) || is_impure_node_unlinking(now, obligation, lhs, lhsVar, rhs);
	if (result.first) {
		result.second = std::make_unique<FulfillmentAxiom>(obligation.kind, std::make_unique<VariableExpression>(obligation.key->decl), true);
	}
	return result;
}

const VariableExpression& extract_or_fail(const Expression& expr, const Expression* lhs=nullptr) {
	auto check = plankton::is_of_type<VariableExpression>(expr);
	if (!check.first) {
		auto msg = lhs ? " to '" + to_string(*lhs) + "'" : "";
		throw UnsupportedConstructError("assigning" + msg + " from unsupported expression '" + to_string(expr) + "'; expected a pointer variable.");
	}
	return *check.second;
}

std::pair<bool, std::unique_ptr<ConjunctionFormula>> ensure_pure_or_spec_ptr(std::unique_ptr<ConjunctionFormula> now, const Dereference& lhs, const VariableExpression& lhsVar, const Expression& rhs) {
	// we assume that the update affects the flow only on the part sent out by the modified selector
	if (!plankton::config->is_flow_field_local(lhsVar.type(), lhs.fieldname)) {
		return std::make_pair(false, std::move(now));
	}

	// extract variable from rhs
	const VariableExpression& rhsVar = extract_or_fail(rhs, &lhs);

	// Case 1/2: pure deletion/insertion
	if (is_pure_node_unlinking(*now, lhs, lhsVar, rhsVar) || is_pure_node_insertion(*now, lhs, lhsVar, rhsVar)) {
		return std::make_pair(true, std::move(now));
	}

	// Case 3/4: impure deletion/insertion
	return try_fulfill_obligation_mapper(std::move(now), try_fulfill_impure_obligation_ptr, lhs, lhsVar, rhsVar);
}

template<bool is_ptr>
std::unique_ptr<Annotation> handle_purity(std::unique_ptr<Annotation> pre, const Assignment& cmd, const Dereference& lhs, const VariableExpression& lhsVar, const Expression& rhs, bool check_purity=true) {
	if (!check_purity) {
		return pre;
	}

	auto now = std::move(pre->now);
	bool success = false;

	// the content of 'lhsVar' is intersected with the empty flow ==> changes are irrelevant
	success = is_owned(*now, lhsVar) || has_no_flow(*now, lhsVar);

	if (plankton::is_of_type<NullValue>(rhs).first && !success) {
		throw VerificationError("Could not handle assignment '" + to_string(cmd) + "': '" + to_string(lhs) + "' is neither known to be owned nor known to have no flow.");
	}

	// ensure 'cmd' is pure or satisfies spec
	if (!success) {
		auto result = (is_ptr ? ensure_pure_or_spec_ptr : ensure_pure_or_spec_data)(std::move(now), lhs, lhsVar, rhs);
		success = result.first;
		now = std::move(result.second);
	}

	// the existence of a future predicate guarantees purity
	success = success || has_enabled_future_predicate(*now, *pre, cmd);

	if (!success) {
		throw VerificationError("Could not find proof obligation or establish specification for impure assignment: '" + to_string(cmd) + "'.");
	}

	pre->now = std::move(now);
	return pre;
}

std::unique_ptr<Annotation> post_full_assign_derefptr_varimmi(std::unique_ptr<Annotation> pre, const Assignment& cmd, const Dereference& lhs, const VariableExpression& lhsVar, const Expression& rhs, bool check_purity=true) {
	auto post = handle_purity<true>(std::move(pre), cmd, lhs, lhsVar, rhs, check_purity);
	post = search_and_destroy_and_inline_deref(std::move(post), lhs, rhs);
	return post;
}

std::unique_ptr<Annotation> post_full_assign_derefdata_varimmi(std::unique_ptr<Annotation> pre, const Assignment& cmd, const Dereference& lhs, const VariableExpression& lhsVar, const Expression& rhs, bool check_purity=true) {
	auto post = handle_purity<false>(std::move(pre), cmd, lhs, lhsVar, rhs, check_purity);
	post = search_and_destroy_and_inline_deref(std::move(post), lhs, rhs);
	return post;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

struct FullPostComputer : public AssignmentComputer<std::unique_ptr<Annotation>> {
	std::unique_ptr<Annotation> pre;
	bool check_purity;

	FullPostComputer(std::unique_ptr<Annotation> pre_, bool check_purity_=true) : pre(std::move(pre_)), check_purity(check_purity_) {
		assert(pre);
	}

	std::unique_ptr<Annotation> var_expr(const Assignment& cmd, const VariableExpression& lhs, const Expression& rhs) override {
		return post_full_assign_var_expr(std::move(pre), cmd, lhs, rhs, check_purity);
	}
	std::unique_ptr<Annotation> var_derefptr(const Assignment& cmd, const VariableExpression& lhs, const Dereference& rhs, const VariableExpression& rhsVar) override {
		return post_full_assign_var_derefptr(std::move(pre), cmd, lhs, rhs, rhsVar, check_purity);
	}
	std::unique_ptr<Annotation> var_derefdata(const Assignment& cmd, const VariableExpression& lhs, const Dereference& rhs, const VariableExpression& rhsVar) override {
		return post_full_assign_var_derefdata(std::move(pre), cmd, lhs, rhs, rhsVar, check_purity);
	}
	std::unique_ptr<Annotation> derefptr_varimmi(const Assignment& cmd, const Dereference& lhs, const VariableExpression& lhsVar, const Expression& rhs) override {
		return post_full_assign_derefptr_varimmi(std::move(pre), cmd, lhs, lhsVar, rhs, check_purity);
	}
	std::unique_ptr<Annotation> derefdata_varimmi(const Assignment& cmd, const Dereference& lhs, const VariableExpression& lhsVar, const Expression& rhs) override {
		return post_full_assign_derefdata_varimmi(std::move(pre), cmd, lhs, lhsVar, rhs, check_purity);
	}
};


std::unique_ptr<Annotation> make_post_full(std::unique_ptr<Annotation> pre, const Assignment& cmd, bool check_purity=true) {
	FullPostComputer computer(std::move(pre), check_purity);
	auto post = compute_assignment_switch(computer, cmd);
	post = post_process(std::move(post));
	return post;
}

std::unique_ptr<Annotation> plankton::post_full(std::unique_ptr<Annotation> pre, const Assignment& cmd) {
	std::cout << std::endl << "ΩΩΩ post "; cola::print(cmd, std::cout); //std::cout << "  ";
	// std::cout << "################# POST FOR #################" << std::endl;
	// plankton::print(*pre->now, std::cout); std::cout << std::endl;
	// std::cout << std::endl;
	// cola::print(cmd, std::cout);
	// std::cout << std::endl;

	auto result = make_post_full(std::move(pre), cmd);

	// std::cout << "################# POST RESULT #################" << std::endl;
	// std::cout << "  ~~> " << std::endl << "  ";
	// plankton::print(*result->now, std::cout); std::cout << std::endl;
	// std::cout << std::endl << std::endl;

	return result;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

struct InvariantComputer : public AssignmentComputer<bool> {
	static constexpr auto NO_CHECKS = [](const auto&, const auto&){ return std::make_unique<ConjunctionFormula>(); };

	static std::unique_ptr<ConjunctionFormula> instantiate_invariant(const VariableDeclaration& decl) {
		static std::map<const VariableDeclaration*, std::unique_ptr<ConjunctionFormula>> var2inv;
		check_config_unchanged();

		// try to copy existing instantiation
		auto find = var2inv.find(&decl);
		if (find != var2inv.end()) {
			return plankton::copy(*find->second);
		}

		// create new instantiation
		auto result = plankton::config->get_invariant().instantiate(decl);
		var2inv[&decl] = plankton::copy(*result);
		return result;
	}

	const Annotation& pre;

	InvariantComputer(const Annotation& pre_) : pre(pre_) {}

	bool var_expr(const Assignment& /*cmd*/, const VariableExpression& lhs, const Expression& /*rhs*/) override {
		if (lhs.decl.is_shared) {
			throw UnsupportedConstructError("assignment to shared variable '" + lhs.decl.name + "'; cannot check invariant");
		}
		return true;
	}

	bool var_derefptr(const Assignment& /*cmd*/, const VariableExpression& /*lhs*/, const Dereference& /*rhs*/, const VariableExpression& /*rhsVar*/) override {
		return true;
	}

	bool var_derefdata(const Assignment& /*cmd*/, const VariableExpression& /*lhs*/, const Dereference& /*rhs*/, const VariableExpression& /*rhsVar*/) override {
		return true;
	}

	std::unique_ptr<Annotation> make_partial_post(std::unique_ptr<ConjunctionFormula> now, const Expression& lhs, const Expression& rhs, bool destroy_flow=false) {
		// TODO: use full post image to avoid code duplication?
		if (destroy_flow) {
			now = destroy_ownership_and_non_local_knowledge(std::move(now), rhs);
		} else {
			now = destroy_ownership(std::move(now), rhs);
		}
		now->conjuncts = container_search_and_destroy(std::move(now->conjuncts), lhs);
		now->conjuncts.push_back(std::make_unique<ExpressionAxiom>(std::make_unique<BinaryExpression>(
			BinaryExpression::Operator::EQ,
			cola::copy(lhs),
			cola::copy(rhs)
		)));
		return std::make_unique<Annotation>(std::move(now));
	}

	std::unique_ptr<Annotation> make_partial_post(const Expression& lhs, const Expression& rhs, bool destroy_flow=false) {
		return make_partial_post(plankton::copy(*pre.now), lhs, rhs, destroy_flow);
	}

	bool solve_invariant(std::unique_ptr<Annotation> premise, const VariableDeclaration& left, const VariableDeclaration& right, const VariableDeclaration& other) {
		// extend premise with invariant for other
		auto otherInv = instantiate_invariant(other);
		premise->now = plankton::conjoin(std::move(premise->now), std::move(otherInv));

		// conclusion: invariant for left, right, other
		auto conclusion = std::move(combine_to_annotation(instantiate_invariant(left), instantiate_invariant(right))->now);
		conclusion = std::move(combine_to_annotation(instantiate_invariant(other), std::move(conclusion))->now);

		// solve
		return plankton::implies(*premise->now, *conclusion);
	}

	bool derefptr_varimmi_insert(const Dereference& lhs, const VariableExpression& lhsVar, const VariableExpression& rhs) {
		std::cout << "checking invariant for insertion" << std::endl;
		// plankton::print(*pre.now, std::cout); std::cout << std::endl << std::endl;

		// ensure we are dealing with a one-nonde-insertion
		if (!is_node_insertion_with_additional_checks(*pre.now, lhs, lhsVar, rhs, NO_CHECKS)) {
			// std::cout << "  ==> it not an insertion" << std::endl;
			return false;
		}

		// premise: partial post extend with invariant for successor or rhs
		auto partial_post = make_partial_post(lhs, rhs);
		VariableDeclaration successor("inv#dummy-ptr-insert", lhs.type(), false);
		partial_post->now->conjuncts.push_back(std::make_unique<ExpressionAxiom>(std::make_unique<BinaryExpression>(
			BinaryExpression::Operator::EQ,
			std::make_unique<Dereference>(cola::copy(rhs), lhs.fieldname),
			std::make_unique<VariableExpression>(successor)
		)));

		// solve
		return solve_invariant(std::move(partial_post), lhsVar.decl, rhs.decl, successor);
	}

	bool derefptr_varimmi_unlink(const Dereference& lhs, const VariableExpression& lhsVar, const VariableExpression& rhs) {
		std::cout << "checking invariant for unlinking" << std::endl;
		// ensure we are dealing with a one-nonde-insertion
		if (!is_node_unlinking_with_additional_checks(*pre.now, lhs, lhsVar, rhs, NO_CHECKS)) {
			return false;
		}

		// premise: partial post extended with pointer to deleted node
		VariableDeclaration inbetween("inv#dummy-ptr-unklink", lhs.type(), false);
		auto now = plankton::copy(*pre.now);
		now->conjuncts.push_back(std::make_unique<ExpressionAxiom>(std::make_unique<BinaryExpression>(
			BinaryExpression::Operator::EQ,
			cola::copy(lhs),
			std::make_unique<VariableExpression>(inbetween)
		)));
		// remove flow knowledge for post ==> inbetween should be "marked" in some way such that the invariant becomes easily checkable
		auto partial_post = make_partial_post(std::move(now), lhs, rhs, true);

		// solve
		return solve_invariant(std::move(partial_post), lhsVar.decl, rhs.decl, inbetween);
	}

	bool check_invariant_locally(const Dereference& lhs, const VariableExpression& lhsVar, const Expression& rhs) {
		// ASSUMPTION: the caller ensured that the update is local, i.e., nodes other than 'lhsVar' are not influenced
		
		// check invariant on modified node (lhsVar)
		auto partial_post = make_partial_post(lhs, rhs);
		auto lhsVarInv = instantiate_invariant(lhsVar.decl);
		return plankton::implies(*partial_post->now, *lhsVarInv);
	}

	bool derefptr_varimmi_owned(const Dereference& lhs, const VariableExpression& lhsVar, const VariableExpression& rhs) {
		return (is_owned(*pre.now, lhsVar) || has_no_flow(*pre.now, lhsVar))
		    && check_invariant_locally(lhs, lhsVar, rhs);
	}

	bool derefptr_varimmi(const Assignment& cmd, const Dereference& lhs, const VariableExpression& lhsVar, const Expression& rhs) override {
		// handle NULL assignment
		if (plankton::is_of_type<NullValue>(rhs).first) {
			if (is_owned(*pre.now, lhsVar) || has_no_flow(*pre.now, lhsVar)) {
				// owned(lhsVar) \/ owned(lhsVar) ==> flow does not change
				auto partial_post = make_partial_post(lhs, rhs, false);
				auto conclusion = instantiate_invariant(lhsVar.decl);
				return plankton::implies(*partial_post->now, *conclusion);
			} else {
				return false;
			}
		}

		auto& rhsVar = extract_or_fail(rhs, &lhs);
		return derefptr_varimmi_owned(lhs, lhsVar, rhsVar)
		    || derefptr_varimmi_insert(lhs, lhsVar, rhsVar)
		    || derefptr_varimmi_unlink(lhs, lhsVar, rhsVar)
		    || has_enabled_future_predicate(*pre.now, pre, cmd);
	}

	bool derefdata_varimmi(const Assignment& cmd, const Dereference& lhs, const VariableExpression& lhsVar, const Expression& rhs) override {
		if (!plankton::config->is_flow_field_local(lhsVar.type(), lhs.fieldname)) {
			return false;
		}

		// check invariant on modified node (lhsVar) ==> update is local to lhsVar
		if (check_invariant_locally(lhs, lhsVar, rhs)) {
			return true;
		}

		// as a last resort, search for an enabled future
		return has_enabled_future_predicate(*pre.now, pre, cmd);
	}
};


bool plankton::post_maintains_invariant(const Annotation& pre, const cola::Assignment& cmd) {
	InvariantComputer computer(pre);
	return compute_assignment_switch(computer, cmd);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

inline void make_shallow_inlining(ConjunctionFormula& dst,
	BinaryExpression::Operator op, const Expression& lhs, const Expression& rhs, // source expression
	BinaryExpression::Operator iop, const Expression& ilhs, const Expression& irhs) // to be inlined
{
	// TODO: de-uglify 
	using Op = BinaryExpression::Operator;

	switch (op) {
		case Op::GT: make_shallow_inlining(dst, Op::LT, rhs, lhs, iop, ilhs, irhs); return;
		case Op::GEQ: make_shallow_inlining(dst, Op::LEQ, rhs, lhs, iop, ilhs, irhs); return;
		case Op::NEQ: return;
		default: break;
	}
	switch (iop) {
		case Op::GT: make_shallow_inlining(dst, op, lhs, rhs, Op::LT, irhs, ilhs); return;
		case Op::GEQ: make_shallow_inlining(dst, op, lhs, rhs, Op::LEQ, irhs, ilhs); return;
		case Op::NEQ: return;
		default: break;
	}

	auto new_op = Op::EQ;
	if (op == Op::LT || iop == Op::LT) new_op = Op::LT;
	else if (op == Op::LEQ || iop == Op::LEQ) new_op = Op::LEQ;
	auto mk_rel = [new_op](const Expression& lhs, const Expression& rhs) {
		return std::make_unique<ExpressionAxiom>(std::make_unique<BinaryExpression>(new_op, cola::copy(lhs), cola::copy(rhs)));
	};

	if (plankton::syntactically_equal(rhs, ilhs)) {
		dst.conjuncts.push_back(mk_rel(lhs, irhs));
	}
	if (plankton::syntactically_equal(lhs, irhs)) {
		dst.conjuncts.push_back(mk_rel(ilhs, rhs));
	}

	if (op == Op::EQ && iop == Op::EQ) {
		if (plankton::syntactically_equal(lhs, ilhs)) {
			dst.conjuncts.push_back(mk_rel(rhs, irhs));
		}
		if (plankton::syntactically_equal(rhs, irhs)) {
			dst.conjuncts.push_back(mk_rel(lhs, ilhs));
		}
	}
}

std::unique_ptr<ConjunctionFormula> shallow_inline(const ConjunctionFormula& now, const AxiomConjunctionFormula& flat) {
	// TODO: de-uglify 

	// extraction
	auto extract = [](const SimpleFormula& formula) -> std::tuple<bool, BinaryExpression::Operator, const Expression*, const Expression*>{
		if (auto axiom = dynamic_cast<const ExpressionAxiom*>(&formula)) {
			if (auto binary = dynamic_cast<const BinaryExpression*>(axiom->expr.get())) {
				if (binary->op != BinaryExpression::Operator::AND && binary->op != BinaryExpression::Operator::OR) {
					return std::make_tuple(true, binary->op, binary->lhs.get(), binary->rhs.get());
				}
			}
		}
		return std::make_tuple(false, BinaryExpression::Operator::EQ, nullptr, nullptr);
	};

	// extract from 'flat' only once
	std::vector<std::tuple<BinaryExpression::Operator, const Expression*, const Expression*>> prepared;
	prepared.reserve(flat.conjuncts.size());
	for (const auto& conjunct : flat.conjuncts) {
		auto [is_binary, op, lhs, rhs] = extract(*conjunct);
		if (is_binary) {
			prepared.push_back(std::make_tuple(op, lhs, rhs));
		}
	}

	// inspect 'now'
	auto result = std::make_unique<ConjunctionFormula>();
	for (const auto& conjunct : now.conjuncts) {
		auto [is_binary, op, lhs, rhs] = extract(*conjunct);
		if (is_binary) {
			for (auto [iop, ilhs, irhs] : prepared) {
				make_shallow_inlining(*result, op, *lhs, *rhs, iop, *ilhs, *irhs);
			}
		}
	}
	return result;
}


std::unique_ptr<Annotation> plankton::post_full(std::unique_ptr<Annotation> pre, const Assume& cmd) {
	std::cout << std::endl << "ΩΩΩ post "; cola::print(cmd, std::cout); // std::cout << "  ";
	// plankton::print(*pre->now, std::cout); std::cout << std::endl;

	// TODO important: enable the following?
	// auto check = is_of_type<BinaryExpression>(*cmd.expr);
	// if (check.first && check.second->op == BinaryExpression::Operator::EQ) {
	// 	try {
	// 		Assignment dummy_assign(cola::copy(*check.second->lhs), cola::copy(*check.second->rhs));
	// 		auto post = make_post_full(plankton::copy(*pre), dummy_assign, program, false);
	// 		dummy_assign.lhs.swap(dummy_assign.rhs);
	// 		pre = make_post_full(std::move(pre), dummy_assign, program, false);
	// 		pre->now->conjuncts.insert(
	// 			pre->now->conjuncts.begin(), std::make_move_iterator(post->now->conjuncts.begin()), std::make_move_iterator(post->now->conjuncts.end())
	// 		);
	// 		pre->time.insert(
	// 			pre->time.begin(), std::make_move_iterator(post->time.begin()), std::make_move_iterator(post->time.end())
	// 		);
	// 	} catch (UnsupportedConstructError err) {
	// 		// do nothing: the assumption simply does not adhere to the expressions normal form we assume for assignments
	// 	}
	// }
	
	auto flat = plankton::flatten(cola::copy(*cmd.expr));
	auto inlined = shallow_inline(*pre->now, *flat);
	pre->now = plankton::conjoin(std::move(pre->now), plankton::conjoin(std::move(inlined), std::move(flat)));
	pre = post_process(std::move(pre));

	// std::cout << "  ~~> " << std::endl << "  ";
	// plankton::print(*pre->now, std::cout); std::cout << std::endl;

	return pre;	
}

inline void add_default_field_value_ptr(ConjunctionFormula& dst, std::unique_ptr<Dereference> field) {
	assert(field);
	dst.conjuncts.push_back(std::make_unique<ExpressionAxiom>(std::make_unique<BinaryExpression>(
		BinaryExpression::Operator::EQ, std::move(field), std::make_unique<NullValue>()
	)));
}
inline void add_default_field_value_bool(ConjunctionFormula& dst, std::unique_ptr<Dereference> field) {
	assert(field);
	dst.conjuncts.push_back(std::make_unique<ExpressionAxiom>(std::make_unique<BinaryExpression>(
		BinaryExpression::Operator::EQ, std::move(field), std::make_unique<BooleanValue>(false)
	)));
}
inline void add_default_field_value_data(ConjunctionFormula& dst, std::unique_ptr<Dereference> field) {
	assert(field);
	dst.conjuncts.push_back(std::make_unique<ExpressionAxiom>(std::make_unique<BinaryExpression>(
		BinaryExpression::Operator::LT, std::make_unique<MinValue>(), cola::copy(*field)
	)));
	assert(field);
	dst.conjuncts.push_back(std::make_unique<ExpressionAxiom>(std::make_unique<BinaryExpression>(
		BinaryExpression::Operator::LT, std::move(field), std::make_unique<MaxValue>()
	)));
}

inline void add_default_field_value(ConjunctionFormula& dst, std::unique_ptr<Dereference> field) {
	switch (field->sort()) {
		case Sort::PTR: add_default_field_value_ptr(dst, std::move(field)); break;
		case Sort::DATA: add_default_field_value_data(dst, std::move(field)); break;
		case Sort::BOOL: add_default_field_value_bool(dst, std::move(field)); break;
		case Sort::VOID: break;
	}
}

std::unique_ptr<Annotation> plankton::post_full(std::unique_ptr<Annotation> pre, const Malloc& cmd) {
	std::cout << std::endl << "ΩΩΩ post "; // cola::print(cmd, std::cout); //std::cout << "  ";
	// plankton::print(*pre->now, std::cout); std::cout << std::endl;

	// TODO: do post for a dummy assignment 'lhs = lhs' to avoid code duplication?
	VariableExpression lhs(cmd.lhs);
	check_purity_var(lhs);

	// destroy knowledge about lhs
	auto now = std::move(pre->now);
	now->conjuncts = container_search_and_destroy(std::move(now->conjuncts), lhs);
	pre->time = container_search_and_destroy_time(std::move(pre->time), lhs);

	// add new knowledge about lhs
	now->conjuncts.push_back(std::make_unique<ExpressionAxiom>(std::make_unique<BinaryExpression>( // lhs != NULL
		BinaryExpression::Operator::NEQ, cola::copy(lhs), std::make_unique<NullValue>()
	)));
	now->conjuncts.push_back( // owned(lhs)
		std::make_unique<OwnershipAxiom>(std::make_unique<VariableExpression>(cmd.lhs))
	);
	now->conjuncts.push_back( // flow(lhs)=empty
		std::make_unique<NegatedAxiom>(std::make_unique<HasFlowAxiom>(std::make_unique<VariableExpression>(cmd.lhs)))
	);
	for (const auto& [fieldname, type] : lhs.type().fields) {
		add_default_field_value(*now, std::make_unique<Dereference>(cola::copy(lhs), fieldname));
	}
	// TODO: more knowledge needed?

	// std::cout << "  ~~> " << std::endl << "  ";
	// plankton::print(*now, std::cout); std::cout << std::endl;

	pre->now = std::move(now);
	return pre;
}

bool plankton::post_maintains_invariant(const Annotation& pre, const cola::Malloc& cmd) {
	auto post = plankton::post_full(std::make_unique<Annotation>(plankton::copy(*pre.now)), cmd);
	auto lhsinv = plankton::config->get_invariant().instantiate(cmd.lhs);
	return plankton::implies(*post->now, *lhsinv);
}



// TODO: can the quick check be done more efficiently when tailoring towards interference scenario??
struct MaintenanceQuickChecker : public AssignmentComputer<bool> {
	const ConjunctionFormula& pre;
	const ConjunctionFormula& maintained;
	MaintenanceQuickChecker(const ConjunctionFormula& pre_, const ConjunctionFormula& maintained_) : pre(pre_), maintained(maintained_) {}

	bool check_var(const VariableExpression& lhs) {
		return !contains_expression(maintained, lhs);
	}

	bool check_deref(const Dereference& lhs, const VariableExpression& lhsVar) {
		// check if 'lhs.fieldname' is not used in 'maintained' ==> valuation remains unchanged
		auto dereferences = extract_dereferences<const Dereference*>(maintained, lhs.fieldname);
		if (dereferences.empty()) return true;

		// check if 'lhsVar' is owned (has no alias) and not used in 'maintained' ==> valuation remains unchanged ==> turns out to be too slow
		// if (!contains_expression(maintained, lhsVar) && is_owned_quick(pre, lhsVar)) return true;

		// go through all dereferences in maintained and check that thei are owned and unequal to 'lhsVar'
		for (auto deref : dereferences) {
			auto [is_var, var_ptr] = plankton::is_of_type<VariableExpression>(*deref->expr);
			if (is_var && !syntactically_equal(lhsVar, *var_ptr) && is_owned_quick(pre, *var_ptr)) continue;
			return false;
		}
		return true;
	}

	bool var_expr(const Assignment& /*cmd*/, const VariableExpression& lhs, const Expression& /*rhs*/) override {
		return check_var(lhs);
	}

	bool var_derefptr(const Assignment& /*cmd*/, const VariableExpression& lhs, const Dereference& /*rhs*/, const VariableExpression& /*rhsVar*/) override {
		return check_var(lhs);
	}

	bool var_derefdata(const Assignment& /*cmd*/, const VariableExpression& lhs, const Dereference& /*rhs*/, const VariableExpression& /*rhsVar*/) override {
		return check_var(lhs);
	}

	bool derefptr_varimmi(const Assignment& /*cmd*/, const Dereference& lhs, const VariableExpression& lhsVar, const Expression& /*rhs*/) override {
		return check_deref(lhs, lhsVar);
	}

	bool derefdata_varimmi(const Assignment& /*cmd*/, const Dereference& lhs, const VariableExpression& lhsVar, const Expression& /*rhs*/) override {
		return check_deref(lhs, lhsVar);
	}
};

bool plankton::post_maintains_formula(const ConjunctionFormula& pre, const ConjunctionFormula& maintained, const cola::Assignment& cmd) {
	auto combined = combine_to_annotation(pre, maintained);

	// quick check
	if (plankton::config->post_maintains_formula_quick_check) {
		MaintenanceQuickChecker checker(*combined->now, maintained);
		bool discharged = compute_assignment_switch(checker, cmd);
		if (discharged) return true;
	}

	// deep check;
	auto post = make_post_full(std::move(combined), cmd, false);
	return plankton::implies(*post->now, maintained);
}
