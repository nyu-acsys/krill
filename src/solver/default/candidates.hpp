#ifndef CANDIDATES_HPP
#define CANDIDATES_HPP

#include "cola/ast.hpp"
#include "heal/logic.hpp"
#include "flowgraph.hpp"

namespace solver {

class CandidateGenerator : public heal::DefaultLogicListener {
    public:
        enum FlowLevel { NONE=0, ONLY=1, FAST=2, FULL=3 };

        static std::deque<std::unique_ptr<heal::Axiom>> Generate(const heal::LogicObject& base, FlowLevel level=FULL) {
            CandidateGenerator generator(base, level);
            return std::move(generator.result);
        }

        static std::deque<std::unique_ptr<heal::Axiom>> Generate(const FlowGraph& graph, EMode mode, FlowLevel level=FULL) {
            CandidateGenerator generator(graph, mode, level);
            return std::move(generator.result);
        }

        void enter(const heal::SymbolicVariable& obj) override { symbols.insert(&obj.Decl()); }
        void enter(const heal::PointsToAxiom& obj) override { flows.insert(&obj.flow.get()); }
        void enter(const heal::InflowContainsValueAxiom& obj) override { flows.insert(&obj.flow.get()); }
        void enter(const heal::InflowContainsRangeAxiom& obj) override { flows.insert(&obj.flow.get()); }
        void enter(const heal::InflowEmptinessAxiom& obj) override { flows.insert(&obj.flow.get()); }

    private:
        std::set<const heal::SymbolicVariableDeclaration*> symbols;
        std::set<const heal::SymbolicFlowDeclaration*> flows;
        std::deque<std::unique_ptr<heal::Axiom>> result;

        explicit CandidateGenerator(const FlowGraph& graph, EMode mode, FlowLevel level) {
            graph.pre->accept(*this);
            for (const auto& node : graph.nodes) node.ToLogic(mode)->accept(*this); // TODO: avoid 'ToLogic' formula construction
            MakeCandidates(level);
        }

        explicit CandidateGenerator(const heal::LogicObject& base, FlowLevel level) {
            base.accept(*this);
            MakeCandidates(level);
        }

        inline void MakeCandidates(FlowLevel level) {
            auto forAllFlows = [this,&level](auto minLevel, auto func){
                if (level < minLevel) return;
                for (const auto* flow : flows) func(flow);
            };

            forAllFlows(ONLY, [this](auto flow){ AddUnaryFlowCandidates(*flow); });
            for (auto symbol = symbols.begin(); symbol != symbols.end(); ++symbol) {
                if (level != ONLY) AddUnarySymbolCandidates(**symbol);
                forAllFlows(ONLY, [this,symbol](auto flow){ AddBinaryFlowCandidates(*flow, **symbol); });
                for (auto other = std::next(symbol); other != symbols.end(); ++other) {
                    if (level != ONLY) AddBinarySymbolCandidates(**symbol, **other);
                    forAllFlows(FULL, [this,symbol,other](auto flow){
                        AddTernaryFlowCandidates(*flow, **symbol, **other);
                    });
                }
            }
        }

        inline static std::unique_ptr<heal::SymbolicVariable> MkVar(const heal::SymbolicVariableDeclaration& decl) {
            return std::make_unique<heal::SymbolicVariable>(decl);
        }

        inline static const std::vector<heal::SymbolicAxiom::Operator>& GetOps(cola::Sort sort) {
            static std::vector<heal::SymbolicAxiom::Operator> ptrOps = { heal::SymbolicAxiom::EQ, heal::SymbolicAxiom::NEQ };
            static std::vector<heal::SymbolicAxiom::Operator> dataOps = {heal::SymbolicAxiom::EQ, heal::SymbolicAxiom::NEQ,
                                                                         heal::SymbolicAxiom::LEQ, heal::SymbolicAxiom::LT,
                                                                         heal::SymbolicAxiom::GEQ, heal::SymbolicAxiom::GT};
            return sort != cola::Sort::DATA ? ptrOps : dataOps;
        }

        inline static std::vector<std::unique_ptr<heal::SymbolicExpression>> GetImmis(cola::Sort sort) {
            std::vector<std::unique_ptr<heal::SymbolicExpression>> result;
            result.reserve(2);
            switch (sort) {
                case cola::Sort::VOID: break;
                case cola::Sort::BOOL: result.push_back(std::make_unique<heal::SymbolicBool>(true)); result.push_back(std::make_unique<heal::SymbolicBool>(false)); break;
                case cola::Sort::DATA: result.push_back(std::make_unique<heal::SymbolicMin>()); result.push_back(std::make_unique<heal::SymbolicMax>()); break;
                case cola::Sort::PTR: result.push_back(std::make_unique<heal::SymbolicNull>()); break;
            }
            return result;
        }

        void AddUnarySymbolCandidates(const heal::SymbolicVariableDeclaration& symbol) {
            for (auto op : GetOps(symbol.type.sort)) {
                for (auto&& immi : GetImmis(symbol.type.sort)) {
                    result.push_back(std::make_unique<heal::SymbolicAxiom>(MkVar(symbol), op, std::move(immi)));
                }
            }
        }

        void AddBinarySymbolCandidates(const heal::SymbolicVariableDeclaration& symbol, const heal::SymbolicVariableDeclaration& other) {
            if (symbol.type.sort != other.type.sort) return;
            for (auto op : GetOps(symbol.type.sort)) {
                result.push_back(std::make_unique<heal::SymbolicAxiom>(MkVar(symbol), op, MkVar(other)));
            }
        }

        void AddUnaryFlowCandidates(const heal::SymbolicFlowDeclaration& flow) {
            for (auto value : {true, false}) {
                result.push_back(std::make_unique<heal::InflowEmptinessAxiom>(flow, value));
            }
        }

        void AddBinaryFlowCandidates(const heal::SymbolicFlowDeclaration& flow, const heal::SymbolicVariableDeclaration& symbol) {
            if (flow.type != symbol.type) return;
            result.push_back(std::make_unique<heal::InflowContainsValueAxiom>(flow, MkVar(symbol)));
        }

        void AddTernaryFlowCandidates(const heal::SymbolicFlowDeclaration& flow, const heal::SymbolicVariableDeclaration& symbol, const heal::SymbolicVariableDeclaration& other) {
            if (flow.type != symbol.type) return;
            if (flow.type != other.type) return;
            result.push_back(std::make_unique<heal::InflowContainsRangeAxiom>(flow, MkVar(symbol), MkVar(other)));
            result.push_back(std::make_unique<heal::InflowContainsRangeAxiom>(flow, MkVar(other), MkVar(symbol)));
        }
    };

}

#endif