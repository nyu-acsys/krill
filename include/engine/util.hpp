#pragma once
#ifndef PLANKTON_ENGINE_UTIL_HPP
#define PLANKTON_ENGINE_UTIL_HPP

#include "programs/ast.hpp"
#include "logics/ast.hpp"
#include "engine/solver.hpp"
#include "engine/encoding.hpp"

namespace plankton {
    // TODO: move functions to solver if they require a SolverConfig ?
    
    const EqualsToAxiom* TryGetResource(const VariableDeclaration& variable, const Formula& state);
    const EqualsToAxiom& GetResource(const VariableDeclaration& variable, const Formula& state);
    EqualsToAxiom& GetResource(const VariableDeclaration& variable, Formula& state);
    
    const MemoryAxiom* TryGetResource(const SymbolDeclaration& address, const Formula& state);
    const MemoryAxiom& GetResource(const SymbolDeclaration& address, const Formula& state);
    MemoryAxiom& GetResource(const SymbolDeclaration& address, Formula& state);
    
    const SymbolDeclaration& Evaluate(const VariableExpression& variable, const Formula& state);
    const SymbolDeclaration& Evaluate(const Dereference& dereference, const Formula& state);
    
    std::unique_ptr<SymbolicExpression> MakeSymbolic(const ValueExpression& expression, const Formula& context);
    
    bool UpdatesFlow(const HeapEffect& effect);
    bool UpdatesField(const HeapEffect& effect, const std::string& field);
    
    void AvoidEffectSymbols(SymbolFactory& factory, const HeapEffect& effect);
    void AvoidEffectSymbols(SymbolFactory& factory, const std::deque<std::unique_ptr<HeapEffect>>& effects);
    
    void MakeMemoryAccessible(Annotation& annotation, std::set<const SymbolDeclaration*> addresses, const Type& flowType);
    
    enum struct ExtensionPolicy { FAST, FULL };
    void ExtendStack(Annotation& annotation, ExtensionPolicy policy);
    void ExtendStack(Annotation& annotation, Encoding& encoding, ExtensionPolicy policy);
    
    
//    bool JoinCompatibleResources(const std::vector<const Annotation*>& annotations);
//    inline bool JoinCompatibleResources() {
//        // abort if resource counts mismatch
//        if (!CompareResourceCount<LocalMemoryResource>()) return false;
//        if (!CompareResourceCount<SharedMemoryCore>()) return false;
//        if (!CompareResourceCount<FulfillmentAxiom>()) return false;
//        if (!CompareResourceCount<MemoryAxiom, std::less_equal<>, EqualsToAxiom>()) return false;
//
//        // check same variables in scope
//        //        auto variablesPremise = plankton::Collect<VariableDeclaration>(*premise.now);
//        //        auto variablesConclusion = plankton::Collect<VariableDeclaration>(*conclusion->now);
//        //        if (variablesPremise.size() != variablesConclusion.size()) return false;
//        //        for (const auto* resource : plankton::Collect<VariableDeclaration>(*premise.now)) {
//        //            plankton::Fin
//        //        }
//
//        // check obligation resources per specification type
//        std::map<Specification, int> obligationCount;
//        for (const auto* obligation : plankton::Collect<ObligationAxiom>(*premise.now)) {
//            obligationCount[obligation->spec] += 1;
//        }
//        for (const auto* obligation : plankton::Collect<ObligationAxiom>(*conclusion->now)) {
//            obligationCount[obligation->spec] -= 1;
//        }
//        return plankton::All(obligationCount, [](const auto& entry){ return entry.second == 0; });
//    }
//template<typename T, typename F = std::equal_to<>, typename U = T>
//        inline bool CompareResourceCount() {
//            auto premiseCollection = plankton::Collect<T>(*premise.now);
//            auto conclusionCollection = plankton::Collect<U>(*conclusion->now);
//            return F()(premiseCollection.size(), conclusionCollection.size());
//        }
    
//    using MemoryMap = std::map<const VariableDeclaration*, const MemoryAxiom*>;
//    MemoryMap MakeMemoryMap(const LogicObject& object);
    
}

#endif //PLANKTON_ENGINE_UTIL_HPP
