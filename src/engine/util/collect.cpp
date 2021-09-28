#include "engine/util.hpp"

using namespace plankton;


struct Collector : public LogicListener {
    void Visit(const StackAxiom&) override { /* do nothing */ }
    void Visit(const InflowEmptinessAxiom&) override { /* do nothing */ }
    void Visit(const InflowContainsValueAxiom&) override { /* do nothing */ }
    void Visit(const InflowContainsRangeAxiom&) override { /* do nothing */ }

    std::set<const SymbolDeclaration*> result;
    void Enter(const SymbolDeclaration& object) override { result.insert(&object); }
};

std::set<const SymbolDeclaration*> plankton::CollectUsefulSymbols(const LogicObject& object) {
    Collector collector;
    object.Accept(collector);
    return std::move(collector.result);
}
