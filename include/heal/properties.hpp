#pragma once
#ifndef HEAL_PROPERTIES
#define HEAL_PROPERTIES


#include <map>
#include <array>
#include <memory>
#include "cola/ast.hpp"
#include "logic.hpp"
#include "util.hpp"


namespace heal {

    template<std::size_t N = 1>
    struct NodeProperty {
        std::string name;
        std::unique_ptr<PointsToAxiom> resource;
        std::array<std::reference_wrapper<const SymbolicVariableDeclaration>, N> vars;
        std::unique_ptr<FlatSeparatingConjunction> blueprint;

        NodeProperty(std::string name_, std::unique_ptr<PointsToAxiom> node_,
                     std::array<std::reference_wrapper<const SymbolicVariableDeclaration>, N> vars_,
                     std::unique_ptr<FlatSeparatingConjunction> blueprint_)
                : name(std::move(name_)), resource(std::move(node_)), vars(std::move(vars_)),
                  blueprint(std::move(blueprint_)) {
            assert(!name.empty());
            assert(resource);
            assert(blueprint);
        }

        std::unique_ptr<FlatSeparatingConjunction> Instantiate(const PointsToAxiom& node, std::array<std::reference_wrapper<const SymbolicVariableDeclaration>, N> decls) const {
            // TODO: proper error handling instead of asserts
            struct ReplacementVisitor : public DefaultLogicNonConstListener {
                std::map<const SymbolicVariableDeclaration*, const SymbolicVariableDeclaration*> symbolMap;
                std::map<const SymbolicFlowDeclaration*, const SymbolicFlowDeclaration*> flowMap;

                void enter(SymbolicVariable& var) override {
                    auto find = symbolMap.find(&var.Decl());
                    if (find != symbolMap.end()) var.decl_storage = *find->second;
                }

                void handle(std::reference_wrapper<const SymbolicFlowDeclaration>& flow) {
                    auto find = flowMap.find(&flow.get());
                    if (find != flowMap.end()) flow = *find->second;
                }
                void enter(PointsToAxiom& axiom) override { handle(axiom.flow); }
                void enter(InflowContainsValueAxiom& axiom) override { handle(axiom.flow); }
                void enter(InflowContainsRangeAxiom& axiom) override { handle(axiom.flow); }
                void enter(InflowEmptinessAxiom& axiom) override { handle(axiom.flow); }
            } visitor;

            // populate resource
            assert(cola::assignable(resource->node->Type(), node.node->Type()));
            assert(resource->fieldToValue.size() == node.fieldToValue.size());
            visitor.flowMap[&resource->flow.get()] = &node.flow.get();
            for (const auto& [name, var] : resource->fieldToValue) {
                assert(node.fieldToValue.count(name) > 0);
                visitor.symbolMap[&var->Decl()] = &node.fieldToValue.at(name)->Decl();
            }

            // populate vars
            for (std::size_t index = 0; index < vars.size(); ++index) {
                assert(cola::assignable(vars.at(index).get().type, decls.at(index).get().type));
                visitor.symbolMap[&vars.at(index).get()] = &decls.at(index).get();
            }

            // create instantiation
            auto result = heal::Copy(*blueprint);
            result->accept(visitor);
            return result;
        }

        std::unique_ptr<SeparatingImplication> InstantiatePure(const PointsToAxiom& node, std::array<std::reference_wrapper<const SymbolicVariableDeclaration>, N> decls) const {
            auto conclusion = Instantiate(node, std::move(decls));
            auto premise = std::make_unique<FlatSeparatingConjunction>();
            premise->conjuncts.push_back(heal::Copy(node));
            return std::make_unique<SeparatingImplication>(std::move(premise), std::move(conclusion));
        }

        template<typename... Targs>
        std::unique_ptr<FlatSeparatingConjunction> Instantiate(const PointsToAxiom& node, const Targs&... args) const {
            std::array<std::reference_wrapper<const SymbolicVariableDeclaration>, N> decls { args... };
            return Instantiate(node, std::move(decls));
        }
        template<typename... Targs>
        std::unique_ptr<SeparatingImplication> InstantiatePure(const PointsToAxiom& node, const Targs&... args) const {
            std::array<std::reference_wrapper<const SymbolicVariableDeclaration>, N> decls { args... };
            return InstantiatePure(node, std::move(decls));
        }
    };

	using Invariant = NodeProperty<0>;
	using Predicate = NodeProperty<1>;

} // namespace heal

#endif