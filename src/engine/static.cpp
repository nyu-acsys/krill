#include "engine/static.hpp"

#include "programs/util.hpp"
#include "logics/util.hpp"
#include "engine/encoding.hpp"
#include "engine/util.hpp"
#include "util/shortcuts.hpp"

using namespace plankton;


std::ostream& plankton::operator<<(std::ostream& stream, const FutureSuggestion& object) {
    stream << "<< " << *object.command << " |  " << *object.guard << " >>";
    return stream;
}

//
// Dataflow
//

inline std::set<const Function*> CollectMacros(const Program& program) {
    std::set<const Function*> result;
    for (const auto& elem : program.macroFunctions) result.insert(elem.get());
    return result;
}

struct ReturnCollector : public ProgramListener {
    std::set<const Return*> returns;
    void Enter(const Return& object) override { returns.insert(&object); }
};

inline std::map<const Return*, const Function*> MakeReturnMap(const Program& program) {
    std::map<const Return*, const Function*> result;
    for (const auto& macro : program.macroFunctions) {
        ReturnCollector collector;
        macro->Accept(collector);
        for (const auto* cmd : collector.returns)
            result[cmd] = macro.get();
    }
    return result;
}

struct PointerCollector : public ProgramListener {
    std::set<const VariableDeclaration*> variables;
    void Enter(const VariableDeclaration& object) override {
        if (object.type.sort != Sort::PTR) return;
        variables.insert(&object);
    }
};

inline std::set<const VariableDeclaration*> CollectPointers(const AstNode& node) {
    PointerCollector collector;
    node.Accept(collector);
    return std::move(collector.variables);
}

struct AlwaysShared : public ProgramListener {
    std::map<const Return*, const Function*> returnMap;
    std::set<const VariableDeclaration*> sharedPointers;
    std::set<const Function*> sharedMacros;
    
    explicit AlwaysShared(const Program& program) : returnMap(MakeReturnMap(program)),
                                                    sharedPointers(CollectPointers(program)),
                                                    sharedMacros(CollectMacros(program)) {
    }
    
    [[nodiscard]] inline std::size_t Size() const {
        return sharedPointers.size() + sharedMacros.size();
    }
    
    [[nodiscard]] inline bool ContainsNonShared(const AstNode& object) const {
        auto variables = CollectPointers(object);
        return plankton::Any(variables, [this](const auto* var){
            return sharedPointers.count(var) == 0;
        });
    }
    
    template<typename L, typename R>
    void HandleAssignment(const Assignment<L,R>& object) {
        for (std::size_t index = 0; index < object.lhs.size(); ++index) {
            if (object.lhs.at(index)->GetSort() != Sort::PTR) continue;
            if (!ContainsNonShared(*object.rhs.at(index))) continue;
            sharedPointers.erase(&object.lhs.at(index)->Decl());
        }
    }
    void Enter(const VariableAssignment& object) override { HandleAssignment(object); }
    void Enter(const MemoryWrite& /*object*/) override { /* do nothing */ }
    void Enter(const Malloc& object) override {
        sharedPointers.erase(&object.lhs->Decl());
    }
    
    void Enter(const Return& object) override {
        auto function = returnMap[&object];
        if (!function) return;
        assert(function->kind == Function::MACRO);
        for (const auto& elem : object.expressions) {
            if (!ContainsNonShared(*elem)) continue;
            sharedMacros.erase(function);
        }
    }
    void Enter(const Macro& object) override {
        assert(object.function.get().kind == Function::MACRO);
        if (plankton::Membership(sharedMacros, &object.function.get())) return;
        for (const auto& elem : object.lhs) {
            sharedPointers.erase(&elem->Decl());
        }
    }
    
    void Visit(const Function& object) override {
        if (object.kind == Function::INIT) return;
        ProgramListener::Visit(object);
    }
};

inline std::set<const VariableDeclaration*> ComputeFixedPoint(const Program& program) {
    AlwaysShared dataFlow(program);
    while (true) {
        auto size = dataFlow.Size();
        program.Accept(dataFlow);
        if (size <= dataFlow.Size())
            return std::move(dataFlow.sharedPointers);
    }
}

DataFlowAnalysis::DataFlowAnalysis(const Program& program) : alwaysShared(ComputeFixedPoint(program)) {
}

bool DataFlowAnalysis::AlwaysPointsToShared(const VariableDeclaration& decl) const {
    return alwaysShared.count(&decl) != 0;
}


