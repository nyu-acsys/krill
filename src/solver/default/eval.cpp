#include "eval.hpp"

#include <set>
#include "cola/util.hpp"

using namespace cola;
using namespace heal;
using namespace solver;

using SimpleSelector = std::pair<const VariableDeclaration*, std::string>;

SimpleSelector DereferenceToSelector(const Dereference& dereference) {
    if (auto var = dynamic_cast<const VariableExpression*>(dereference.expr.get())) {
        return { &var->decl, dereference.fieldname };
    }
    std::stringstream err;
    err << "Unsupported dereference: expected variable but got '";
    cola::print(*dereference.expr, err);
    err << "'.";
    throw std::logic_error(err.str()); // TODO: better error class
}

struct BaseResourceFinder : public DefaultLogicListener, DefaultLogicNonConstListener {
    using DefaultLogicListener::visit; using DefaultLogicNonConstListener::visit;

    // ignore disjunctions
    void visit(StackDisjunction& axiom) override { if(axiom.axioms.size() == 1) axiom.axioms.front()->accept(*this); }
    void visit(const StackDisjunction& axiom) override { if(axiom.axioms.size() == 1) axiom.axioms.front()->accept(*this); }

    // ignore separating implications
    void visit(SeparatingConjunction& object) override { /* do nothing */ }
    void visit(const SeparatingConjunction& object) override { /* do nothing */ }

    // ignore time predicates
    void visit(PastPredicate& object) override { /* do nothing */ }
    void visit(const PastPredicate& object) override { /* do nothing */ }
    void visit(FuturePredicate& object) override { /* do nothing */ }
    void visit(const FuturePredicate& object) override { /* do nothing */ }
};

//
// Variables
//

struct VariableResourceFinder : public BaseResourceFinder {
    const VariableDeclaration& search;
    EqualsToAxiom* resultNonConst = nullptr;
    const EqualsToAxiom* resultConst = nullptr;

    explicit VariableResourceFinder(const VariableDeclaration& search) : search(search) {}

    // extract resource
    bool Matches(const EqualsToAxiom& axiom) {
        bool matches = &axiom.variable->decl == &search;
        if (matches && resultConst != nullptr)
            throw std::logic_error("Cautiously aborting: found multiple resources for variable '" + search.name +"'.");
        return matches;
    }
    using DefaultLogicListener::enter; using DefaultLogicNonConstListener::enter;
    void enter(const EqualsToAxiom& axiom) override { if (Matches(axiom)) resultConst = &axiom; }
    void enter(EqualsToAxiom& axiom) override {if (Matches(axiom)) { resultNonConst = &axiom; resultConst = &axiom; } }
};

template<typename T>
VariableResourceFinder MakeVariableResourceFinder(const VariableDeclaration& variable, T& object) {
    VariableResourceFinder finder(variable);
    object.accept(finder);
    return finder;
}

EqualsToAxiom* solver::FindResource(const VariableDeclaration& variable, LogicObject& object) {
    return MakeVariableResourceFinder(variable, object).resultNonConst;
}

const EqualsToAxiom* solver::FindResource(const VariableDeclaration& variable, const LogicObject& object) {
    return MakeVariableResourceFinder(variable, object).resultConst;
}

//
// Dereferences
//

struct MemoryResourceFinder : public BaseResourceFinder {
    std::set<const VariableDeclaration*> search;
    PointsToAxiom* resultNonConst = nullptr;
    const PointsToAxiom* resultConst = nullptr;

    explicit MemoryResourceFinder(const VariableDeclaration& search) : search({ &search }) {}

    // collect equalities among variables
    using DefaultLogicListener::enter; using DefaultLogicNonConstListener::enter;
    void Handle(const EqualsToAxiom& axiom) {
        if (search.count(&axiom.variable->decl) != 0) search.insert(&axiom.value->Decl());
        if (search.count(&axiom.value->Decl()) != 0) search.insert(&axiom.variable->decl);
    }
    void Handle(const SymbolicAxiom& axiom) {
        if (axiom.op != SymbolicAxiom::EQ) return;
        if (auto lhs = dynamic_cast<const SymbolicVariable*>(axiom.lhs.get())) {
            if (auto rhs = dynamic_cast<const SymbolicVariable*>(axiom.rhs.get())) {
                if (search.count(&lhs->Decl()) != 0) search.insert(&rhs->Decl());
                if (search.count(&rhs->Decl()) != 0) search.insert(&lhs->Decl());
            }
        }
    }
    void enter(EqualsToAxiom& axiom) override { Handle(axiom); }
    void enter(const EqualsToAxiom& axiom) override { Handle(axiom); }
    void enter(SymbolicAxiom& axiom) override { Handle(axiom); }
    void enter(const SymbolicAxiom& axiom) override { Handle(axiom); }

