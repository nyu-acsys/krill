#include "eval.hpp"

#include <set>
#include "cola/util.hpp"
#include "heal/util.hpp" // TODO: delete

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
    void visit(SeparatingImplication&) override { /* do nothing */ }
    void visit(const SeparatingImplication&) override { /* do nothing */ }

    // ignore time predicates
    void visit(PastPredicate&) override { /* do nothing */ }
    void visit(const PastPredicate&) override { /* do nothing */ }
    void visit(FuturePredicate&) override { /* do nothing */ }
    void visit(const FuturePredicate&) override { /* do nothing */ }
};

//
// Variables
//

struct VariableResourceFinder : public BaseResourceFinder {
    const VariableDeclaration& search;
    EqualsToAxiom* resultNonConst = nullptr; // TODO: avoid this, instead do const_cast
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

struct EqualityFinder : public BaseResourceFinder {
    using DefaultLogicListener::enter; using DefaultLogicNonConstListener::enter;

    std::set<const VariableDeclaration*> search;
    explicit EqualityFinder(const VariableDeclaration& search) : search({ &search }) {}

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

    static std::set<const VariableDeclaration*> FindEqualities(const VariableDeclaration& decl, const LogicObject& obj) {
        EqualityFinder finder(decl);
        for (std::size_t oldSize = 0; oldSize != finder.search.size(); oldSize = finder.search.size()) {
            obj.accept(finder);
        }
        return std::move(finder.search);
    }
};

struct MemoryResourceFinder : public BaseResourceFinder {
    std::set<const VariableDeclaration*> search;
    PointsToAxiom* resultNonConst = nullptr;
    const PointsToAxiom* resultConst = nullptr;

    explicit MemoryResourceFinder(std::set<const VariableDeclaration*> search) : search(std::move(search)) {}

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
MemoryResourceFinder MakeMemoryResourceFinder(const VariableDeclaration& decl, T& object) {
    MemoryResourceFinder finder(EqualityFinder::FindEqualities(decl, object));
    object.accept(finder);
    assert(finder.resultConst == nullptr || &finder.resultConst->node->Type() == &decl.type);
    return finder;
}

template<typename T>
MemoryResourceFinder MakeMemoryResourceFinder(const SimpleSelector& dereference, T& object) {
    return MakeMemoryResourceFinder(*dereference.first, object);
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

heal::PointsToAxiom* solver::FindMemory(const heal::SymbolicVariableDeclaration& address, heal::LogicObject& object) {
    return MakeMemoryResourceFinder(address, object).resultNonConst;
}

const heal::PointsToAxiom* solver::FindMemory(const heal::SymbolicVariableDeclaration& address, const heal::LogicObject& object) {
    return MakeMemoryResourceFinder(address, object).resultConst;
}

struct SpecificationFinder : public BaseResourceFinder {
    std::deque<const heal::SpecificationAxiom*> result;
    std::set<const VariableDeclaration*> search;

    explicit SpecificationFinder(std::set<const VariableDeclaration*> search) : search(std::move(search)) {}

    inline bool Matches(const SpecificationAxiom& axiom) {
        return search.count(&axiom.key->Decl()) != 0;
    }

    using DefaultLogicListener::exit; using DefaultLogicNonConstListener::exit;
    void exit(const ObligationAxiom& obj) override { if (Matches(obj)) result.push_back(&obj); }
    void exit(const FulfillmentAxiom& obj) override { if (Matches(obj)) result.push_back(&obj); }
    void exit(ObligationAxiom& obj) override { if (Matches(obj)) result.push_back(&obj); }
    void exit(FulfillmentAxiom& obj) override { if (Matches(obj)) result.push_back(&obj); }
};

std::deque<const heal::SpecificationAxiom*> solver::FindSpecificationAxioms(const heal::SymbolicVariableDeclaration& key,
                                                                            const heal::LogicObject& object) {
    if (key.type.sort != Sort::DATA) return {};
    SpecificationFinder finder(EqualityFinder::FindEqualities(key, object));
    return std::move(finder.result);
}

//
// ValuationMap
//

template<bool LAZY>
const heal::PointsToAxiom* ValuationMap<LAZY>::GetMemoryResourceOrNull(const heal::SymbolicVariableDeclaration& variable) {
    if (LAZY) {
        auto find = addressToMemory.find(&variable);
        if (find != addressToMemory.end()) return find->second;
    }
    auto* result = solver::FindMemory(variable, context);
    if (LAZY) addressToMemory[&variable] = result;
    return result;
}

template<bool LAZY>
const heal::PointsToAxiom& ValuationMap<LAZY>::GetMemoryResourceOrFail(const heal::SymbolicVariableDeclaration& variable) {
    auto result = GetMemoryResourceOrNull(variable);
    if (result) return *result;
    throw std::logic_error("Unsafe operation: memory at '" + variable.name + "' is not accessible"); // TODO: better error class
}

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

template<bool LAZY>
std::unique_ptr<heal::SymbolicVariable> ValuationMap<LAZY>::Evaluate(const VariableExpression& expr) {
    return std::make_unique<SymbolicVariable>(GetValueOrFail(expr.decl));
}

template<bool LAZY>
std::unique_ptr<heal::SymbolicVariable> ValuationMap<LAZY>::Evaluate(const Dereference& expr) {
    return std::make_unique<SymbolicVariable>(GetValueOrFail(expr));
}

template<bool LAZY>
struct Evaluator : public BaseVisitor {
    ValuationMap<LAZY>& valueMap;
    std::unique_ptr<SymbolicExpression> result = nullptr;

    explicit Evaluator(ValuationMap<LAZY>& valueMap) : valueMap(valueMap) {}

    void visit(const BooleanValue& expr) override { result = std::make_unique<SymbolicBool>(expr.value); }
    void visit(const NullValue& /*expr*/) override { result = std::make_unique<SymbolicNull>(); }
    void visit(const MaxValue& /*expr*/) override { result = std::make_unique<SymbolicMax>(); }
    void visit(const MinValue& /*expr*/) override { result = std::make_unique<SymbolicMin>(); }

    void visit(const VariableExpression& expr) override { result = valueMap.Evaluate(expr); }
    void visit(const Dereference& expr) override { result = valueMap.Evaluate(expr); }

    void visit(const NegatedExpression& expr) override { throw std::logic_error("Cannot evaluate expression: '!' not supported"); } // TODO: better error class // TODO: print offending expression
    void visit(const EmptyValue& expr) override { throw std::logic_error("Cannot evaluate expression: 'EMPTY' not supported"); } // TODO: better error class // TODO: print offending expression
    void visit(const NDetValue& expr) override { throw std::logic_error("Cannot evaluate expression: '*' not supported"); } // TODO: better error class // TODO: print offending expression
};

template<bool LAZY>
std::unique_ptr<heal::SymbolicExpression> ValuationMap<LAZY>::Evaluate(const Expression& expr) {
    Evaluator evaluator(*this);
    expr.accept(evaluator);
    return std::move(evaluator.result);
}

template class solver::ValuationMap<true>;
template class solver::ValuationMap<false>;
