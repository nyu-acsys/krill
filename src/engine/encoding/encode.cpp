#include "engine/encoding.hpp"

#include "logics/util.hpp"
#include "engine/util.hpp"

using namespace plankton;


static constexpr int NULL_VALUE = 0;
static constexpr int MIN_VALUE = -65536;
static constexpr int MAX_VALUE = 65536;

EExpr Encoding::Min() { return EExpr(context.int_val(MIN_VALUE)); }
EExpr Encoding::Max() { return EExpr(context.int_val(MAX_VALUE)); }
EExpr Encoding::Null() { return EExpr(context.int_val(NULL_VALUE)); }
EExpr Encoding::Bool(bool val) { return EExpr(context.bool_val(val)); }

z3::expr Encoding::Replace(const EExpr& expression, const EExpr& replace, const EExpr& with) {
    z3::expr_vector replaceVec(context), withVec(context);
    replaceVec.push_back(replace.AsExpr());
    withVec.push_back(with.AsExpr());
    return expression.AsExpr().substitute(replaceVec, withVec);
}

z3::expr_vector Encoding::AsVector(const std::vector<EExpr>& vector) {
    z3::expr_vector result(context);
    for (const auto& elem : vector) result.push_back(elem.AsExpr());
    return result;
}

EExpr Encoding::MakeDistinct(const std::vector<EExpr>& vec) { return EExpr(z3::distinct(AsVector(vec))); }
EExpr Encoding::MakeAnd(const std::vector<EExpr>& vec) { return EExpr(z3::mk_and(AsVector(vec))); }
EExpr Encoding::MakeOr(const std::vector<EExpr>& vec) { return EExpr(z3::mk_or(AsVector(vec))); }
EExpr Encoding::MakeAtMost(const std::vector<EExpr>& vec, unsigned int count) {
    return EExpr(z3::atmost(AsVector(vec), count));
}


z3::sort Encoding::EncodeSort(Sort sort) {
    switch (sort) {
        case Sort::BOOL: return context.bool_sort();
        default: return context.int_sort();
    }
}

z3::expr Encoding::MakeQuantifiedVariable(Sort sort) {
    return context.constant("__qv", EncodeSort(sort));
}


template<typename K, typename V, typename F>
inline V GetOrCreate(std::map<K, V>& map, K key, F create) {
    auto find = map.find(key);
    if (find != map.end()) return find->second;
    auto newValue = create();
    map.emplace(key, newValue);
    return newValue;
}

EExpr Encoding::Encode(const VariableDeclaration& decl) {
    return GetOrCreate(variableEncoding, &decl, [this,&decl](){
        auto name = "__" + decl.name;
        auto expr = context.constant(name.c_str(), EncodeSort(decl.type.sort));
        return EExpr(expr);
    });
}

EExpr Encoding::Encode(const SymbolDeclaration& decl) {
    switch (decl.order) {
        case Order::FIRST:
            return GetOrCreate(symbolEncoding, &decl, [this, &decl]() {
                // create symbol
                auto name = "_v" + decl.name;
                auto expr = context.constant(name.c_str(), EncodeSort(decl.type.sort));
                // add implicit bounds on first order data values
                if (decl.type.sort == Sort::DATA) {
                    solver.add(Min().AsExpr() <= expr);
                    solver.add(expr <= Max().AsExpr());
                }
                // add implicit bounds on second order data values
                return EExpr(expr);
            });
    
        case Order::SECOND:
            return GetOrCreate(symbolEncoding, &decl, [this, &decl]() {
                // create symbol
                auto name = "_V" + decl.name;
                auto expr = context.function(name.c_str(), EncodeSort(decl.type.sort), context.bool_sort());
                // add implicit bounds on data values
                assert(decl.type.sort == Sort::DATA);
                auto qv = MakeQuantifiedVariable(decl.type.sort);
                solver.add(z3::forall(qv, z3::implies(qv < Min().AsExpr(), !expr(qv))));
                solver.add(z3::forall(qv, z3::implies(qv > Max().AsExpr(), !expr(qv))));
                return EExpr(expr);
            });
    }
}

EExpr Encoding::EncodeExists(Sort sort, const std::function<EExpr(EExpr)>& makeInner) {
    auto qv = MakeQuantifiedVariable(sort);
    auto inner = makeInner(EExpr(qv));
    return EExpr(z3::exists(qv, inner.AsExpr()));
}

