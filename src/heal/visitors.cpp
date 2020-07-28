#include "heal/visitors.hpp"

#include <cassert>
#include "heal/logic.hpp"

using namespace cola;
using namespace heal;


template<typename V, typename C>
void visit_all(V& visitor, const C& container) {
	for (const auto& elem : container) {
		elem->accept(visitor);
	}
}


void LogicListener::visit(const ConjunctionFormula& formula) {
	enter(formula);
	visit_all(*this, formula.conjuncts);
	exit(formula);
}

void LogicListener::visit(const AxiomConjunctionFormula& formula) {
	enter(formula);
	visit_all(*this, formula.conjuncts);
	exit(formula);
}

void LogicListener::visit(const ImplicationFormula& formula) {
	enter(formula);
	formula.premise->accept(*this);
	formula.conclusion->accept(*this);
	exit(formula);
}

void LogicListener::visit(const NegatedAxiom& formula) {
	enter(formula);
	formula.axiom->accept(*this);
	exit(formula);
}

void LogicListener::visit(const PastPredicate& formula) {
	enter(formula);
	formula.formula->accept(*this);
	exit(formula);
}

void LogicListener::visit(const FuturePredicate& formula) {
	enter(formula);
	formula.pre->accept(*this);
	formula.post->accept(*this);
	exit(formula);
}

void LogicListener::visit(const Annotation& formula) {
	enter(formula);
	formula.now->accept(*this);
	visit_all(*this, formula.time);
	exit(formula);
}


void LogicNonConstListener::visit(ConjunctionFormula& formula) {
	enter(formula);
	visit_all(*this, formula.conjuncts);
	exit(formula);
}

void LogicNonConstListener::visit(AxiomConjunctionFormula& formula) {
	enter(formula);
	visit_all(*this, formula.conjuncts);
	exit(formula);
}

void LogicNonConstListener::visit(ImplicationFormula& formula) {
	enter(formula);
	formula.premise->accept(*this);
	formula.conclusion->accept(*this);
	exit(formula);
}

void LogicNonConstListener::visit(NegatedAxiom& formula) {
	enter(formula);
	formula.axiom->accept(*this);
	exit(formula);
}

void LogicNonConstListener::visit(PastPredicate& formula) {
	enter(formula);
	formula.formula->accept(*this);
	exit(formula);
}

void LogicNonConstListener::visit(FuturePredicate& formula) {
	enter(formula);
	formula.pre->accept(*this);
	formula.post->accept(*this);
	exit(formula);
}

void LogicNonConstListener::visit(Annotation& formula) {
	enter(formula);
	formula.now->accept(*this);
	visit_all(*this, formula.time);
	exit(formula);
}
