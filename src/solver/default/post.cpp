#include "default_solver.hpp"

#include "cola/util.hpp"
#include "heal/util.hpp"
#include "eval.hpp"

using namespace cola;
using namespace heal;
using namespace solver;

//
// Assumptions
//

inline SymbolicAxiom::Operator TranslateOp(BinaryExpression::Operator op) {
    switch (op) {
        case BinaryExpression::Operator::EQ: return SymbolicAxiom::EQ;
        case BinaryExpression::Operator::NEQ: return SymbolicAxiom::NEQ;
        case BinaryExpression::Operator::LEQ: return SymbolicAxiom::LEQ;
        case BinaryExpression::Operator::LT: return SymbolicAxiom::LT;
        case BinaryExpression::Operator::GEQ: return SymbolicAxiom::GEQ;
        case BinaryExpression::Operator::GT: return SymbolicAxiom::GT;
        default: throw;
    }
}

struct RelationTranslator : public BaseVisitor {
    LazyValuationMap valueMap;
    std::unique_ptr<SymbolicExpression> result;

    explicit RelationTranslator(const Annotation& context) : valueMap(context) {}

    const SymbolicVariableDeclaration& GetValue(const VariableExpression& expr) { return valueMap.GetValueOrFail(expr.decl); }
    const SymbolicVariableDeclaration& GetValue(const Dereference& expr) { return valueMap.GetValueOrFail(expr); }

    void visit(const BooleanValue& expr) override { result = std::make_unique<SymbolicBool>(expr.value); }
    void visit(const NullValue& /*expr*/) override { result = std::make_unique<SymbolicNull>(); }
    void visit(const MaxValue& /*expr*/) override { result = std::make_unique<SymbolicMax>(); }
    void visit(const MinValue& /*expr*/) override { result = std::make_unique<SymbolicMin>(); }
    void visit(const VariableExpression& expr) override { result = std::make_unique<SymbolicVariable>(GetValue(expr)); }
    void visit(const Dereference& expr) override { result = std::make_unique<SymbolicVariable>(GetValue(expr)); }

    void visit(const NegatedExpression& expr) override { throw std::logic_error("Cannot translate expression into logic: '!' not supported"); }
    void visit(const EmptyValue& expr) override { throw std::logic_error("Cannot translate expression into logic: 'EMPTY' not supported"); }
    void visit(const NDetValue& expr) override { throw std::logic_error("Cannot translate expression into logic: '*' not supported"); }

    std::unique_ptr<SymbolicAxiom> Translate(const BinaryExpression& expr) {
        assert(expr.op != BinaryExpression::Operator::OR);
        assert(expr.op != BinaryExpression::Operator::AND);
        expr.lhs->accept(*this);
        auto lhs = std::move(result);
        expr.rhs->accept(*this);
        auto rhs = std::move(result);
        auto op = TranslateOp(expr.op);
        return std::make_unique<SymbolicAxiom>(std::move(lhs), op, std::move(rhs));
    }
};

struct ExpressionTranslator : public BaseVisitor {
    RelationTranslator translator;
    explicit ExpressionTranslator(const Annotation& context) : translator(context) {}

    std::unique_ptr<SeparatingConjunction> result = std::make_unique<SeparatingConjunction>();
    std::unique_ptr<StackDisjunction> disjunction = nullptr;

    void HandleConjunction(const BinaryExpression& expr) {
        if (disjunction)
            throw std::logic_error("Cannot translate expression into logic: unsupported nesting of '&&' inside '||'.");
        expr.rhs->accept(*this);
        expr.lhs->accept(*this);
    }

    void HandleDisjunction(const BinaryExpression& expr) {
        bool needsFlush = disjunction == nullptr;
        if (needsFlush) disjunction = std::make_unique<StackDisjunction>();
        expr.rhs->accept(*this);
        expr.lhs->accept(*this);
        if (needsFlush) {
            result->conjuncts.push_back(std::move(disjunction));
            disjunction = nullptr;
        }
    }

    void HandleRelation(const BinaryExpression& expr) {
        auto axiom = translator.Translate(expr);
        if (disjunction) disjunction->axioms.push_back(std::move(axiom));
        else result->conjuncts.push_back(std::move(axiom));
    }

    void visit(const BinaryExpression& expr) override {
        switch (expr.op) {
            case BinaryExpression::Operator::AND: HandleConjunction(expr); return;
            case BinaryExpression::Operator::OR: HandleDisjunction(expr); return;
            default: HandleRelation(expr); return;
        }
    }
};

std::unique_ptr<Formula> ExpressionToFormula(const Expression& expression, const Annotation& context) {
    ExpressionTranslator translator(context);
    expression.accept(translator);
    heal::Simplify(*translator.result);
    return std::move(translator.result);
}

std::unique_ptr<heal::Annotation> DefaultSolver::Post(std::unique_ptr<Annotation> pre, const Assume& cmd) const {
    auto formula = ExpressionToFormula(*cmd.expr, *pre);
    pre->now = heal::Conjoin(std::move(pre->now), std::move(formula));
    return pre;
}

//
// Malloc
//

bool UpdateVariableValuation(Formula& formula, const VariableDeclaration& variable, const SymbolicVariableDeclaration& newValue) {
    // updating the variable directly is easier than splitting the formula into the resource and the frame
    EqualsToAxiom* resource = FindResource(variable, formula);
    if (resource == nullptr) return false;
    assert(&resource->variable->decl == &variable);
    resource->value = std::make_unique<SymbolicVariable>(newValue);
    return true;
}

