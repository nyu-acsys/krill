#include "parser/builder.hpp"

#include "PlanktonBaseVisitor.h"
#include "logics/util.hpp"
#include "util/log.hpp"

using namespace plankton;

/* TODO: enforce the following properties
 *        - Predicates must not access shared variables
 *        - Predicates must not use pointer selectors
 *        - shared NodeInvariant must contain shared variables 'v' only in the form 'v == ...'
 *          (sep imp must be pure? what goes in must come out?)
 *        - local NodeInvariant must not contain shared variables
 *        - NodeInvariants must target pointer variable
 */



//
// Helpers
//

inline bool NoConfig(PlanktonParser::ProgramContext& context) {
    return context.ctns.empty() && context.outf.empty() && context.ninv.empty();
}

inline void CheckConfig(PlanktonParser::ProgramContext& context, bool prepared) {
    if (!prepared) throw std::logic_error("Internal error: 'AstBuilder::PrepareMake' must be called first."); // TODO: better error handling
    if (context.ctns.empty()) throw std::logic_error("Parse error: incomplete flow definition, contains predicate missing."); // TODO: better error handling
    if (context.outf.empty()) throw std::logic_error("Parse error: incomplete flow definition, outflow predicate missing."); // TODO: better error handling
}

inline const Type& GetType(const AstBuilder& builder, PlanktonParser::TypeContext& context) {
    auto typeName = builder.MakeBaseTypeName(context);
    return builder.TypeByName(typeName);
}

inline const Type& GetNodeType(const AstBuilder& builder, PlanktonParser::TypeContext& context) {
    auto& type = GetType(builder, context);
    if (type.sort == Sort::PTR) return type;
    throw std::logic_error("Parse error: expected pointer type in flow definition."); // TODO: better error handling
}

inline const Type& GetValueType(const AstBuilder& builder, PlanktonParser::TypeContext& context) {
    auto& type = GetType(builder, context);
    if (type == Type::Data()) return type;
    throw std::logic_error("Parse error: expected 'data_t' in flow definition."); // TODO: better error handling
}


//
// Storage units
//

using FieldMap = std::map<std::string, std::reference_wrapper<const SymbolDeclaration>>;

struct FlowConstructionInfo {
    SymbolFactory factory;
    std::unique_ptr<VariableDeclaration> nodeVar = nullptr;
    std::unique_ptr<EqualsToAxiom> node = nullptr;
    std::unique_ptr<MemoryAxiom> memory = nullptr;
    std::unique_ptr<VariableDeclaration> valueVar = nullptr;
    std::unique_ptr<EqualsToAxiom> value = nullptr;
    
    void AddPtr(const Type& type, const std::string& name) {
        assert(type.sort == Sort::PTR);
        nodeVar = std::make_unique<VariableDeclaration>(name, type, false);
        AddPtr(*nodeVar);
    }
    
    void AddPtr(const VariableDeclaration& decl) {
        assert(decl.type.sort == Sort::PTR);
        node = std::make_unique<EqualsToAxiom>(decl, factory.GetFreshFO(decl.type));
        memory = plankton::MakeSharedMemory(node->Value(), Type::Data(), factory);
    }
    
    void AddVal(const Type& type, const std::string& name) {
        valueVar = std::make_unique<VariableDeclaration>(name, type, false);
        value = std::make_unique<EqualsToAxiom>(*valueVar, factory.GetFreshFO(type));
    }
};

struct FlowStore {
    const SymbolDeclaration* node = nullptr;
    const SymbolDeclaration* nodeFlow = nullptr;
    const SymbolDeclaration* value = nullptr;
    FieldMap fields;
    std::unique_ptr<ImplicationSet> invariant = nullptr;
};

