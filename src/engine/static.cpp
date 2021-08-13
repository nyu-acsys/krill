#include "engine/static.hpp"

#include "util/shortcuts.hpp"

using namespace plankton;


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
            if (object.lhs.at(index)->Sort() != Sort::PTR) continue;
            if (ContainsNonShared(*object.rhs.at(index)))
                sharedPointers.erase(&object.lhs.at(index)->Decl());
        }
    }
    void Enter(const VariableAssignment& object) override { HandleAssignment(object); }
    void Enter(const MemoryRead& object) override { HandleAssignment(object); }
    void Enter(const MemoryWrite& /*object*/) override { /* do nothing */ }
    void Enter(const Malloc& object) override { sharedPointers.erase(&object.lhs->Decl()); }
    
    void Enter(const Return& object) override {
        auto function = returnMap[&object];
        if (!function) return;
        assert(function->kind == Function::MACRO);
        for (const auto& elem : object.expressions) {
            if (ContainsNonShared(*elem))
                sharedMacros.erase(function);
        }
    }
    void Enter(const Macro& object) override {
        assert(object.function.get().kind == Function::MACRO);
        if (sharedMacros.count(&object.function.get()) != 0) return;
        for (const auto& elem : object.lhs)
            sharedPointers.erase(&elem->Decl());
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

DataFlowAnalysis::DataFlowAnalysis(const Program& program) : alwaysShared(ComputeFixedPoint(program)) {}

bool DataFlowAnalysis::AlwaysPointsToShared(const VariableDeclaration& decl) const {
    return alwaysShared.count(&decl) != 0;
}
