#include "heal/visitors.hpp"

#include "heal/logic.hpp"

using namespace cola;
using namespace heal;


template<typename V, typename C>
void visit_all(V& visitor, C& container) {
	for (auto& elem : container) {
		elem->accept(visitor);
	}
}

template<typename V, typename C>
void visit_all_values(V& visitor, C& container) {
    for (auto& elem : container) {
        elem.second->accept(visitor);
    }
}

void LogicListener::visit(const SymbolicVariable& expression) { enter(expression); exit(expression); }
void LogicListener::visit(const SymbolicBool& expression) { enter(expression); exit(expression); }
void LogicListener::visit(const SymbolicNull& expression) { enter(expression); exit(expression); }
void LogicListener::visit(const SymbolicMin& expression) { enter(expression); exit(expression); }
void LogicListener::visit(const SymbolicMax& expression) { enter(expression); exit(expression); }
void LogicListener::visit(const SeparatingConjunction& formula) { enter(formula); visit_all(*this, formula.conjuncts); exit(formula); }
void LogicListener::visit(const SeparatingImplication& formula) { enter(formula); formula.premise->accept(*this); formula.conclusion->accept(*this); exit(formula); }
void LogicListener::visit(const PointsToAxiom& formula) { enter(formula); formula.node->accept(*this); visit_all_values(*this, formula.fieldToValue); exit(formula); }
void LogicListener::visit(const EqualsToAxiom& formula) { enter(formula); formula.value->accept(*this); exit(formula); }
void LogicListener::visit(const SymbolicAxiom& formula) { enter(formula); formula.lhs->accept(*this); formula.rhs->accept(*this); exit(formula); }
void LogicListener::visit(const StackDisjunction& formula) { enter(formula); visit_all(*this, formula.axioms); exit(formula); }
void LogicListener::visit(const InflowContainsValueAxiom& formula) { enter(formula); formula.value->accept(*this); exit(formula); }
void LogicListener::visit(const InflowContainsRangeAxiom& formula) { enter(formula); formula.valueLow->accept(*this); formula.valueHigh->accept(*this); exit(formula); }
void LogicListener::visit(const InflowEmptinessAxiom& formula) { enter(formula); exit(formula); }
void LogicListener::visit(const ObligationAxiom& formula) { enter(formula); formula.key->accept(*this); exit(formula); }
void LogicListener::visit(const FulfillmentAxiom& formula) { enter(formula); formula.key->accept(*this); exit(formula); }
void LogicListener::visit(const PastPredicate& formula) { enter(formula); formula.formula->accept(*this); exit(formula); }
void LogicListener::visit(const FuturePredicate& formula) { enter(formula); formula.pre->accept(*this); formula.post->accept(*this); exit(formula); }
void LogicListener::visit(const Annotation& formula) { enter(formula); formula.now->accept(*this); visit_all(*this, formula.time); exit(formula); }


void LogicNonConstListener::visit(SymbolicVariable& expression) { enter(expression); exit(expression); }
void LogicNonConstListener::visit(SymbolicBool& expression) { enter(expression); exit(expression); }
void LogicNonConstListener::visit(SymbolicNull& expression) { enter(expression); exit(expression); }
void LogicNonConstListener::visit(SymbolicMin& expression) { enter(expression); exit(expression); }
void LogicNonConstListener::visit(SymbolicMax& expression) { enter(expression); exit(expression); }
void LogicNonConstListener::visit(SeparatingConjunction& formula) { enter(formula); visit_all(*this, formula.conjuncts); exit(formula); }
void LogicNonConstListener::visit(SeparatingImplication& formula) { enter(formula); formula.premise->accept(*this); formula.conclusion->accept(*this); exit(formula); }
void LogicNonConstListener::visit(PointsToAxiom& formula) { enter(formula); formula.node->accept(*this); visit_all_values(*this, formula.fieldToValue); exit(formula); }
void LogicNonConstListener::visit(EqualsToAxiom& formula) { enter(formula); formula.value->accept(*this); exit(formula); }
void LogicNonConstListener::visit(SymbolicAxiom& formula) { enter(formula); formula.lhs->accept(*this); formula.rhs->accept(*this); exit(formula); }
void LogicNonConstListener::visit(StackDisjunction& formula) { enter(formula); visit_all(*this, formula.axioms); exit(formula); }
void LogicNonConstListener::visit(InflowContainsValueAxiom& formula) { enter(formula); formula.value->accept(*this); exit(formula); }
void LogicNonConstListener::visit(InflowContainsRangeAxiom& formula) { enter(formula); formula.valueLow->accept(*this); formula.valueHigh->accept(*this); exit(formula); }
void LogicNonConstListener::visit(InflowEmptinessAxiom& formula) { enter(formula); exit(formula); }
void LogicNonConstListener::visit(ObligationAxiom& formula) { enter(formula); formula.key->accept(*this); exit(formula); }
void LogicNonConstListener::visit(FulfillmentAxiom& formula) { enter(formula); formula.key->accept(*this); exit(formula); }
void LogicNonConstListener::visit(PastPredicate& formula) { enter(formula); formula.formula->accept(*this); exit(formula); }
void LogicNonConstListener::visit(FuturePredicate& formula) { enter(formula); formula.pre->accept(*this); formula.post->accept(*this); exit(formula); }
void LogicNonConstListener::visit(Annotation& formula) { enter(formula); formula.now->accept(*this); visit_all(*this, formula.time); exit(formula); }
