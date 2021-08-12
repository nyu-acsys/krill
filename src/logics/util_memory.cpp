#include "logics/util.hpp"

using namespace plankton;


template<typename T>
inline std::unique_ptr<T> MakeMemory(const SymbolDeclaration& address, const Type& flowType, SymbolFactory& factory) {
    auto node = std::make_unique<SymbolicVariable>(address);
    auto flow = std::make_unique<SymbolicVariable>(factory.GetFreshSO(flowType));
    std::map<std::string, std::unique_ptr<SymbolicVariable>> fieldToValue;
    for (const auto&[field, type] : address.type.fields) {
        auto& value = factory.GetFreshFO(type);
        fieldToValue[field] = std::make_unique<SymbolicVariable>(value);
    }
    return std::make_unique<T>(std::move(node), std::move(flow), std::move(fieldToValue));
}

std::unique_ptr<SharedMemoryCore>
plankton::MakeSharedMemory(const SymbolDeclaration& address, const Type& flowType, SymbolFactory& factory) {
    return MakeMemory<SharedMemoryCore>(address, flowType, factory);
}

std::unique_ptr<LocalMemoryResource>
plankton::MakeLocalMemory(const SymbolDeclaration& address, const Type& flowType, SymbolFactory& factory) {
    return MakeMemory<LocalMemoryResource>(address, flowType, factory);
}

bool plankton::IsLocal(const MemoryAxiom& axiom) {
    struct : public BaseLogicVisitor {
        bool result = false;
        void Visit(const LocalMemoryResource& /*object*/) override { result = true; }
        void Visit(const SharedMemoryCore& /*object*/) override { result = false; }
    } visitor;
    axiom.Accept(visitor);
    return visitor.result;
}