//
// Futures
//

inline bool AlwaysLocal(const VariableDeclaration& decl, const DataFlowAnalysis& dataFlow) {
    // TODO: this should be 'AlwaysPointsToLocal'
    return !dataFlow.AlwaysPointsToShared(decl);
}

inline std::unique_ptr<Annotation> MakeDummyState(const std::set<const VariableDeclaration*>& vars) {
    SymbolFactory factory;
    auto result = std::make_unique<SeparatingConjunction>();
    for (const auto* var : vars) {
        auto& value = factory.GetFreshFO(var->type);
        result->Conjoin(std::make_unique<EqualsToAxiom>(*var, value));
        result->Conjoin(plankton::MakeSharedMemory(value, Type::Data(), factory));
    }
    return std::make_unique<Annotation>(std::move(result));
}

inline bool LooksBounded(const Dereference& lhs, const SimpleExpression& rhs,
                         const std::vector<std::unique_ptr<BinaryExpression>>& assumptions,
                         const DataFlowAnalysis& dataFlow) {
    if (AlwaysLocal(lhs.variable->Decl(), dataFlow)) return true;

    struct : public ProgramListener {
        std::set<const VariableDeclaration*> vars;
        void Enter(const VariableDeclaration& object) override { vars.insert(&object); }
    } collector;
    lhs.Accept(collector);
    rhs.Accept(collector);
    for (const auto& expr : assumptions) expr->Accept(collector);

    auto state = MakeDummyState(collector.vars);
    for (const auto& expr : assumptions) {
        auto left = plankton::MakeSymbolic(*expr->lhs, *state->now);
        auto right = plankton::MakeSymbolic(*expr->rhs, *state->now);
        state->Conjoin(std::make_unique<StackAxiom>(expr->op, std::move(left), std::move(right)));
    }

    Encoding enc(*state->now);
    plankton::ExtendStack(*state, enc, ExtensionPolicy::POINTERS);
    plankton::InlineAndSimplify(*state);
    Encoding encoding(*state->now);

    auto rhsSym = encoding.Encode(*plankton::MakeSymbolic(rhs, *state->now));
    auto& lhsVal = plankton::Evaluate(lhs, *state->now);
    if (encoding.Implies(rhsSym == encoding.Encode(lhsVal))) return true;

    if (auto lhsNext = plankton::TryGetResource(lhsVal, *state->now)) {
        for (const auto&[field, value] : lhsNext->fieldToValue) {
            if (value->GetSort() != Sort::PTR) continue;
            if (encoding.Implies(encoding.Encode(*value) == rhsSym)) return true;
        }
    }

    if (auto rhsVar = dynamic_cast<const VariableExpression*>(&rhs)) {
        auto& rhsRes = plankton::GetResource(rhsVar->Decl(), *state->now);
        auto& rhsMem = plankton::GetResource(rhsRes.Value(), *state->now);
        for (const auto&[field, value] : rhsMem.fieldToValue) {
            if (value->GetSort() != Sort::PTR) continue;
            if (encoding.Implies(encoding.Encode(*value) == encoding.Encode(lhsVal))) return true;
        }
    }

    return false;
}

inline bool LooksBounded(const MemoryWrite& write, const std::vector<std::unique_ptr<BinaryExpression>>& assumptions,
                         const DataFlowAnalysis& dataFlow) {
    for (std::size_t index = 0; index < write.lhs.size(); ++index) {
        if (write.lhs.at(index)->GetSort() != Sort::PTR) continue;
        if (LooksBounded(*write.lhs.at(index), *write.rhs.at(index), assumptions, dataFlow)) continue;
        return false;
    }
    return true;
}


inline bool ContainsExpression(const BinaryExpression& expr, const Expression& search) {
    assert(dynamic_cast<const VariableExpression*>(&search) || dynamic_cast<const Dereference*>(&search));
    struct : public ProgramListener {
        std::deque<const Expression*> expressions;
        void Enter(const VariableExpression& object) override { expressions.push_back(&object); }
        void Enter(const Dereference& object) override { expressions.push_back(&object); }
    } collector;
    expr.Accept(collector);
    return plankton::Any(collector.expressions, [&search](const auto* expr){
        return plankton::SyntacticalEqual(search, *expr);
    });
}