std::pair<const SymbolicVariableDeclaration&, std::unique_ptr<Formula>> AllocateFreshCell(const Type &allocationType, const Type &flowType, const Annotation &context) {
    SymbolicFactory factory(context);
    auto node = std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(allocationType));
    auto& flow = factory.GetUnusedFlowVariable(flowType);
    std::map<std::string, std::unique_ptr<SymbolicVariable>> fieldToValue;
    for (const auto& [field, type] : allocationType.fields) {
        fieldToValue[field] = std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(type));
    }
    auto cell = std::make_unique<PointsToAxiom>(std::move(node), true, flow, std::move(fieldToValue));
    // TODO: default initialization?
    return { cell->node->decl_storage.get(), std::move(cell) };
}

std::unique_ptr<heal::Annotation> DefaultSolver::Post(std::unique_ptr<Annotation> pre, const Malloc& cmd) const {
    auto& flowType = Config().flowDomain->GetFlowValueType();
    auto [cell, formula] = AllocateFreshCell(cmd.lhs.type, flowType, *pre);
    auto success = UpdateVariableValuation(*pre->now, cmd.lhs, cell);
    if (!success) throw std::logic_error("Unsafe update: variable '" + cmd.lhs.name + "' is not accessible."); // TODO: better error class
    pre->now = heal::Conjoin(std::move(pre->now), std::move(formula));
    return pre;
}

//
// Assignment
//

std::pair<std::unique_ptr<heal::Annotation>, std::unique_ptr<Effect>> DefaultSolver::Post(std::unique_ptr<Annotation> pre, const Assignment& cmd) const {
    if (auto variable = dynamic_cast<const VariableExpression*>(cmd.lhs.get())) {
        return Post(std::move(pre), cmd, *variable);
    } else if (auto dereference = dynamic_cast<const Dereference*>(cmd.lhs.get())) {
        return Post(std::move(pre), cmd, *dereference);
    } else {
        std::stringstream err;
        err << "Unsupported assignment: expected a variable or a dereference as left-hand side, got '.";
        cola::print(*cmd.lhs, err);
        err << "'.";
        throw std::logic_error(err.str()); // TODO: better error class
    }
}

struct PostVariableAssignmentVisitor : public BaseVisitor {
    std::unique_ptr<SymbolicVariable> value;
    std::unique_ptr<Formula> context;

    void visit(const BooleanValue& expr) override { value = std::make_unique<SymbolicBool>(expr.value); }
    void visit(const NullValue& /*expr*/) override { value = std::make_unique<SymbolicNull>(); }
    void visit(const MaxValue& /*expr*/) override { value = std::make_unique<SymbolicMax>(); }
    void visit(const MinValue& /*expr*/) override { value = std::make_unique<SymbolicMin>(); }
    void visit(const VariableExpression& expr) override { value = std::make_unique<SymbolicVariable>(GetValue(expr)); }
    void visit(const Dereference& expr) override { value = std::make_unique<SymbolicVariable>(GetValue(expr)); }

    void visit(const BooleanValue& node) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const BooleanValue&"); }
    void visit(const NullValue& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const NullValue&"); }
    void visit(const MaxValue& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const MaxValue&"); }
    void visit(const MinValue& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const MinValue&"); }
    void visit(const VariableExpression& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const VariableExpression&"); }
    void visit(const BinaryExpression& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const BinaryExpression&"); }
    void visit(const Dereference& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const Dereference&"); }

    void visit(const NegatedExpression& /*node*/) override { throw std::logic_error("Unsupported assignment: right-hand side must not be negated."); } // TODO: better error class
    void visit(const EmptyValue& /*node*/) override { throw std::logic_error("Unsupported assignment: right-hand side must not be 'EMPTY'."); } // TODO: better error class
    void visit(const NDetValue& /*node*/) override { throw std::logic_error("Unsupported assignment: right-hand side must not be '*'."); } // TODO: better error class
};

std::pair<std::unique_ptr<SymbolicVariable>, std::unique_ptr<Formula>> Eval(const Expression& expression) {
    // TODO: implement (should be done in eval.hpp, maybe yields an additional formula with info about the value?)
    throw std::logic_error("not yet implemented");
}

std::pair<std::unique_ptr<heal::Annotation>, std::unique_ptr<Effect>> DefaultSolver::Post(std::unique_ptr<heal::Annotation> pre, const cola::Assignment& cmd, const cola::VariableExpression& lhs) const {
    if (lhs.decl.is_shared) throw std::logic_error("Unsupported assignment: cannot assign to shared variables."); // TODO: better error class
    auto resource = solver::FindResource(lhs.decl, *pre);
    auto [value, context] = Eval(*cmd.rhs);
    resource->value = std::move(value);
    auto effect = std::make_unique<Effect>(heal::Copy(*resource), std::move(context), cmd);
    return { std::move(pre), std::move(effect) };
}

std::pair<std::unique_ptr<heal::Annotation>, std::unique_ptr<Effect>> DefaultSolver::Post(std::unique_ptr<heal::Annotation> pre, const cola::Assignment& cmd, const cola::Dereference& lhs) const {
    throw std::logic_error("not yet implemented");
}