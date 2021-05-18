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

struct ExpressionTranslator : public BaseVisitor {
    LazyValuationMap evaluator;
    explicit ExpressionTranslator(const Annotation& context) : evaluator(context) {}

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
        auto axiom = std::make_unique<SymbolicAxiom>(evaluator.Evaluate(*expr.lhs), TranslateOp(expr.op), evaluator.Evaluate(*expr.rhs));
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

    // TODO: all other expressions are not supported => override visit to print better error message (or ignore and rely on preprocessing?)
};

std::unique_ptr<Formula> ExpressionToFormula(const Expression& expression, const Annotation& context) {
    ExpressionTranslator translator(context);
    expression.accept(translator);
    heal::Simplify(*translator.result);
    return std::move(translator.result);
}

PostImage DefaultSolver::Post(std::unique_ptr<Annotation> pre, const Assume& cmd) const {
    auto formula = ExpressionToFormula(*cmd.expr, *pre);
    pre->now = heal::Conjoin(std::move(pre->now), std::move(formula));
    return PostImage(std::move(pre));
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

PostImage DefaultSolver::Post(std::unique_ptr<Annotation> pre, const Malloc& cmd) const {
    auto& flowType = Config().flowDomain->GetFlowValueType();
    auto [cell, formula] = AllocateFreshCell(cmd.lhs.type, flowType, *pre);
    auto success = UpdateVariableValuation(*pre->now, cmd.lhs, cell);
    if (!success) throw std::logic_error("Unsafe update: variable '" + cmd.lhs.name + "' is not accessible."); // TODO: better error class
    pre->now = heal::Conjoin(std::move(pre->now), std::move(formula));
    return PostImage(std::move(pre));
}

//
// Assignment
//

PostImage DefaultSolver::Post(std::unique_ptr<Annotation> pre, const Assignment& cmd) const {
    if (auto variable = dynamic_cast<const VariableExpression*>(cmd.lhs.get())) {
        return PostVariableUpdate(std::move(pre), cmd, *variable);
    } else if (auto dereference = dynamic_cast<const Dereference*>(cmd.lhs.get())) {
        return PostMemoryUpdate(std::move(pre), cmd, *dereference);
    } else {
        std::stringstream err;
        err << "Unsupported assignment: expected a variable or a dereference as left-hand side, got '.";
        cola::print(*cmd.lhs, err);
        err << "'.";
        throw std::logic_error(err.str()); // TODO: better error class
    }
}

PostImage DefaultSolver::PostVariableUpdate(std::unique_ptr<heal::Annotation> pre, const cola::Assignment& cmd, const cola::VariableExpression& lhs) const {
    if (lhs.decl.is_shared)
        throw std::logic_error("Unsupported assignment: cannot assign to shared variables."); // TODO: better error class

    auto resource = solver::FindResource(lhs.decl, *pre);
    auto value = EagerValuationMap(*pre).Evaluate(*cmd.rhs);
    auto& symbol = SymbolicFactory(*pre).GetUnusedSymbolicVariable(lhs.type());
    resource->value = std::make_unique<SymbolicVariable>(symbol);
    pre->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(std::make_unique<SymbolicVariable>(symbol), SymbolicAxiom::EQ, std::move(value)));
    heal::Simplify(*pre->now); // TODO: have trivial equalities inlined

    // TODO: generate points to for newly traversed/reached memory
    // TODO: generate fulfillments for pure specs
    return PostImage(std::move(pre)); // local variable update => no effect
}
