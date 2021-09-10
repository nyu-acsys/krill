#include "logics/visitors.hpp"

#include "programs/ast.hpp"
#include "logics/ast.hpp"
#include "util/shortcuts.hpp"

using namespace plankton;


//
// Walking the AST
//

template<typename T, typename U>
void HandleMemoryAxiom(T& listener, U& axiom) {
    axiom.node->Accept(listener);
    axiom.flow->Accept(listener);
    for (auto& pair : axiom.fieldToValue) pair.second->Accept(listener);
}

void LogicVisitor::Walk(const SymbolicVariable& /*object*/) { /* do nothing */ }
void LogicVisitor::Walk(const SymbolicBool& /*object*/) { /* do nothing */ }
void LogicVisitor::Walk(const SymbolicNull& /*object*/) { /* do nothing */ }
void LogicVisitor::Walk(const SymbolicMin& /*object*/) { /* do nothing */ }
void LogicVisitor::Walk(const SymbolicMax& /*object*/) { /* do nothing */ }
void LogicVisitor::Walk(const LocalMemoryResource& object) { HandleMemoryAxiom(*this, object); }
void LogicVisitor::Walk(const SharedMemoryCore& object) { HandleMemoryAxiom(*this, object); }
void LogicVisitor::Walk(const SeparatingConjunction& object) {
    for (const auto& elem : object.conjuncts) elem->Accept(*this);
}
void LogicVisitor::Walk(const EqualsToAxiom& object) {
    object.value->Accept(*this);
}
void LogicVisitor::Walk(const StackAxiom& object) {
    object.lhs->Accept(*this);
    object.rhs->Accept(*this);
}
void LogicVisitor::Walk(const InflowEmptinessAxiom& object) {
    object.flow->Accept(*this);
}
void LogicVisitor::Walk(const InflowContainsValueAxiom& object) {
    object.flow->Accept(*this);
    object.value->Accept(*this);
}
void LogicVisitor::Walk(const InflowContainsRangeAxiom& object) {
    object.flow->Accept(*this);
    object.valueLow->Accept(*this);
    object.valueHigh->Accept(*this);
}
void LogicVisitor::Walk(const ObligationAxiom& object) {
    object.key->Accept(*this);
}
void LogicVisitor::Walk(const FulfillmentAxiom& /*object*/) { /* do nothing */ }
void LogicVisitor::Walk(const NonSeparatingImplication& object) {
    object.premise->Accept(*this);
    object.conclusion->Accept(*this);
}
void LogicVisitor::Walk(const ImplicationSet& object) {
    for (const auto& elem : object.conjuncts) elem->Accept(*this);
}
void LogicVisitor::Walk(const PastPredicate& object) {
    object.formula->Accept(*this);
}
void LogicVisitor::Walk(const FuturePredicate& object) {
    object.pre->Accept(*this);
    object.post->Accept(*this);
}
void LogicVisitor::Walk(const Annotation& object) {
    object.now->Accept(*this);
    for (const auto& elem : object.past) elem->Accept(*this);
    for (const auto& elem : object.future) elem->Accept(*this);
}