template<bool HasMem, bool HasVal>
std::unique_ptr<ImplicationSet> Instantiate(const FlowStore& store,
                                            const MemoryAxiom* memory, const SymbolDeclaration* value) {
    // get blueprint
    assert(store.invariant);
    auto blueprint = plankton::Copy(*store.invariant);
    
    // create variable map
    std::map<const SymbolDeclaration*, const SymbolDeclaration*> replacement;
    auto AddReplacement = [&replacement](auto& replace, auto& with) {
        [[maybe_unused]] auto insertion = replacement.emplace(&replace, &with);
        assert(insertion.second);
    };
    if constexpr (HasMem) {
        assert(memory);
        assert(store.node);
        assert(store.nodeFlow);
        AddReplacement(*store.node, memory->node->Decl());
        AddReplacement(*store.nodeFlow, memory->flow->Decl());
        for (const auto& [fieldName, fieldValue] : memory->fieldToValue)
            AddReplacement(store.fields.at(fieldName).get(), fieldValue->Decl());
    } else {
        assert(!memory);
        assert(!store.node);
    }
    if constexpr (HasVal) {
        assert(value);
        assert(store.value);
        AddReplacement(*store.value, *value);
    } else {
        assert(!value);
        assert(!store.value);
    }
    
    // rename blueprint
    auto renaming = [&replacement](const SymbolDeclaration& replace) -> const SymbolDeclaration& {
        auto find = replacement.find(&replace);
        if (find != replacement.end()) return *find->second;
        throw std::logic_error("Internal error: instantiation failed due to incomplete renaming.");
    };
    plankton::RenameSymbols(*blueprint, renaming);
    
    return blueprint;
}


//
// Parsing config components
//

FlowStore MakeFlowDef(AstBuilder& builder, FlowConstructionInfo info, PlanktonParser::InvariantContext& context) {
    FlowStore result;
    if (info.value) result.value = &info.value->Value();
    if (info.node) {
        result.node = &info.node->Value();
        assert(info.memory);
        result.nodeFlow = &info.memory->flow->Decl();
        for (const auto& [field, value] : info.memory->fieldToValue) result.fields.emplace(field, value->Decl());
    }

    builder.PushScope();
    if (info.nodeVar) builder.AddDecl(std::move(info.nodeVar));
    if (info.valueVar) builder.AddDecl(std::move(info.valueVar));

    auto evalContext = std::make_unique<SeparatingConjunction>();
    if (info.memory) evalContext->Conjoin(std::move(info.memory));
    if (info.node) evalContext->Conjoin(std::move(info.node));
    if (info.value) evalContext->Conjoin(std::move(info.value));

    result.invariant = builder.MakeInvariant(context, *evalContext);
    builder.PopScope();

    plankton::Simplify(*result.invariant);
    return result;
}


//
// Pre/post-processing config components
//

FlowStore MakeContains(AstBuilder& builder, PlanktonParser::ContainsPredicateContext& context) {
    FlowConstructionInfo info;
    info.AddPtr(GetNodeType(builder, *context.nodeType), context.nodeName->getText());
    info.AddVal(GetValueType(builder, *context.valueType), context.valueName->getText());
    return MakeFlowDef(builder, std::move(info), *context.invariant());
}

FlowStore MakeOutflow(AstBuilder& builder, PlanktonParser::OutflowPredicateContext& context) {
    FlowConstructionInfo info;
    info.AddPtr(GetNodeType(builder, *context.nodeType), context.nodeName->getText());
    info.AddVal(GetValueType(builder, *context.valueType), context.valueName->getText());
    return MakeFlowDef(builder, std::move(info), *context.invariant());
}

FlowStore MakeNodeInvariant(AstBuilder& builder, PlanktonParser::NodeInvariantContext& context) {
    FlowConstructionInfo info;
    info.AddPtr(GetNodeType(builder, *context.nodeType), context.nodeName->getText());
    return MakeFlowDef(builder, std::move(info), *context.invariant());
}

