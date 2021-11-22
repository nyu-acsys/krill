#include "logics/util.hpp"

using namespace plankton;


SymbolRenaming plankton::MakeDefaultRenaming(SymbolFactory& factory) {
    std::map<const SymbolDeclaration*, const SymbolDeclaration*> map;
    auto renaming = [map=std::move(map),&factory](const SymbolDeclaration& symbol) mutable -> const SymbolDeclaration& {
        auto find = map.find(&symbol);
        if (find != map.end()) return *find->second;
        auto& result = factory.GetFresh(symbol.type, symbol.order);
        map[&symbol] = &result;
        return result;
    };
    return renaming;
}

SymbolRenaming plankton::MakeMemoryRenaming(const MemoryAxiom& replace, const MemoryAxiom& with) {
    assert(replace.node->Type() == with.node->Type());
    std::map<const SymbolDeclaration*, const SymbolDeclaration*> map;
    map[&replace.node->Decl()] = &with.node->Decl();
    map[&replace.flow->Decl()] = &with.flow->Decl();
    for (const auto& [name, value] : replace.fieldToValue) {
        map[&value->Decl()] = &with.fieldToValue.at(name)->Decl();
    }

    return [map=std::move(map)](const SymbolDeclaration& decl) -> const SymbolDeclaration& {
        auto find = map.find(&decl);
        if (find != map.end()) return *find->second;
        else return decl;
    };
}

struct SymbolRenamingListener : public MutableLogicListener {
    const SymbolRenaming& renaming;
    
    explicit SymbolRenamingListener(const SymbolRenaming& renaming) : renaming(renaming) {}
    
    void Enter(SymbolicVariable& object) override {
        object.decl = renaming(object.Decl());
    }
};

void plankton::RenameSymbols(LogicObject& object, const SymbolRenaming& renaming) {
    SymbolRenamingListener listener(renaming);
    object.Accept(listener);
}

void plankton::RenameSymbols(LogicObject& object, SymbolFactory& factory) {
    auto renaming = MakeDefaultRenaming(factory);
    plankton::RenameSymbols(object, renaming);
}

void plankton::RenameSymbols(LogicObject& object, const LogicObject& avoidSymbolsFrom) {
    SymbolFactory factory(avoidSymbolsFrom);
    RenameSymbols(object, factory);
}
