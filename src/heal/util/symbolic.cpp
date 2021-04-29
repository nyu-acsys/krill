#include "heal/util.hpp"

using namespace cola;
using namespace heal;


bool heal::IsSymbolicSymbol(const VariableDeclaration& decl) {
    return heal::IsOfType<SymbolicVariableDeclaration>(decl).first || heal::IsOfType<SymbolicFlowDeclaration>(decl).first;
}

template<typename T>
const T* GetUnusedSymbol(const Type& type, const std::set<const cola::VariableDeclaration*>& inUse, const std::deque<std::unique_ptr<T>>& allSymbols) {
    auto find = std::find_if(allSymbols.begin(), allSymbols.end(), [&inUse,&type](const auto& decl){
        return &decl->type == &type && inUse.count(decl.get()) == 0;
    });
    if (find != allSymbols.end()) return find->get();
    return nullptr;
}


//    std::set<const cola::VariableDeclaration*> inUse;

SymbolicFactory::SymbolicFactory() = default;
SymbolicFactory::SymbolicFactory(const LogicObject& avoid) : inUse(CollectSymbolicSymbols(avoid)) {}

const SymbolicVariableDeclaration& SymbolicFactory::GetUnusedSymbolicVariable(const cola::Type& type) {
    auto result = GetUnusedSymbol(type, inUse, SymbolicPool::GetAllVariables());
    if (!result) result = &SymbolicPool::MakeFreshVariable(type);
    inUse.insert(result);
    return *result;
}

const SymbolicFlowDeclaration& SymbolicFactory::GetUnusedFlowVariable(const cola::Type& type) {
    auto result = GetUnusedSymbol(type, inUse, SymbolicPool::GetAllFlows());
    if (!result) result = &SymbolicPool::MakeFreshFlow(type);
    inUse.insert(result);
    return *result;
}

//
// Rename
//

struct SymbolRenaming {
    bool appliedRenaming = false;
    std::set<const VariableDeclaration*> avoid;
    std::map<const VariableDeclaration*, const SymbolicVariableDeclaration*> renameVar;
    std::map<const VariableDeclaration*, const SymbolicFlowDeclaration*> renameFlow;

    explicit SymbolRenaming(const LogicObject& avoidSymbolsFrom) : avoid(CollectSymbolicSymbols(avoidSymbolsFrom)) {}

    template<typename T, typename M, typename N>
    const T& GetRenaming(const T& decl, M& map, N mk) {
        auto find = map.find(&decl);
        if (find != map.end()) return *find->second;
        const T* newBinding;
        if (avoid.count(&decl) == 0) newBinding = &decl;
        else {
            newBinding = mk();
            appliedRenaming = true;
        }
        avoid.insert(newBinding);
        return *newBinding;
    }

    const SymbolicVariableDeclaration& operator()(const SymbolicVariableDeclaration& decl) {
        return GetRenaming(decl, renameVar, [&](){
            return GetUnusedSymbol(decl.type, avoid, SymbolicPool::GetAllVariables());
        });
    }
    const SymbolicFlowDeclaration& operator()(const SymbolicFlowDeclaration& decl) {
        return GetRenaming(decl, renameFlow, [&](){
            return GetUnusedSymbol(decl.type, avoid, SymbolicPool::GetAllFlows());
        });
    }
};

struct Renamer : public DefaultLogicNonConstListener {
    SymbolRenaming renaming;

    explicit Renamer(const LogicObject& avoidSymbolsFrom) : renaming(avoidSymbolsFrom) {}

    void enter(SymbolicVariable& object) override { object.decl_storage = renaming(object.decl_storage); }
    void enter(PointsToAxiom& object) override { object.flow = renaming(object.flow); }
    void enter(InflowContainsValueAxiom& object) override { object.flow = renaming(object.flow); }
    void enter(InflowContainsRangeAxiom& object) override { object.flow = renaming(object.flow); }
    void enter(InflowEmptinessAxiom& object) override { object.flow = renaming(object.flow); }
};

bool heal::RenameSymbolicSymbols(LogicObject& object, const LogicObject& avoidSymbolsFrom) {
    Renamer renamer(avoidSymbolsFrom);
    object.accept(renamer);
    return renamer.renaming.appliedRenaming;
}

//
// Collect
//

struct SymbolicVariableCollector : public DefaultLogicListener {
    std::set<const cola::VariableDeclaration*> result;

    void enter(const SymbolicVariable& object) override { result.insert(&object.decl_storage.get()); }
    void enter(const PointsToAxiom& object) override { result.insert(&object.flow.get()); }
    void enter(const InflowContainsValueAxiom& object) override { result.insert(&object.flow.get()); }
    void enter(const InflowContainsRangeAxiom& object) override { result.insert(&object.flow.get()); }
    void enter(const InflowEmptinessAxiom& object) override { result.insert(&object.flow.get()); }
};

std::set<const cola::VariableDeclaration*> heal::CollectSymbolicSymbols(const LogicObject& object) {
    SymbolicVariableCollector collector;
    object.accept(collector);
    return std::move(collector.result);
}