std::map<const VariableDeclaration*, FlowStore> ExtractVariableInvariants(const FlowStore& invariant) {
    assert(invariant.node);
    std::map<const VariableDeclaration*, FlowStore> result;
    
    auto variables = plankton::Collect<VariableDeclaration>(*invariant.invariant);
    auto containsOtherVariable = [&variables](const auto& obj, const auto& decl) {
        auto decls = plankton::Collect<VariableDeclaration>(*obj);
        return plankton::ContainsIf(decls, [&decl,&variables](auto& elem){
            return *elem != decl && plankton::Membership(variables, elem);
        });
    };
    
    std::set<const SymbolDeclaration*> forbidden;
    forbidden.insert(invariant.nodeFlow);
    for (const auto& pair : invariant.fields) forbidden.insert(&pair.second.get());
    forbidden.erase(invariant.node);
    auto containsForbidden = [&forbidden](const auto& obj) {
        auto symbols = plankton::Collect<SymbolDeclaration>(*obj);
        auto result = plankton::NonEmptyIntersection(forbidden, symbols);
        return result;
    };
    
    for (const auto* var : variables) {
        FlowStore store;
        store.value = invariant.node;
        store.invariant = plankton::Copy(*invariant.invariant);
    
        for (auto& imp: store.invariant->conjuncts) {
            // remove conclusions that refer to other variables or to the memory
            plankton::RemoveIf(imp->conclusion->conjuncts, [&](const auto& obj) {
                return containsForbidden(obj) || containsOtherVariable(obj, *var);
            });
            // remove variable valuation premise (its given when instantiated)
            plankton::RemoveIf(imp->premise->conjuncts, [&](const auto& obj) {
                if (auto resource = dynamic_cast<const EqualsToAxiom*>(obj.get())) {
                    return resource->Variable() == *var && resource->Value() == *store.value;
                }
                return false;
            });
        }
        // remove implications that refer to other variables or the memory
        plankton::RemoveIf(store.invariant->conjuncts, [&](const auto& obj) {
            return obj->conclusion->conjuncts.empty() || containsForbidden(obj) || containsOtherVariable(obj, *var);
        });
        
        if (store.invariant->conjuncts.empty()) continue;
        [[maybe_unused]] auto insertion = result.emplace(var, std::move(store));
        assert(insertion.second);
    }
    
    return result;
}


//
// Building config
//

struct ParsedSolverConfigImpl : public ParsedSolverConfig {
    std::map<const Type*, FlowStore> containsPred, sharedInv, localInv;
    std::map<std::pair<const Type*, std::string>, FlowStore> outflowPred;
    std::map<const VariableDeclaration*, FlowStore> variableInv;
    
    template<typename T>
    [[nodiscard]] inline const FlowStore* FindStore(const std::map<T, FlowStore>& map, const T& key) const {
        auto find = map.find(key);
        if (find != map.end()) return &find->second;
        return nullptr;
    }
    
    [[nodiscard]] std::unique_ptr<ImplicationSet> GetSharedNodeInvariant(const SharedMemoryCore& memory) const override {
        auto store = FindStore(sharedInv, &memory.node->GetType());
        if (!store) return std::make_unique<ImplicationSet>();
        return Instantiate<true, false>(*store, &memory, nullptr);
    }
    
    [[nodiscard]] std::unique_ptr<ImplicationSet> GetLocalNodeInvariant(const LocalMemoryResource& memory) const override {
        auto store = FindStore(localInv, &memory.node->GetType());
        if (!store) return std::make_unique<ImplicationSet>();
        return Instantiate<true, false>(*store, &memory, nullptr);
    }
    
    [[nodiscard]] std::unique_ptr<ImplicationSet> GetSharedVariableInvariant(const EqualsToAxiom& variable) const override {
        auto store = FindStore(variableInv, &variable.Variable());
        if (!store) return std::make_unique<ImplicationSet>();
        return Instantiate<false, true>(*store, nullptr, &variable.Value());
    }
    