inline bool ContainsShared(const Expression& expr, const DataFlowAnalysis& dataFlow) {
    struct SharedChecker: public ProgramListener {
        const DataFlowAnalysis& dataFlow;
        bool result = false;
        explicit SharedChecker(const DataFlowAnalysis& dataFlow) : dataFlow(dataFlow) {}
        void Enter(const VariableExpression& object) override { result |= !object.Decl().isShared; }
        void Enter(const Dereference& object) override {
            result |= !object.variable->Decl().isShared || AlwaysLocal(object.variable->Decl(), dataFlow);
        }
    } checker(dataFlow);
    expr.Accept(checker);
    return checker.result;
}

struct SuggestionFinder : public DefaultProgramVisitor {
    DataFlowAnalysis dataFlow;
    bool insideAtomic = false;
    std::vector<std::unique_ptr<BinaryExpression>> assumptions;
    std::deque<std::unique_ptr<FutureSuggestion>> suggestions;

    explicit SuggestionFinder(const Program& program) : dataFlow(program) {}

    void HandleSuggestion(const MemoryWrite& write) {
        if (LooksBounded(write, assumptions, dataFlow)) return;
        auto suggest = std::make_unique<FutureSuggestion>(plankton::Copy(write));
        for (const auto& cond : assumptions) suggest->guard->conjuncts.push_back(plankton::Copy(*cond));
        suggestions.push_back(std::move(suggest));
    }

    void RemoveOverride(const Expression& expr) {
        plankton::RemoveIf(assumptions, [&expr](const auto& elem){
            return ContainsExpression(*elem, expr);
        });
    }

    void AddAssumption(std::unique_ptr<BinaryExpression> expr) {
        bool keep = insideAtomic;
        keep |= !ContainsShared(*expr, dataFlow);
        auto deref = dynamic_cast<const Dereference*>(expr->lhs.get());
        keep |= deref != nullptr && AlwaysLocal(deref->variable->Decl(), dataFlow);
        if (!keep) return;
        assumptions.push_back(std::move(expr));
    }

    void Visit(const Assume& object) override { AddAssumption(plankton::Copy(*object.condition)); }
    void Visit(const Malloc& object) override { RemoveOverride(*object.lhs); }
    void Visit(const Macro& /*object*/) override { assumptions.clear(); }
    void Visit(const VariableAssignment& object) override {
        for (const auto& var : object.lhs) RemoveOverride(*var);
        for (std::size_t index = 0; index < object.lhs.size(); ++index) {
            AddAssumption(std::make_unique<BinaryExpression>(BinaryOperator::EQ, plankton::Copy(*object.lhs.at(index)),
                                                             plankton::Copy(*object.rhs.at(index))));
        }
    }
    void Visit(const MemoryWrite& object) override {
        HandleSuggestion(object);
        for (const auto& deref : object.lhs) RemoveOverride(*deref);
        for (std::size_t index = 0; index < object.lhs.size(); ++index) {
            AddAssumption(std::make_unique<BinaryExpression>(BinaryOperator::EQ, plankton::Copy(*object.lhs.at(index)),
                                                             plankton::Copy(*object.rhs.at(index))));
        }
    }
    void Visit(const Scope& object) override { object.body->Accept(*this); }
    void Visit(const Sequence& object) override { object.first->Accept(*this); object.second->Accept(*this); }
    void Visit(const Atomic& object) override {
        auto inAtomic = insideAtomic;
        insideAtomic = true;
        object.body->Accept(*this);
        insideAtomic = inAtomic;
        if (insideAtomic) return;
        plankton::RemoveIf(assumptions, [this](const auto& elem){ return ContainsShared(*elem, dataFlow); });
    }
    void Visit(const UnconditionalLoop& object) override {
        assumptions.clear();
        object.body->Accept(*this);
        assumptions.clear(); // trivial join
    }
    void Visit(const Choice& object) override {
        auto pre = std::move(assumptions);
        for (const auto& branch : object.branches) {
            assumptions.clear();
            for (const auto& elem : pre) assumptions.push_back(plankton::Copy(*elem));
            branch->Accept(*this);
        }
        assumptions.clear(); // trivial join
    }
};

[[nodiscard]] std::deque<std::unique_ptr<FutureSuggestion>> plankton::SuggestFutures(const Program& program) {
    SuggestionFinder finder(program);
    for (const auto& function : program.macroFunctions) function->body->Accept(finder);
    for (const auto& function : program.apiFunctions) function->body->Accept(finder);
    return std::move(finder.suggestions);
}