EExpr Encoding::EncodeForAll(Sort sort, const std::function<EExpr(EExpr)>& makeInner) {
    auto qv = MakeQuantifiedVariable(sort);
    auto inner = makeInner(EExpr(qv));
    return EExpr(z3::forall(qv, inner.AsExpr()));
}

EExpr Encoding::EncodeFlowContains(const SymbolDeclaration& flow, const EExpr& expr) {
    return Encode(flow)(expr);
}

EExpr Encoding::EncodeIsNonNull(const EExpr& expr) {
    return expr != Null();
}

EExpr Encoding::EncodeIsNonNull(const SymbolDeclaration& decl) {
    return Encode(decl) != Null();
}

EExpr Encoding::EncodeIsNonNull(const VariableDeclaration& decl) {
    return Encode(decl) != Null();
}

EExpr Encoding::EncodeIsNull(const EExpr& expr) {
    return expr == Null();
}

EExpr Encoding::EncodeIsNull(const SymbolDeclaration& decl) {
    return Encode(decl) == Null();
}

EExpr Encoding::EncodeIsNull(const VariableDeclaration& decl) {
    return Encode(decl) == Null();
}

EExpr Encoding::EncodeMemoryEquality(const MemoryAxiom& memory, const MemoryAxiom& other) {
    z3::expr_vector result(context);
    result.push_back(Encode(memory.node->Decl()).AsExpr() == Encode(other.node->Decl()).AsExpr());
    result.push_back(Encode(memory.flow->Decl()).AsExpr() == Encode(other.flow->Decl()).AsExpr());
    assert(memory.node->Type() == other.node->Type());
    for (const auto& [field, value] : memory.fieldToValue) {
        result.push_back(Encode(value->Decl()).AsExpr() == Encode(other.fieldToValue.at(field)->Decl()).AsExpr());
    }
    return EExpr(z3::mk_and(result));
}

struct FormulaEncoder : public BaseLogicVisitor {
    Encoding& encoding;
    explicit FormulaEncoder(Encoding& encoding) : encoding(encoding) {}
    std::optional<EExpr> result = std::nullopt;
    
    EExpr Encode(const LogicObject& object) {
        result = std::nullopt;
        object.Accept(*this);
        assert(result.has_value());
        return result.value();
    }
    
    void Visit(const LocalMemoryResource& object) override { result = encoding.EncodeIsNonNull(object.node->Decl()); }
    void Visit(const SharedMemoryCore& object) override { result = encoding.EncodeIsNonNull(object.node->Decl()); }
    void Visit(const ObligationAxiom& /*object*/) override { result = encoding.Bool(true); }
    void Visit(const FulfillmentAxiom& /*object*/) override { result = encoding.Bool(true); }
    
