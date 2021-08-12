#include "engine/solver.hpp"

#include "logics/util.hpp"
#include "engine/util.hpp"
#include "engine/encoding.hpp"

using namespace plankton;


void plankton::MakeMemoryAccessible(SeparatingConjunction& formula, std::set<const SymbolDeclaration*> symbols,
                                    const Type& flowType, SymbolFactory& factory, Encoding& encoding) {
    if (symbols.empty()) return;
    
    // prune potentially null symbols
    symbols = encoding.ComputeNonNull(std::move(symbols));
    
    // create memory
    for (const auto* symbol : symbols) {
        assert(symbol->type.sort == Sort::PTR);
        if (plankton::TryGetResource(*symbol, formula)) continue;
        formula.Conjoin(plankton::MakeSharedMemory(*symbol, flowType, factory));
    }
}

void plankton::MakeMemoryAccessible(Annotation& annotation, std::set<const SymbolDeclaration*> symbols,
                                    const Type& flowType) {
    if (symbols.empty()) return;
    SymbolFactory factory(annotation);
    Encoding encoding(*annotation.now);
    return plankton::MakeMemoryAccessible(*annotation.now, std::move(symbols), flowType, factory, encoding);
}