void MutableLogicVisitor::Walk(SymbolicVariable& /*object*/) { /* do nothing */ }
void MutableLogicVisitor::Walk(SymbolicBool& /*object*/) { /* do nothing */ }
void MutableLogicVisitor::Walk(SymbolicNull& /*object*/) { /* do nothing */ }
void MutableLogicVisitor::Walk(SymbolicMin& /*object*/) { /* do nothing */ }
void MutableLogicVisitor::Walk(SymbolicMax& /*object*/) { /* do nothing */ }
void MutableLogicVisitor::Walk(LocalMemoryResource& object) { HandleMemoryAxiom(*this, object); }
void MutableLogicVisitor::Walk(SharedMemoryCore& object) { HandleMemoryAxiom(*this, object); }
void MutableLogicVisitor::Walk(SeparatingConjunction& object) {
    for (auto& elem : object.conjuncts) elem->Accept(*this);
}
void MutableLogicVisitor::Walk(EqualsToAxiom& object) {
    object.value->Accept(*this);
}
void MutableLogicVisitor::Walk(StackAxiom& object) {
    object.lhs->Accept(*this);
    object.rhs->Accept(*this);
}
void MutableLogicVisitor::Walk(InflowEmptinessAxiom& object) {
    object.flow->Accept(*this);
}
void MutableLogicVisitor::Walk(InflowContainsValueAxiom& object) {
    object.flow->Accept(*this);
    object.value->Accept(*this);
}
void MutableLogicVisitor::Walk(InflowContainsRangeAxiom& object) {
    object.flow->Accept(*this);
    object.valueLow->Accept(*this);
    object.valueHigh->Accept(*this);
}
void MutableLogicVisitor::Walk(ObligationAxiom& object) {
    object.key->Accept(*this);
}
void MutableLogicVisitor::Walk(FulfillmentAxiom& /*object*/) { /* do nothing */ }
void MutableLogicVisitor::Walk(NonSeparatingImplication& object) {
    object.premise->Accept(*this);
    object.conclusion->Accept(*this);
}
void MutableLogicVisitor::Walk(ImplicationSet& object) {
    for (auto& elem : object.conjuncts) elem->Accept(*this);
}
void MutableLogicVisitor::Walk(PastPredicate& object) {
    object.formula->Accept(*this);
}
void MutableLogicVisitor::Walk(FuturePredicate& object) {
    object.pre->Accept(*this);
    object.post->Accept(*this);
}
void MutableLogicVisitor::Walk(Annotation& object) {
    object.now->Accept(*this);
    for (auto& elem : object.past) elem->Accept(*this);
    for (auto& elem : object.future) elem->Accept(*this);
}


//
// Visitors
//

struct LogicVisitorNotImplementedException : public ExceptionWithMessage {
    explicit LogicVisitorNotImplementedException(std::string base, std::string arg) : ExceptionWithMessage(
            "Call to unimplemented visitor member function with argument type '" + std::move(arg) +
            "' of base class '" + std::move(base) + "'.") {}
};

#define COMPLAIN(B,T) throw LogicVisitorNotImplementedException(#B,#T)

void BaseLogicVisitor::Visit(const SymbolicVariable& /*object*/) { COMPLAIN(BaseLogicVisitor, const SymbolicVariable&); }
void BaseLogicVisitor::Visit(const SymbolicBool& /*object*/) { COMPLAIN(BaseLogicVisitor, const SymbolicBool&); }
void BaseLogicVisitor::Visit(const SymbolicNull& /*object*/) { COMPLAIN(BaseLogicVisitor, const SymbolicNull&); }
void BaseLogicVisitor::Visit(const SymbolicMin& /*object*/) { COMPLAIN(BaseLogicVisitor, const SymbolicMin&); }
void BaseLogicVisitor::Visit(const SymbolicMax& /*object*/) { COMPLAIN(BaseLogicVisitor, const SymbolicMax&); }
void BaseLogicVisitor::Visit(const SeparatingConjunction& /*object*/) { COMPLAIN(BaseLogicVisitor, const SeparatingConjunction&); }
void BaseLogicVisitor::Visit(const LocalMemoryResource& /*object*/) { COMPLAIN(BaseLogicVisitor, const LocalMemoryResource&); }
void BaseLogicVisitor::Visit(const SharedMemoryCore& /*object*/) { COMPLAIN(BaseLogicVisitor, const SharedMemoryCore&); }
void BaseLogicVisitor::Visit(const EqualsToAxiom& /*object*/) { COMPLAIN(BaseLogicVisitor, const EqualsToAxiom&); }
void BaseLogicVisitor::Visit(const StackAxiom& /*object*/) { COMPLAIN(BaseLogicVisitor, const StackAxiom&); }
void BaseLogicVisitor::Visit(const InflowEmptinessAxiom& /*object*/) { COMPLAIN(BaseLogicVisitor, const InflowEmptinessAxiom&); }
void BaseLogicVisitor::Visit(const InflowContainsValueAxiom& /*object*/) { COMPLAIN(BaseLogicVisitor, const InflowContainsValueAxiom&); }
void BaseLogicVisitor::Visit(const InflowContainsRangeAxiom& /*object*/) { COMPLAIN(BaseLogicVisitor, const InflowContainsRangeAxiom&); }
void BaseLogicVisitor::Visit(const ObligationAxiom& /*object*/) { COMPLAIN(BaseLogicVisitor, const ObligationAxiom&); }
void BaseLogicVisitor::Visit(const FulfillmentAxiom& /*object*/) { COMPLAIN(BaseLogicVisitor, const FulfillmentAxiom&); }
void BaseLogicVisitor::Visit(const NonSeparatingImplication& /*object*/) { COMPLAIN(BaseLogicVisitor, const NonSeparatingImplication&); }
void BaseLogicVisitor::Visit(const ImplicationSet& /*object*/) { COMPLAIN(BaseLogicVisitor, const ImplicationSet&); }
void BaseLogicVisitor::Visit(const PastPredicate& /*object*/) { COMPLAIN(BaseLogicVisitor, const PastPredicate&); }
void BaseLogicVisitor::Visit(const FuturePredicate& /*object*/) { COMPLAIN(BaseLogicVisitor, const FuturePredicate&); }
void BaseLogicVisitor::Visit(const Annotation& /*object*/) { COMPLAIN(BaseLogicVisitor, const Annotation&); }

