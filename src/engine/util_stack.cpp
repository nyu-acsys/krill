#include "engine/util.hpp"

#include "logics/util.hpp"
#include "util/shortcuts.hpp"

using namespace plankton;


struct Generator {
    ExtensionPolicy policy;
    std::set<const SymbolDeclaration*> symbols;
    std::set<const SymbolDeclaration*> flows;
    std::deque<std::unique_ptr<Axiom>> result;
    
    explicit Generator(ExtensionPolicy policy) : policy(policy) {}
    
    inline void AddSymbolsFrom(const LogicObject& object) {
        auto newSymbols = plankton::Collect<SymbolicVariable>(object);
        for (const auto* elem : newSymbols) {
            switch (elem->Order()) {
                case Order::FIRST: symbols.insert(&elem->Decl()); break;
                case Order::SECOND: flows.insert(&elem->Decl()); break;
            }
        }
    }
    
    inline std::deque<std::unique_ptr<Axiom>> Generate() {
        result.clear();
        MakeCandidates();
        return std::move(result);
    }
    
private:
    inline void MakeCandidates() {
        // symbols
        auto isPtr = [](auto it){ return (**it).type.sort == Sort::PTR; };
        for (auto symbol = symbols.begin(); symbol != symbols.end(); ++symbol) {
            if (!isPtr(symbol) && !AtLeast(ExtensionPolicy::FAST)) continue;
            AddUnarySymbolCandidates(**symbol);
            for (auto other = std::next(symbol); other != symbols.end(); ++other) {
                AddBinarySymbolCandidates(**symbol, **other);
            }
        }
        
        // flows
        if (!AtLeast(ExtensionPolicy::FAST)) return;
        for (const auto* flow : flows) {
            AddUnaryFlowCandidates(*flow);
            for (auto symbol = symbols.begin(); symbol != symbols.end(); ++symbol) {
                AddBinaryFlowCandidates(*flow, **symbol);
                if (!AtLeast(ExtensionPolicy::FULL)) continue;
                for (auto other = std::next(symbol); other != symbols.end(); ++other) {
                    AddTernaryFlowCandidates(*flow, **symbol, **other);
                }
            }
        }
    }
    
    [[nodiscard]] inline bool AtLeast(ExtensionPolicy min) const {
        return min <= policy;
    }

    inline static std::unique_ptr<SymbolicVariable> MkVar(const SymbolDeclaration& decl) {
        return std::make_unique<SymbolicVariable>(decl);
    }

    inline static const std::vector<BinaryOperator>& GetOps(Sort sort) {
        static std::vector<BinaryOperator> ptrOps = { BinaryOperator::EQ, BinaryOperator::NEQ };
        static std::vector<BinaryOperator> dataOps = {BinaryOperator::EQ, BinaryOperator::NEQ,
                                                      BinaryOperator::LEQ, BinaryOperator::LT,
                                                      BinaryOperator::GEQ, BinaryOperator::GT};
        return sort == Sort::DATA ? dataOps : ptrOps;
    }

    inline static std::vector<std::unique_ptr<SymbolicExpression>> GetImmis(const Type& type) {
        std::vector<std::unique_ptr<SymbolicExpression>> result;
        result.reserve(2);
        switch (type.sort) {
            case Sort::VOID: break;
            case Sort::BOOL: result.push_back(std::make_unique<SymbolicBool>(true)); result.push_back(std::make_unique<SymbolicBool>(false)); break;
            case Sort::DATA: result.push_back(std::make_unique<SymbolicMin>()); result.push_back(std::make_unique<SymbolicMax>()); break;
            case Sort::PTR: result.push_back(std::make_unique<SymbolicNull>(type)); break;
        }
        return result;
    }

    inline void AddUnarySymbolCandidates(const SymbolDeclaration& symbol) {
        for (auto op : GetOps(symbol.type.sort)) {
            for (auto&& immi : GetImmis(symbol.type)) {
                result.push_back(std::make_unique<StackAxiom>(op, MkVar(symbol), std::move(immi)));
            }
        }
    }

    inline void AddBinarySymbolCandidates(const SymbolDeclaration& symbol, const SymbolDeclaration& other) {
        if (symbol.type.sort != other.type.sort) return;
        for (auto op : GetOps(symbol.type.sort)) {
            result.push_back(std::make_unique<StackAxiom>(op, MkVar(symbol), MkVar(other)));
        }
    }

    inline void AddUnaryFlowCandidates(const SymbolDeclaration& flow) {
        for (auto value : {true, false}) {
            result.push_back(std::make_unique<InflowEmptinessAxiom>(flow, value));
        }
    }

    inline void AddBinaryFlowCandidates(const SymbolDeclaration& flow, const SymbolDeclaration& symbol) {
        if (flow.type != symbol.type) return;
        result.push_back(std::make_unique<InflowContainsValueAxiom>(flow, symbol));
        result.push_back(std::make_unique<InflowContainsRangeAxiom>(MkVar(flow), MkVar(symbol), std::make_unique<SymbolicMax>()));
        result.push_back(std::make_unique<InflowContainsRangeAxiom>(MkVar(flow), std::make_unique<SymbolicMin>(), MkVar(symbol)));
    }

    inline void AddTernaryFlowCandidates(const SymbolDeclaration& flow, const SymbolDeclaration& symbol, const SymbolDeclaration& other) {
        if (flow.type != symbol.type) return;
        if (flow.type != other.type) return;
        result.push_back(std::make_unique<InflowContainsRangeAxiom>(flow, symbol, other));
        result.push_back(std::make_unique<InflowContainsRangeAxiom>(flow, other, symbol));
    }
};


void plankton::ExtendStack(Annotation& annotation, Encoding& encoding, ExtensionPolicy policy) {
    Generator candidates(policy);
    candidates.AddSymbolsFrom(annotation);
    
    for (auto& candidate : candidates.Generate()) {
        encoding.AddCheck(encoding.Encode(*candidate), [&candidate,&annotation](bool holds){
            if (holds) annotation.Conjoin(std::move(candidate));
        });
    }
}

void plankton::ExtendStack(Annotation& annotation, ExtensionPolicy policy) {
    Encoding encoding;
    encoding.AddPremise(encoding.Encode(*annotation.now));
    plankton::ExtendStack(annotation, encoding, policy);
}
