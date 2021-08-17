#include "engine/util.hpp"

using namespace plankton;


bool plankton::UpdatesFlow(const HeapEffect& effect) {
    return effect.pre->flow->Decl() != effect.post->flow->Decl();
}

bool plankton::UpdatesField(const HeapEffect& effect, const std::string& field) {
    return effect.pre->fieldToValue.at(field)->Decl() != effect.post->fieldToValue.at(field)->Decl();
}

void plankton::AvoidEffectSymbols(SymbolFactory& factory, const HeapEffect& effect) {
    factory.Avoid(*effect.pre);
    factory.Avoid(*effect.post);
    factory.Avoid(*effect.context);
}

void plankton::AvoidEffectSymbols(SymbolFactory& factory, const std::deque<std::unique_ptr<HeapEffect>>& effects) {
    for (const auto& effect : effects) plankton::AvoidEffectSymbols(factory, *effect);
}