void MutableBaseLogicVisitor::Visit(SymbolicVariable& /*object*/) { COMPLAIN(MutableBaseLogicVisitor, SymbolicVariable&); }
void MutableBaseLogicVisitor::Visit(SymbolicBool& /*object*/) { COMPLAIN(MutableBaseLogicVisitor, SymbolicBool&); }
void MutableBaseLogicVisitor::Visit(SymbolicNull& /*object*/) { COMPLAIN(MutableBaseLogicVisitor, SymbolicNull&); }
void MutableBaseLogicVisitor::Visit(SymbolicMin& /*object*/) { COMPLAIN(MutableBaseLogicVisitor, SymbolicMin&); }
void MutableBaseLogicVisitor::Visit(SymbolicMax& /*object*/) { COMPLAIN(MutableBaseLogicVisitor, SymbolicMax&); }
void MutableBaseLogicVisitor::Visit(SeparatingConjunction& /*object*/) { COMPLAIN(MutableBaseLogicVisitor, SeparatingConjunction&); }
void MutableBaseLogicVisitor::Visit(LocalMemoryResource& /*object*/) { COMPLAIN(MutableBaseLogicVisitor, LocalMemoryResource&); }
void MutableBaseLogicVisitor::Visit(SharedMemoryCore& /*object*/) { COMPLAIN(MutableBaseLogicVisitor, SharedMemoryCore&); }
void MutableBaseLogicVisitor::Visit(EqualsToAxiom& /*object*/) { COMPLAIN(MutableBaseLogicVisitor, EqualsToAxiom&); }
void MutableBaseLogicVisitor::Visit(StackAxiom& /*object*/) { COMPLAIN(MutableBaseLogicVisitor, StackAxiom&); }
void MutableBaseLogicVisitor::Visit(InflowEmptinessAxiom& /*object*/) { COMPLAIN(MutableBaseLogicVisitor, InflowEmptinessAxiom&); }
void MutableBaseLogicVisitor::Visit(InflowContainsValueAxiom& /*object*/) { COMPLAIN(MutableBaseLogicVisitor, InflowContainsValueAxiom&); }
void MutableBaseLogicVisitor::Visit(InflowContainsRangeAxiom& /*object*/) { COMPLAIN(MutableBaseLogicVisitor, InflowContainsRangeAxiom&); }
void MutableBaseLogicVisitor::Visit(ObligationAxiom& /*object*/) { COMPLAIN(MutableBaseLogicVisitor, ObligationAxiom&); }
void MutableBaseLogicVisitor::Visit(FulfillmentAxiom& /*object*/) { COMPLAIN(MutableBaseLogicVisitor, FulfillmentAxiom&); }
void MutableBaseLogicVisitor::Visit(NonSeparatingImplication& /*object*/) { COMPLAIN(MutableBaseLogicVisitor, SeparatingImplication &); }
void MutableBaseLogicVisitor::Visit(ImplicationSet& /*object*/) { COMPLAIN(MutableBaseLogicVisitor, Invariant &); }
void MutableBaseLogicVisitor::Visit(PastPredicate& /*object*/) { COMPLAIN(MutableBaseLogicVisitor, PastPredicate&); }
void MutableBaseLogicVisitor::Visit(FuturePredicate& /*object*/) { COMPLAIN(MutableBaseLogicVisitor, FuturePredicate&); }
void MutableBaseLogicVisitor::Visit(Annotation& /*object*/) { COMPLAIN(MutableBaseLogicVisitor, Annotation&); }


