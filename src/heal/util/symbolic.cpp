#include "heal/util.hpp"

using namespace cola;
using namespace heal;


bool SymbolicVariableSetComparator::operator() (const std::reference_wrapper<const SymbolicVariableDeclaration>& var, const std::reference_wrapper<const SymbolicVariableDeclaration>& other) const {
    return &var.get() < &other.get();
}

bool heal::IsSymbolicVariable(const VariableDeclaration& decl) {
    return heal::IsOfType<SymbolicVariableDeclaration>(decl).first;
}

bool heal::IsSymbolicVariable(const LogicVariable& variable) {
    return heal::IsSymbolicVariable(variable.Decl());
}

const SymbolicVariableDeclaration& heal::MakeFreshSymbolicVariable(const Type& type) {
    return SymbolicVariablePool::MakeFresh(type);
}

const SymbolicVariableDeclaration& heal::GetUnusedSymbolicVariable(const Type& type, const SymbolicVariableSet& variablesInUse) {
    auto isUsed = [&variablesInUse](const auto* decl) -> bool {
        return variablesInUse.end() != std::find_if(variablesInUse.begin(), variablesInUse.end(), [&decl](const auto& other) { return &other.get() == decl; });
    };
    for (const auto& decl : SymbolicVariablePool::GetAll()) {
        if (decl->type == type && !isUsed(decl.get())) return *decl;
    }
    return MakeFreshSymbolicVariable(type);
}

const SymbolicVariableDeclaration& heal::GetUnusedSymbolicVariable(const Type& type, const LogicObject& object) {
    return heal::GetUnusedSymbolicVariable(type, heal::CollectSymbolicVariables(object));
}

template<typename T>
std::unique_ptr<T> ApplyRenaming(std::unique_ptr<T> object, const LogicObject& avoidSymbolicVariablesFrom, bool* changed) {
    auto variablesInUse = heal::CollectSymbolicVariables(avoidSymbolicVariablesFrom);
    auto variablesToRename = heal::CollectSymbolicVariables(*object);
    std::map<const VariableDeclaration*, const VariableDeclaration*> replacementMap;
    for (const VariableDeclaration& decl : variablesToRename) {
        auto& replacement = GetUnusedSymbolicVariable(decl.type, variablesInUse);
        replacementMap[&decl] = &replacement;
        variablesInUse.insert(std::ref(replacement));
    }
    auto transformer = [&replacementMap](const VariableDeclaration& decl){
        auto find = replacementMap.find(&decl);
        return find != replacementMap.end() ? find->second : nullptr;
    };
    return heal::Replace(std::move(object), transformer, changed);
}

std::unique_ptr<LogicObject> heal::RenameSymbolicVariables(std::unique_ptr<LogicObject> object, const LogicObject& avoidSymbolicVariablesFrom, bool* changed) {
    return ApplyRenaming(std::move(object), avoidSymbolicVariablesFrom, changed);
}
std::unique_ptr<SeparatingConjunction> heal::RenameSymbolicVariables(std::unique_ptr<SeparatingConjunction> formula, const LogicObject& avoidSymbolicVariablesFrom, bool* changed) {
    return ApplyRenaming(std::move(formula), avoidSymbolicVariablesFrom, changed);
}
std::unique_ptr<TimePredicate> heal::RenameSymbolicVariables(std::unique_ptr<TimePredicate> predicate, const LogicObject& avoidSymbolicVariablesFrom, bool* changed) {
    return ApplyRenaming(std::move(predicate), avoidSymbolicVariablesFrom, changed);
}
std::unique_ptr<Annotation> heal::RenameSymbolicVariables(std::unique_ptr<Annotation> annotation, const LogicObject& avoidSymbolicVariablesFrom, bool* changed) {
    return ApplyRenaming(std::move(annotation), avoidSymbolicVariablesFrom, changed);
}


//
// Collect
//

struct SymbolicVariableCollector : public DefaultLogicListener {
    SymbolicVariableSet result;

    void enter(const LogicVariable& node) override {
        auto [isSymbolic, var] = heal::IsOfType<SymbolicVariableDeclaration>(node.Decl());
        if (isSymbolic) result.insert(*var);
    }
};

SymbolicVariableSet heal::CollectSymbolicVariables(const LogicObject& object) {
    SymbolicVariableCollector collector;
    object.accept(collector);
    return std::move(collector.result);
}
