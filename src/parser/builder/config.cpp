#include "parser/builder.hpp"

#include "logics/util.hpp"
#include "util/log.hpp"

using namespace plankton;


//
// Helpers
//

inline bool NoConfig(PlanktonParser::ProgramContext& context) {
    return context.ctns.empty() && context.outf.empty() && context.vinv.empty() && context.ninv.empty();
}

inline void CheckConfig(PlanktonParser::ProgramContext& context) {
    if (context.ctns.empty()) throw std::logic_error("Parse error: incomplete flow definition, contains predicate missing."); // TODO: better error handling
    if (context.outf.empty()) throw std::logic_error("Parse error: incomplete flow definition, outflow predicate missing."); // TODO: better error handling
    if (context.vinv.empty()) WARNING("no variable invariants found.")
    if (context.ninv.empty()) WARNING("no node invariants found.")
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
        node = std::make_unique<EqualsToAxiom>(*nodeVar, factory.GetFreshFO(type));
        std::map<std::string, std::reference_wrapper<const SymbolDeclaration>> fields;
        for (const auto& [fieldName, fieldType] : type.fields) {
            fields.emplace(fieldName, factory.GetFreshFO(fieldType));
        }
        memory = std::make_unique<SharedMemoryCore>(node->Value(), factory.GetFreshSO(Type::Data()), std::move(fields));
    }
    
    void AddVal(const Type& type, const std::string& name) {
        valueVar = std::make_unique<VariableDeclaration>(name, type, false);
        value = std::make_unique<EqualsToAxiom>(*valueVar, factory.GetFreshFO(type));
    }
    
    void AddVal(const VariableDeclaration& decl) {
        value = std::make_unique<EqualsToAxiom>(decl, factory.GetFreshFO(decl.type));
    }
};

struct FlowStore {
    const SymbolDeclaration* node = nullptr;
    const SymbolDeclaration* nodeFlow = nullptr;
    const SymbolDeclaration* value = nullptr;
    FieldMap fields;
    std::unique_ptr<Formula> predicate = nullptr;
    std::unique_ptr<Invariant> invariant = nullptr;
};