void DefaultLogicVisitor::Visit(const SymbolicVariable& /*object*/) { /* do nothing */ }
void DefaultLogicVisitor::Visit(const SymbolicBool& /*object*/) { /* do nothing */ }
void DefaultLogicVisitor::Visit(const SymbolicNull& /*object*/) { /* do nothing */ }
void DefaultLogicVisitor::Visit(const SymbolicMin& /*object*/) { /* do nothing */ }
void DefaultLogicVisitor::Visit(const SymbolicMax& /*object*/) { /* do nothing */ }
void DefaultLogicVisitor::Visit(const SeparatingConjunction& /*object*/) { /* do nothing */ }
void DefaultLogicVisitor::Visit(const LocalMemoryResource& /*object*/) { /* do nothing */ }
void DefaultLogicVisitor::Visit(const SharedMemoryCore& /*object*/) { /* do nothing */ }
void DefaultLogicVisitor::Visit(const EqualsToAxiom& /*object*/) { /* do nothing */ }
void DefaultLogicVisitor::Visit(const StackAxiom& /*object*/) { /* do nothing */ }
void DefaultLogicVisitor::Visit(const InflowEmptinessAxiom& /*object*/) { /* do nothing */ }
void DefaultLogicVisitor::Visit(const InflowContainsValueAxiom& /*object*/) { /* do nothing */ }
void DefaultLogicVisitor::Visit(const InflowContainsRangeAxiom& /*object*/) { /* do nothing */ }
void DefaultLogicVisitor::Visit(const ObligationAxiom& /*object*/) { /* do nothing */ }
void DefaultLogicVisitor::Visit(const FulfillmentAxiom& /*object*/) { /* do nothing */ }
void DefaultLogicVisitor::Visit(const NonSeparatingImplication& /*object*/) { /* do nothing */ }
void DefaultLogicVisitor::Visit(const ImplicationSet& /*object*/) { /* do nothing */ }
void DefaultLogicVisitor::Visit(const PastPredicate& /*object*/) { /* do nothing */ }
void DefaultLogicVisitor::Visit(const FuturePredicate& /*object*/) { /* do nothing */ }
void DefaultLogicVisitor::Visit(const Annotation& /*object*/) { /* do nothing */ }

void MutableDefaultLogicVisitor::Visit(SymbolicVariable& /*object*/) { /* do nothing */ }
void MutableDefaultLogicVisitor::Visit(SymbolicBool& /*object*/) { /* do nothing */ }
void MutableDefaultLogicVisitor::Visit(SymbolicNull& /*object*/) { /* do nothing */ }
void MutableDefaultLogicVisitor::Visit(SymbolicMin& /*object*/) { /* do nothing */ }
void MutableDefaultLogicVisitor::Visit(SymbolicMax& /*object*/) { /* do nothing */ }
void MutableDefaultLogicVisitor::Visit(SeparatingConjunction& /*object*/) { /* do nothing */ }
void MutableDefaultLogicVisitor::Visit(LocalMemoryResource& /*object*/) { /* do nothing */ }
void MutableDefaultLogicVisitor::Visit(SharedMemoryCore& /*object*/) { /* do nothing */ }
void MutableDefaultLogicVisitor::Visit(EqualsToAxiom& /*object*/) { /* do nothing */ }
void MutableDefaultLogicVisitor::Visit(StackAxiom& /*object*/) { /* do nothing */ }
void MutableDefaultLogicVisitor::Visit(InflowEmptinessAxiom& /*object*/) { /* do nothing */ }
void MutableDefaultLogicVisitor::Visit(InflowContainsValueAxiom& /*object*/) { /* do nothing */ }
void MutableDefaultLogicVisitor::Visit(InflowContainsRangeAxiom& /*object*/) { /* do nothing */ }
void MutableDefaultLogicVisitor::Visit(ObligationAxiom& /*object*/) { /* do nothing */ }
void MutableDefaultLogicVisitor::Visit(FulfillmentAxiom& /*object*/) { /* do nothing */ }
void MutableDefaultLogicVisitor::Visit(NonSeparatingImplication& /*object*/) { /* do nothing */ }
void MutableDefaultLogicVisitor::Visit(ImplicationSet& /*object*/) { /* do nothing */ }
void MutableDefaultLogicVisitor::Visit(PastPredicate& /*object*/) { /* do nothing */ }
void MutableDefaultLogicVisitor::Visit(FuturePredicate& /*object*/) { /* do nothing */ }
void MutableDefaultLogicVisitor::Visit(Annotation& /*object*/) { /* do nothing */ }


