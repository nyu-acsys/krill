#include "engine/util.hpp"

#include "logics/util.hpp"
#include "engine/encoding.hpp"

using namespace plankton;


inline bool ExtendIfNeeded(SeparatingConjunction& formula, std::set<const SymbolDeclaration*> symbols,
                           const Type& flowType, SymbolFactory& factory, Encoding& encoding) {
    if (symbols.empty()) return false;

    // prune potentially null symbols
    symbols = encoding.ComputeNonNull(std::move(symbols));

    // create memory
    bool extended = false;
    for (const auto* symbol : symbols) {
        assert(symbol->type.sort == Sort::PTR);
        if (plankton::TryGetResource(*symbol, formula)) continue;
        formula.Conjoin(plankton::MakeSharedMemory(*symbol, flowType, factory));
        extended = true;
    }

    return extended;
}

void plankton::MakeMemoryAccessible(SeparatingConjunction& formula, std::set<const SymbolDeclaration*> symbols,
                                    const Type& flowType, SymbolFactory& factory, Encoding& encoding) {
    ExtendIfNeeded(formula, std::move(symbols), flowType, factory, encoding);
}

void plankton::MakeMemoryAccessible(Annotation& annotation, std::set<const SymbolDeclaration*> symbols, const SolverConfig& config) {
    if (symbols.empty()) return;
    SymbolFactory factory(annotation);
    Encoding encoding(*annotation.now, config);
    auto extended = ExtendIfNeeded(*annotation.now, std::move(symbols), config.GetFlowValueType(), factory, encoding);
    // if (extended) plankton::ExtendStack(annotation, config, ExtensionPolicy::FAST); // TODO: do this?
}
