#include "engine/solver.hpp"

#include "logics/util.hpp"
#include "engine/util.hpp"
#include "engine/encoding.hpp"

using namespace plankton;


void plankton::MakeMemoryAccessible(Annotation& annotation, std::set<const SymbolDeclaration*> symbols, const Type& flowType) {
    // prune potentially null symbols
    symbols = Encoding(*annotation.now).ComputeNonNull(std::move(symbols));
    
    // create memory
    SymbolFactory factory(annotation);
    for (const auto* symbol : symbols) {
        assert(symbol->type.sort == Sort::PTR);
        if (plankton::TryGetResource(*symbol, *annotation.now)) continue;
        annotation.Conjoin(plankton::MakeSharedMemory(*symbol, flowType, factory));
    }
}