void LogicListener::Visit(const SymbolicVariable& object) { Enter(object); Enter(object.Decl()); Walk(object); }
void LogicListener::Visit(const SymbolicBool& object) { Enter(object); Walk(object); }
void LogicListener::Visit(const SymbolicNull& object) { Enter(object); Walk(object); }
void LogicListener::Visit(const SymbolicMin& object) { Enter(object); Walk(object); }
void LogicListener::Visit(const SymbolicMax& object) { Enter(object); Walk(object); }
void LogicListener::Visit(const SeparatingConjunction& object) { Enter(object); Walk(object); }
void LogicListener::Visit(const LocalMemoryResource& object) { Enter(object); Walk(object); }
void LogicListener::Visit(const SharedMemoryCore& object) { Enter(object); Walk(object); }
void LogicListener::Visit(const EqualsToAxiom& object) { Enter(object); Enter(object.variable); Walk(object); }
void LogicListener::Visit(const StackAxiom& object) { Enter(object); Walk(object); }
void LogicListener::Visit(const InflowEmptinessAxiom& object) { Enter(object); Walk(object); }
void LogicListener::Visit(const InflowContainsValueAxiom& object) { Enter(object); Walk(object); }
void LogicListener::Visit(const InflowContainsRangeAxiom& object) { Enter(object); Walk(object); }
void LogicListener::Visit(const ObligationAxiom& object) { Enter(object); Walk(object); }
void LogicListener::Visit(const FulfillmentAxiom& object) { Enter(object); Walk(object); }
void LogicListener::Visit(const NonSeparatingImplication& object) { Enter(object); Walk(object); }
void LogicListener::Visit(const ImplicationSet& object) { Enter(object); Walk(object); }
void LogicListener::Visit(const PastPredicate& object) { Enter(object); Walk(object); }
void LogicListener::Visit(const FuturePredicate& object) { Enter(object); Walk(object); }
void LogicListener::Visit(const Annotation& object) { Enter(object); Walk(object); }

void MutableLogicListener::Visit(SymbolicVariable& object) { Enter(object); Enter(object.Decl()); Walk(object); }
void MutableLogicListener::Visit(SymbolicBool& object) { Enter(object); Walk(object); }
void MutableLogicListener::Visit(SymbolicNull& object) { Enter(object); Walk(object); }
void MutableLogicListener::Visit(SymbolicMin& object) { Enter(object); Walk(object); }
void MutableLogicListener::Visit(SymbolicMax& object) { Enter(object); Walk(object); }
void MutableLogicListener::Visit(SeparatingConjunction& object) { Enter(object); Walk(object); }
void MutableLogicListener::Visit(LocalMemoryResource& object) { Enter(object); Walk(object); }
void MutableLogicListener::Visit(SharedMemoryCore& object) { Enter(object); Walk(object); }
void MutableLogicListener::Visit(EqualsToAxiom& object) { Enter(object); Enter(object.variable); Walk(object); }
void MutableLogicListener::Visit(StackAxiom& object) { Enter(object); Walk(object); }
void MutableLogicListener::Visit(InflowEmptinessAxiom& object) { Enter(object); Walk(object); }
void MutableLogicListener::Visit(InflowContainsValueAxiom& object) { Enter(object); Walk(object); }
void MutableLogicListener::Visit(InflowContainsRangeAxiom& object) { Enter(object); Walk(object); }
void MutableLogicListener::Visit(ObligationAxiom& object) { Enter(object); Walk(object); }
void MutableLogicListener::Visit(FulfillmentAxiom& object) { Enter(object); Walk(object); }
void MutableLogicListener::Visit(NonSeparatingImplication& object) { Enter(object); Walk(object); }
void MutableLogicListener::Visit(ImplicationSet& object) { Enter(object); Walk(object); }
void MutableLogicListener::Visit(PastPredicate& object) { Enter(object); Walk(object); }
void MutableLogicListener::Visit(FuturePredicate& object) { Enter(object); Walk(object); }
void MutableLogicListener::Visit(Annotation& object) { Enter(object); Walk(object); }