    // extract resource
    bool Matches(const PointsToAxiom& axiom) {
        bool matches = search.count(&axiom.node->Decl()) != 0;
        if (matches && resultConst != nullptr)
            throw std::logic_error("Cautiously aborting: found multiple resources for address '" + (*search.begin())->name +"'.");
        return matches;
    }
    using DefaultLogicListener::exit; using DefaultLogicNonConstListener::exit;
    void exit(const PointsToAxiom& axiom) override { if (Matches(axiom)) resultConst = &axiom; }
    void exit(PointsToAxiom& axiom) override {if (Matches(axiom)) { resultNonConst = &axiom; resultConst = &axiom; } }
};

template<typename T>
MemoryResourceFinder MakeMemoryResourceFinder(const SimpleSelector& dereference, T& object) {
    MemoryResourceFinder finder(*dereference.first);
    object.accept(finder);
    assert(finder.resultConst == nullptr || &finder.resultConst->node->Type() == &dereference.first->type);
    return finder;
}

PointsToAxiom* FindDereferenceResource(const SimpleSelector& dereference, LogicObject& object) {
    return MakeMemoryResourceFinder(dereference, object).resultNonConst;
}

const PointsToAxiom* FindDereferenceResource(const SimpleSelector& dereference, const LogicObject& object) {
    return MakeMemoryResourceFinder(dereference, object).resultConst;
}

PointsToAxiom* solver::FindResource(const cola::Dereference& dereference, LogicObject& object) {
    return FindDereferenceResource(DereferenceToSelector(dereference), object);
}

const PointsToAxiom* solver::FindResource(const cola::Dereference& dereference, const LogicObject& object) {
    return FindDereferenceResource(DereferenceToSelector(dereference), object);
}

//
// ValuationMap
//

template<bool LAZY>
const SymbolicVariableDeclaration* ValuationMap<LAZY>::GetValueOrNull(const VariableDeclaration &decl) {
    if (LAZY) {
        auto find = variableToSymbolic.find(&decl);
        if (find != variableToSymbolic.end()) return find->second;
    }
    const SymbolicVariableDeclaration *result = nullptr;
    auto resource = solver::FindResource(decl, context);
    if (resource) result = &resource->value->Decl();
    if (LAZY) variableToSymbolic[&decl] = result;
    return result;
}

template<bool LAZY>
const SymbolicVariableDeclaration& ValuationMap<LAZY>::GetValueOrFail(const VariableDeclaration &decl) {
    auto result = GetValueOrNull(decl);
    if (result) return *result;
    throw std::logic_error("Unsafe operation: variable '" + decl.name + "' is not accessible"); // TODO: better error class
}

template<bool LAZY>
const SymbolicVariableDeclaration* ValuationMap<LAZY>::GetValueOrNull(const Dereference& dereference) {
    auto selector = DereferenceToSelector(dereference);
    if (LAZY) {
        auto find = selectorToSymbolic.find(selector);
        if (find != selectorToSymbolic.end()) return find->second;
    }
    const SymbolicVariableDeclaration *result = nullptr;
    auto resource = FindDereferenceResource(selector, context);
    if (resource) result = &resource->fieldToValue.at(selector.second)->Decl();
    if (LAZY) selectorToSymbolic[selector] = result;
    return result;
}

template<bool LAZY>
const SymbolicVariableDeclaration& ValuationMap<LAZY>::GetValueOrFail(const Dereference& dereference) {
    auto result = GetValueOrNull(dereference);
    if (result) return *result;
    std::stringstream err;
    err << "Unsafe operation: dereference of '";
    cola::print(*dereference.expr, err);
    err << "'.";
    throw std::logic_error(err.str()); // TODO: better error class
}