    void Visit(const SymbolicVariable& object) override { result = encoding.Encode(object); }
    void Visit(const SymbolicBool& object) override { result = encoding.Bool(object.value); }
    void Visit(const SymbolicNull& /*object*/) override { result = encoding.Null(); }
    void Visit(const SymbolicMin& /*object*/) override { result = encoding.Min(); }
    void Visit(const SymbolicMax& /*object*/) override { result = encoding.Max(); }
    void Visit(const SeparatingConjunction& object) override {
        std::vector<EExpr> conjuncts;
        conjuncts.reserve(object.conjuncts.size());
        for (const auto& elem : object.conjuncts) conjuncts.push_back(Encode(*elem));
        auto localMemory = plankton::Collect<LocalMemoryResource>(object);
        auto allMemory = plankton::Collect<MemoryAxiom>(object);
        for (const auto* local : localMemory) {
            for (const auto* other : allMemory) {
                if (local == other) continue;
                conjuncts.push_back(encoding.Encode(local->node->Decl()) != encoding.Encode(other->node->Decl()));
            }
        }
        result = encoding.MakeAnd(conjuncts);
    }
    void Visit(const EqualsToAxiom& object) override {
        result = encoding.Encode(object.Variable()) == encoding.Encode(object.Value());
    }
    void Visit(const StackAxiom& object) override {
        auto lhs = Encode(*object.lhs);
        auto rhs = Encode(*object.rhs);
        switch (object.op) {
            case BinaryOperator::EQ: result = lhs == rhs; break;
            case BinaryOperator::NEQ: result = lhs != rhs; break;
            case BinaryOperator::LEQ: result = lhs <= rhs; break;
            case BinaryOperator::LT: result = lhs < rhs; break;
            case BinaryOperator::GEQ: result = lhs >= rhs; break;
            case BinaryOperator::GT: result = lhs > rhs; break;
        }
    }
    void Visit(const InflowEmptinessAxiom& object) override {
        auto flow = encoding.Encode(object.flow->Decl());
        result = encoding.EncodeForAll(object.flow->Sort(), [flow](auto qv){
            return !flow(qv);
        });
        if (!object.isEmpty) result = !result.value();
    }
    void Visit(const InflowContainsValueAxiom& object) override {
        result = encoding.Encode(object.flow->Decl())(encoding.Encode(object.value->Decl()));
    }
    void Visit(const InflowContainsRangeAxiom& object) override {
        auto flow = encoding.Encode(object.flow->Decl());
        auto low = Encode(*object.valueLow);
        auto high = Encode(*object.valueHigh);
        result = encoding.EncodeForAll(object.flow->Sort(), [flow,low,high](auto qv){
            return ((low <= qv) && (qv <= high)) >> flow(qv);
        });
    }
    void Visit(const SeparatingImplication& object) override {
        result = Encode(*object.premise) >> Encode(*object.conclusion);
    }
    void Visit(const Invariant& object) override {
        auto conjuncts = plankton::MakeVector<EExpr>(object.conjuncts.size());
        for (const auto& conjunct : object.conjuncts) conjuncts.push_back(Encode(*conjunct));
        result = encoding.MakeAnd(conjuncts);
    }
};

EExpr Encoding::Encode(const LogicObject& object) {
    FormulaEncoder encoder(*this);
    return encoder.Encode(object);
}

EExpr Encoding::EncodeInvariants(const Formula& formula, const SolverConfig& config) {
    auto local = plankton::Collect<LocalMemoryResource>(formula);
    auto shared = plankton::Collect<SharedMemoryCore>(formula);
    auto variables = plankton::Collect<EqualsToAxiom>(formula);
    std::vector<EExpr> result;
    result.reserve(local.size() + shared.size() + variables.size());
    for (const auto* mem : local) result.push_back(Encode(*config.GetLocalNodeInvariant(*mem)));
    for (const auto* mem : shared) result.push_back(Encode(*config.GetSharedNodeInvariant(*mem)));
    for (const auto* var : variables)
        if (var->Variable().isShared) result.push_back(Encode(*config.GetSharedVariableInvariant(*var)));
    return MakeAnd(result);
}

EExpr Encoding::EncodeAcyclicity(const Formula& formula) {
    auto reachability = plankton::ComputeReachability(formula);
    std::vector<EExpr> result;
    result.reserve(reachability.container.size());
    for (const auto& [node, reach] : reachability.container) {
        std::vector<EExpr> distinct;
        distinct.reserve(reach.size() + 1);
        distinct.push_back(Encode(*node));
        for (const auto* other : reach) distinct.push_back(Encode(*other));
        result.push_back(MakeDistinct(distinct));
    }
    return MakeAnd(result);
}

EExpr Encoding::EncodeOwnership(const Formula& formula) {
    auto local = plankton::Collect<LocalMemoryResource>(formula);
    auto shared = plankton::Collect<SharedMemoryCore>(formula);
    auto variables = plankton::Collect<EqualsToAxiom>(formula);
    auto result = plankton::MakeVector<EExpr>(local.size() * (shared.size() + variables.size()));
    for (const auto* memory : local) {
        auto address = Encode(memory->node->Decl());
        for (const auto* variable : variables) {
            if (!variable->Variable().isShared) continue;
            result.push_back(address != Encode(variable->Variable()));
        }
        for (const auto* other : shared) {
            result.push_back(address != Encode(other->node->Decl()));
            for (const auto& pair : other->fieldToValue) {
                if (pair.second->Sort() != Sort::PTR) continue;
                result.push_back(address != Encode(pair.second->Decl()));
            }
        }
    }
    return MakeAnd(result);
}