void LogicListener::Enter(const VariableDeclaration& /*object*/) { /* do nothing */ }
void LogicListener::Enter(const SymbolDeclaration& /*object*/) { /* do nothing */ }
void LogicListener::Enter(const SymbolicVariable& /*object*/) { /* do nothing */ }
void LogicListener::Enter(const SymbolicBool& /*object*/) { /* do nothing */ }
void LogicListener::Enter(const SymbolicNull& /*object*/) { /* do nothing */ }
void LogicListener::Enter(const SymbolicMin& /*object*/) { /* do nothing */ }
void LogicListener::Enter(const SymbolicMax& /*object*/) { /* do nothing */ }
void LogicListener::Enter(const SeparatingConjunction& /*object*/) { /* do nothing */ }
void LogicListener::Enter(const LocalMemoryResource& /*object*/) { /* do nothing */ }
void LogicListener::Enter(const SharedMemoryCore& /*object*/) { /* do nothing */ }
void LogicListener::Enter(const EqualsToAxiom& /*object*/) { /* do nothing */ }
void LogicListener::Enter(const StackAxiom& /*object*/) { /* do nothing */ }
void LogicListener::Enter(const InflowEmptinessAxiom& /*object*/) { /* do nothing */ }
void LogicListener::Enter(const InflowContainsValueAxiom& /*object*/) { /* do nothing */ }
void LogicListener::Enter(const InflowContainsRangeAxiom& /*object*/) { /* do nothing */ }
void LogicListener::Enter(const ObligationAxiom& /*object*/) { /* do nothing */ }
void LogicListener::Enter(const FulfillmentAxiom& /*object*/) { /* do nothing */ }
void LogicListener::Enter(const NonSeparatingImplication& /*object*/) { /* do nothing */ }
void LogicListener::Enter(const ImplicationSet& /*object*/) { /* do nothing */ }
void LogicListener::Enter(const PastPredicate& /*object*/) { /* do nothing */ }
void LogicListener::Enter(const FuturePredicate& /*object*/) { /* do nothing */ }
void LogicListener::Enter(const Annotation& /*object*/) { /* do nothing */ }

void MutableLogicListener::Enter(const VariableDeclaration& /*object*/) { /* do nothing */ }
void MutableLogicListener::Enter(const SymbolDeclaration& /*object*/) { /* do nothing */ }
void MutableLogicListener::Enter(SymbolicVariable& /*object*/) { /* do nothing */ }
void MutableLogicListener::Enter(SymbolicBool& /*object*/) { /* do nothing */ }
void MutableLogicListener::Enter(SymbolicNull& /*object*/) { /* do nothing */ }
void MutableLogicListener::Enter(SymbolicMin& /*object*/) { /* do nothing */ }
void MutableLogicListener::Enter(SymbolicMax& /*object*/) { /* do nothing */ }
void MutableLogicListener::Enter(SeparatingConjunction& /*object*/) { /* do nothing */ }
void MutableLogicListener::Enter(LocalMemoryResource& /*object*/) { /* do nothing */ }
void MutableLogicListener::Enter(SharedMemoryCore& /*object*/) { /* do nothing */ }
void MutableLogicListener::Enter(EqualsToAxiom& /*object*/) { /* do nothing */ }
void MutableLogicListener::Enter(StackAxiom& /*object*/) { /* do nothing */ }
void MutableLogicListener::Enter(InflowEmptinessAxiom& /*object*/) { /* do nothing */ }
void MutableLogicListener::Enter(InflowContainsValueAxiom& /*object*/) { /* do nothing */ }
void MutableLogicListener::Enter(InflowContainsRangeAxiom& /*object*/) { /* do nothing */ }
void MutableLogicListener::Enter(ObligationAxiom& /*object*/) { /* do nothing */ }
void MutableLogicListener::Enter(FulfillmentAxiom& /*object*/) { /* do nothing */ }
void MutableLogicListener::Enter(NonSeparatingImplication& /*object*/) { /* do nothing */ }
void MutableLogicListener::Enter(ImplicationSet& /*object*/) { /* do nothing */ }
void MutableLogicListener::Enter(PastPredicate& /*object*/) { /* do nothing */ }
void MutableLogicListener::Enter(FuturePredicate& /*object*/) { /* do nothing */ }
void MutableLogicListener::Enter(Annotation& /*object*/) { /* do nothing */ }
