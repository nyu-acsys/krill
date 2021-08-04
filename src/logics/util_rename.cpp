#include "logics/util.hpp"

using namespace plankton;


struct SymbolRenamingListener : public MutableLogicListener {
    SymbolRenaming renaming;
    
    explicit SymbolRenamingListener(SymbolRenaming renaming) : renaming(renaming) {}
    
    void Visit(SymbolicVariable& object) override {
        object.decl = renaming(object.Decl());
    }
};

void plankton::RenameSymbolicSymbols(LogicObject& object, SymbolRenaming renaming) {
    SymbolRenamingListener listener(renaming);
    object.Accept(listener);
}

void plankton::RenameSymbolicSymbols(LogicObject& object, SymbolFactory& factory) {
    std::map<const SymbolDeclaration*, const SymbolDeclaration*> map;
    auto renaming = [&map,&factory](const SymbolDeclaration& symbol) -> const SymbolDeclaration& {
        auto find = map.find(&symbol);
        if (find != map.end()) return *find->second;
        auto& result = factory.GetFresh(symbol.type, symbol.order);
        map[&symbol] = &result;
        return result;
    };
    plankton::RenameSymbolicSymbols(object, renaming);
}

void plankton::RenameSymbolicSymbols(LogicObject& object, const LogicObject& avoidSymbolsFrom) {
    SymbolFactory factory(avoidSymbolsFrom);
    RenameSymbolicSymbols(object, factory);
}
