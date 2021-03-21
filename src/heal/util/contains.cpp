#include "heal/util.hpp"

using namespace cola;
using namespace heal;


struct ContainsChecker : public DefaultLogicListener {
    const LogicObject& search;
    bool deepCheck;
    bool withinObligation = false;
    bool found = false;
    bool foundInsideObligation = false;

    ContainsChecker(const LogicObject& search, bool deepCheck) : search(search), deepCheck(deepCheck) {}

    void CheckObject(const LogicObject& object) {
        if (!deepCheck && found) return;
        bool objectIsSearch = heal::SyntacticallyEqual(search, object);
        found |= objectIsSearch;
        foundInsideObligation |= objectIsSearch && withinObligation;
    }

    void enter(const LogicVariable& object) override { CheckObject(object); }
    void enter(const SymbolicBool& object) override { CheckObject(object); }
    void enter(const SymbolicNull& object) override { CheckObject(object); }
    void enter(const SymbolicMin& object) override { CheckObject(object); }
    void enter(const SymbolicMax& object) override { CheckObject(object); }
    void enter(const FlatSeparatingConjunction& object) override { CheckObject(object); }
    void enter(const SeparatingConjunction& object) override { CheckObject(object); }
    void enter(const SeparatingImplication& object) override { CheckObject(object); }
    void enter(const NegatedAxiom& object) override { CheckObject(object); }
    void enter(const BoolAxiom& object) override { CheckObject(object); }
    void enter(const PointsToAxiom& object) override { CheckObject(object); }
    void enter(const StackAxiom& object) override { CheckObject(object); }
    void enter(const StackDisjunction& object) override { CheckObject(object); }
    void enter(const OwnershipAxiom& object) override { CheckObject(object); }
    void enter(const DataStructureLogicallyContainsAxiom& object) override { CheckObject(object); }
    void enter(const NodeLogicallyContainsAxiom& object) override { CheckObject(object); }
    void enter(const KeysetContainsAxiom& object) override { CheckObject(object); }
    void enter(const HasFlowAxiom& object) override { CheckObject(object); }
    void enter(const FlowContainsAxiom& object) override { CheckObject(object); }
    void enter(const UniqueInflowAxiom& object) override { CheckObject(object); }
    void enter(const FulfillmentAxiom& object) override { CheckObject(object); }
    void enter(const PastPredicate& object) override { CheckObject(object); }
    void enter(const FuturePredicate& object) override { CheckObject(object); }
    void enter(const Annotation& object) override { CheckObject(object); }

    void enter(const ObligationAxiom& object) override { withinObligation = true; CheckObject(object); }
    void exit(const ObligationAxiom&) override { withinObligation = false; }
};

bool heal::SyntacticallyContains(const LogicObject& object, const LogicObject& search, bool* insideObligation) {
    ContainsChecker checker(search, insideObligation != nullptr);
    object.accept(checker);
    if (insideObligation) *insideObligation = checker.foundInsideObligation;
	return checker.found;
}