template<typename T, bool HasMem, bool HasVal>
std::unique_ptr<T> Instantiate(const FlowStore& store, const MemoryAxiom* memory, const SymbolDeclaration* value) {
    // get blueprint
    std::unique_ptr<T> blueprint;
    if constexpr (std::is_same_v<T, Formula>) {
        assert(store.predicate);
        blueprint = plankton::Copy(*store.predicate);
    } else if constexpr (std::is_same_v<T, Invariant>) {
        assert(store.invariant);
        blueprint = plankton::Copy(*store.invariant);
    } else {
        throw std::logic_error("Internal error: instantiation failed.");
    }
    
    // create variable map
    std::map<const SymbolDeclaration*, const SymbolDeclaration*> replacement;
    auto AddReplacement = [&replacement](auto& replace, auto& with) {
        auto insertion = replacement.emplace(&replace, &with);
        assert(insertion.second);
    };
    if constexpr (HasMem) {
        assert(memory);
        assert(store.node);
        assert(store.nodeFlow);
        AddReplacement(*store.node, memory->node->Decl());
        AddReplacement(*store.nodeFlow, memory->node->Decl());
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

template<bool HasMem, bool HasVal>
void MergeIntoInvariantStore(FlowStore& into, const FlowStore& absorb) {
    std::unique_ptr<MemoryAxiom> memory = nullptr;
    if constexpr (HasMem) {
        assert(into.node);
        memory = std::make_unique<SharedMemoryCore>(*into.node, *into.nodeFlow, into.fields);
    }
    const SymbolDeclaration* value = nullptr;
    if constexpr (HasVal) {
        assert(into.value);
        value = into.value;
    }
    
    assert(into.invariant);
    auto instance = Instantiate<Invariant, HasMem, HasVal>(absorb, memory.get(), value);
    plankton::MoveInto(std::move(instance->conjuncts), into.invariant->conjuncts);
}


//
// Parsing config components
//

FlowStore MakeFlowDef(AstBuilder& builder, FlowConstructionInfo info,
                      PlanktonParser::FormulaContext* predicateContext,
                      PlanktonParser::InvariantContext* invariantContext) {
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
    
    if (predicateContext) result.predicate = builder.MakeFormula(*predicateContext, *evalContext);
    if (invariantContext) result.invariant = builder.MakeInvariant(*invariantContext, *evalContext);
    builder.PopScope();
    
    return result;
}


//
// Pre/post-processing config components
//

FlowStore MakeContains(AstBuilder& builder, PlanktonParser::ContainsPredicateContext& context) {
    FlowConstructionInfo info;
    info.AddPtr(GetNodeType(builder, *context.nodeType), context.nodeName->getText());
    info.AddVal(GetValueType(builder, *context.valueType), context.valueName->getText());
    return MakeFlowDef(builder, std::move(info), context.formula(), nullptr);
}

FlowStore MakeOutflow(AstBuilder& builder, PlanktonParser::OutflowPredicateContext& context) {
    FlowConstructionInfo info;
    info.AddPtr(GetNodeType(builder, *context.nodeType), context.nodeName->getText());
    info.AddVal(GetValueType(builder, *context.valueType), context.valueName->getText());
    return MakeFlowDef(builder, std::move(info), context.formula(), nullptr);
}

FlowStore MakeNodeInvariant(AstBuilder& builder, PlanktonParser::NodeInvariantContext& context) {
    FlowConstructionInfo info;
    info.AddPtr(GetNodeType(builder, *context.nodeType), context.nodeName->getText());
    return MakeFlowDef(builder, std::move(info), nullptr, context.invariant());
}

FlowStore MakeVariableInvariant(AstBuilder& builder, PlanktonParser::VariableInvariantContext& context) {
    FlowConstructionInfo info;
    info.AddVal(builder.VariableByName(context.Identifier()->getText()));
    return MakeFlowDef(builder, std::move(info), nullptr, context.invariant());
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
    
    [[nodiscard]] std::unique_ptr<Invariant> GetSharedNodeInvariant(const SharedMemoryCore& memory) const override {
        auto store = FindStore(sharedInv, &memory.node->Type());
        if (!store) return std::make_unique<Invariant>();
        return Instantiate<Invariant, true, false>(*store, &memory, nullptr);
    }
    
    [[nodiscard]] std::unique_ptr<Invariant> GetLocalNodeInvariant(const LocalMemoryResource& memory) const override {
        auto store = FindStore(localInv, &memory.node->Type());
        if (!store) return std::make_unique<Invariant>();
        return Instantiate<Invariant, true, false>(*store, &memory, nullptr);
    }
    
    [[nodiscard]] std::unique_ptr<Invariant> GetSharedVariableInvariant(const EqualsToAxiom& variable) const override {
        auto store = FindStore(variableInv, &variable.Variable());
        if (!store) return std::make_unique<Invariant>();
        return Instantiate<Invariant, false, true>(*store, nullptr, &variable.Value());
    }
    
    [[nodiscard]] std::unique_ptr<Formula> GetOutflowContains(const MemoryAxiom& memory, const std::string& fieldName,
                                                              const SymbolDeclaration& value) const override {
        auto store = FindStore(outflowPred, std::make_pair(&memory.node->Type(), fieldName));
        if (!store) throw std::logic_error("Internal error: cannot find outflow predicate");
        return Instantiate<Formula, true, true>(*store, &memory, &value);
    }
    
    [[nodiscard]] std::unique_ptr<Formula> GetLogicallyContains(const MemoryAxiom& memory,
                                                                const SymbolDeclaration& value) const override {
        auto store = FindStore(containsPred, &memory.node->Type());
        if (!store) throw std::logic_error("Internal error: cannot find contains predicate");
        return Instantiate<Formula, true, true>(*store, &memory, &value);
    }
};

std::unique_ptr<ParsedSolverConfig> AstBuilder::MakeConfig(PlanktonParser::ProgramContext& context) {
    if (NoConfig(context)) return nullptr;
    CheckConfig(context);
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
        auto store = MakeNodeInvariant(*this, *invariantContext);
        assert(store.node);
        auto& type = store.node->type;
        assert(invariantContext->isShared != nullptr ^ invariantContext->isLocal != nullptr);
        auto& target = invariantContext->isShared ? result->sharedInv : result->localInv;
        auto find = target.find(&type);
        if (find == target.end()) target.emplace(&type, std::move(store));
        else MergeIntoInvariantStore<true, false>(find->second, store);
    }
    for (const auto& type : _types) {
        if (result->sharedInv.count(type.get()) == 0) WARNING("no shared invariant for type " << type->name << ".")
        if (result->localInv.count(type.get()) == 0) WARNING("no local invariant for type " << type->name << ".")
    }
    
    for (auto* invariantContext : context.vinv) {
        auto store = MakeVariableInvariant(*this, *invariantContext);
        assert(store.node || store.value);
        auto& var = VariableByName(invariantContext->Identifier()->getText());
        auto find = result->variableInv.find(&var);
        if (find == result->variableInv.end()) result->variableInv.emplace(&var, std::move(store));
        else MergeIntoInvariantStore<false, true>(find->second, store);
    }
    assert(!_variables.empty());
    for (const auto& var : _variables.front()) {
        if (result->variableInv.count(var.get()) != 0) continue;
        WARNING("no invariant for shared variable '" << var->name << "'.")
    }
    
    return result;
}