    [[nodiscard]] std::unique_ptr<ImplicationSet> GetOutflowContains(const MemoryAxiom& memory, const std::string& fieldName,
                                                                     const SymbolDeclaration& value) const override {
        auto store = FindStore(outflowPred, std::make_pair(&memory.node->GetType(), fieldName));
        if (!store) throw std::logic_error("Internal error: cannot find outflow predicate");
        return Instantiate<true, true>(*store, &memory, &value);
    }
    
    [[nodiscard]] std::unique_ptr<ImplicationSet> GetLogicallyContains(const MemoryAxiom& memory,
                                                                const SymbolDeclaration& value) const override {
        auto store = FindStore(containsPred, &memory.node->GetType());
        if (!store) throw std::logic_error("Internal error: cannot find contains predicate");
        return Instantiate<true, true>(*store, &memory, &value);
    }
};

std::unique_ptr<ParsedSolverConfig> AstBuilder::MakeConfig(PlanktonParser::ProgramContext& context) {
    if (NoConfig(context)) return nullptr;
    CheckConfig(context, prepared);
    auto result = std::make_unique<ParsedSolverConfigImpl>();

    for (auto* containsContext : context.ctns) {
        auto store = MakeContains(*this, *containsContext);
        assert(store.node);
        auto& type = store.node->type;
        auto insertion = result->containsPred.emplace(&type, std::move(store));
        if (insertion.second) continue;
        throw std::logic_error("Parse error: duplicate contains predicate definition for type '" + type.name + "'."); // TODO: better error handling
    }

    for (auto* outflowContext : context.outf) {
        auto store = MakeOutflow(*this, *outflowContext);
        assert(store.node);
        auto& type = store.node->type;
        auto field = outflowContext->field->getText();
        auto insertion = result->outflowPred.emplace(std::make_pair(&type, field), std::move(store));
        if (insertion.second) continue;
        throw std::logic_error("Parse error: duplicate outflow definition for field '" + field + "' of type '" + type.name + "'."); // TODO: better error handling
    }

    for (auto* invariantContext : context.ninv) {
        assert(invariantContext->isShared != nullptr ^ invariantContext->isLocal != nullptr);
        bool shared = invariantContext->isShared;

        auto store = MakeNodeInvariant(*this, *invariantContext);
        if (shared) result->variableInv = ExtractVariableInvariants(std::as_const(store));

        assert(store.node);
        auto& type = store.node->type;
        auto& target = shared ? result->sharedInv : result->localInv;
        auto insertion = target.emplace(&type, std::move(store));
        if (!insertion.second)
            throw std::logic_error("Parse error: duplicate invariant definition for for type '" + type.name + "'."); // TODO: better error handling
    }
    for (const auto& type : _types) {
        if (result->sharedInv.count(type.get()) == 0)
            WARNING("no shared invariant for type " << type->name << "." << std::endl)
        if (result->localInv.count(type.get()) == 0)
            WARNING("no local invariant for type " << type->name << "." << std::endl)
    }
    assert(!_variables.empty());
    for (const auto& var : _variables.front()) {
        if (var->type.sort != Sort::PTR) continue;
        if (result->variableInv.count(var.get()) != 0) continue;
        WARNING("no invariant for '" << var->name << "'." << std::endl)
    }

    return result;
}

std::unique_ptr<ParsedSolverConfig> AstBuilder::MakeConfig(PlanktonParser::FlowgraphsContext& context) {
    auto result = std::make_unique<ParsedSolverConfigImpl>();

    for (auto* outflowContext : context.outf) {
        auto store = MakeOutflow(*this, *outflowContext);
        assert(store.node);
        auto& type = store.node->type;
        auto field = outflowContext->field->getText();
        auto insertion = result->outflowPred.emplace(std::make_pair(&type, field), std::move(store));
        if (insertion.second) continue;
        throw std::logic_error("Parse error: duplicate outflow definition for field '" + field + "' of type '" + type.name + "'."); // TODO: better error handling
    }

    return result;
}
