#include "default_solver.hpp"

#include "cola/util.hpp"
#include "heal/util.hpp"
#include "eval.hpp"
#include "encoder.hpp"
#include "z3++.h"
#include "expand.hpp"
#include "post_helper.hpp"

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

    inline void HandleConjunction(const BinaryExpression& expr) {
        if (disjunction)
            throw std::logic_error("Cannot translate expression into logic: unsupported nesting of '&&' inside '||'.");
        expr.rhs->accept(*this);
        expr.lhs->accept(*this);
    }

    inline void HandleDisjunction(const BinaryExpression& expr) {
        bool needsFlush = disjunction == nullptr;
        if (needsFlush) disjunction = std::make_unique<StackDisjunction>();
        expr.rhs->accept(*this);
        expr.lhs->accept(*this);
        if (needsFlush) {
            result->conjuncts.push_back(std::move(disjunction));
            disjunction = nullptr;
        }
    }

    inline void HandleRelation(const BinaryExpression& expr) {
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

inline std::unique_ptr<Formula> ExpressionToFormula(const Expression& expression, const Annotation& context) {
    ExpressionTranslator translator(context);
    expression.accept(translator);
    heal::Simplify(*translator.result);
    return std::move(translator.result);
}

PostImage DefaultSolver::Post(std::unique_ptr<Annotation> pre, const Assume& cmd) const {
    std::cout << " -- Post image for "; heal::Print(*pre, std::cout); std::cout << " "; cola::print(cmd, std::cout);
    // TODO: check unsatisfiability (cannot return false at the moment)
    auto formula = ExpressionToFormula(*cmd.expr, *pre);

    z3::context context;
    z3::solver solver(context);
    Z3Encoder encoder(context, solver);
    solver.add(encoder(*pre->now));
    if (solver.check() == z3::unsat) std::cout << "======> PRE IS UNSAT" << std::endl;
    solver.add(encoder(*formula));
    if (solver.check() == z3::unsat) std::cout << "======> POST IS UNSAT" << std::endl;

    pre->now->conjuncts.push_back(heal::Copy(*formula)); // TODO: avoid copy
//    heal::Print(*pre, std::cout); std::cout << std::endl;
    SymbolicFactory factory(*pre);
    pre->now = solver::ExpandMemoryFrontier(std::move(pre->now), factory, Config(), *formula);
    pre = TryAddPureFulfillment(std::move(pre), *cmd.expr, Config());
    heal::InlineAndSimplify(*pre->now);
    heal::Print(*pre, std::cout); std::cout << std::endl;
    return PostImage(std::move(pre));
}

//
// Malloc
//

inline bool TryUpdateVariableValuation(Formula& formula, const VariableDeclaration& variable, const SymbolicVariableDeclaration& newValue) {
    // updating the variable directly is easier than splitting the formula into the resource and the frame
    EqualsToAxiom* resource = FindResource(variable, formula);
    if (resource == nullptr) return false;
    assert(&resource->variable->decl == &variable);
    resource->value = std::make_unique<SymbolicVariable>(newValue);
    return true;
}

inline void CheckCell(const Formula& state, const PointsToAxiom& memory, const SolverConfig& config) {
    z3::context context;
    z3::solver solver(context);
    Z3Encoder encoder(context, solver);
    auto good = solver::ComputeImplied(solver, encoder(state), encoder(*config.GetNodeInvariant(memory)));
    if (good) return;
    throw std::logic_error("Newly allocated node does not satisfy invariant."); // TODO: better error handling
}

inline std::pair<const SymbolicVariableDeclaration&, std::unique_ptr<Formula>> AllocateFreshCell(const Type &allocationType, const Annotation &context, const SolverConfig& config) {
    SymbolicFactory factory(context);
    auto& address = factory.GetUnusedSymbolicVariable(allocationType);
    auto node = std::make_unique<SymbolicVariable>(address);
    auto& flow = factory.GetUnusedFlowVariable(config.GetFlowValueType());
    std::map<std::string, std::unique_ptr<SymbolicVariable>> fieldToValue;
    for (const auto& [field, type] : allocationType.fields) {
        fieldToValue[field] = std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(type));
    }
    auto cell = std::make_unique<PointsToAxiom>(std::move(node), true, flow, std::move(fieldToValue));

    // TODO: default initialization?
    // TODO: add no-flow constraint?
    CheckCell(*cell, *cell, config);
    return { cell->node->decl_storage.get(), std::move(cell) };
}

PostImage DefaultSolver::Post(std::unique_ptr<Annotation> pre, const Malloc& cmd) const {
    std::cout << " -- Post image for "; heal::Print(*pre, std::cout); std::cout << " "; cola::print(cmd, std::cout);
    if (cmd.lhs.is_shared)
        throw std::logic_error("Unsupported assignment: cannot assign to shared variables."); // TODO: better error handling

    auto [cell, formula] = AllocateFreshCell(cmd.lhs.type, *pre, Config());
    auto success = TryUpdateVariableValuation(*pre->now, cmd.lhs, cell);
    if (!success) throw std::logic_error("Unsafe update: variable '" + cmd.lhs.name + "' is not accessible."); // TODO: better error class

    pre->now = heal::Conjoin(std::move(pre->now), std::move(formula));
    heal::Print(*pre, std::cout); std::cout << std::endl;
    return PostImage(std::move(pre));
}

//
// Assignment
//

PostImage DefaultSolver::Post(std::unique_ptr<Annotation> pre, const Assignment& cmd) const {
    std::cout << " -- Post image for "; heal::Print(*pre, std::cout); std::cout << " "; cola::print(cmd, std::cout);
    if (auto variable = dynamic_cast<const VariableExpression*>(cmd.lhs.get())) {
        return PostVariableUpdate(std::move(pre), cmd, *variable);
    } else if (auto dereference = dynamic_cast<const Dereference*>(cmd.lhs.get())) {
        return PostMemoryUpdate(std::move(pre), cmd, *dereference);
    } else {
        std::stringstream err;
        err << "Unsupported assignment: expected a variable or a dereference as left-hand side, got '";
        cola::print(*cmd.lhs, err);
        err << "'.";
        throw std::logic_error(err.str()); // TODO: better error class
    }
}

PostImage DefaultSolver::PostVariableUpdate(std::unique_ptr<Annotation> pre, const Assignment& cmd, const VariableExpression& lhs) const {
    if (lhs.decl.is_shared)
        throw std::logic_error("Unsupported assignment: cannot assign to shared variables."); // TODO: better error handling

//    begin debug
//    std::cout << std::endl << std::endl << std::endl << "====================" << std::endl;
//    std::cout << "== Command: "; cola::print(cmd, std::cout);
//    std::cout << "== Pre: " << std::endl; heal::Print(*pre, std::cout); std::cout << std::endl << std::flush;
//    end debug

    // perform update
    SymbolicFactory factory(*pre);
    auto value = EagerValuationMap(*pre).Evaluate(*cmd.rhs);
    auto& symbol = factory.GetUnusedSymbolicVariable(lhs.type());
    auto* resource = FindResource(lhs.decl, *pre->now);
    if (!resource) throw std::logic_error("Unsafe update: variable '" + lhs.decl.name + "' is not accessible."); // TODO: better error handling
    resource->value = std::make_unique<SymbolicVariable>(symbol);
    pre->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(std::make_unique<SymbolicVariable>(symbol), SymbolicAxiom::EQ, std::move(value)));

    // try derive helpful knowledge
    pre->now = solver::ExpandMemoryFrontier(std::move(pre->now), factory, Config(), *resource->value); // TODO: force extension for symbol (=resource->value->Decl())?
    pre = TryAddPureFulfillment(std::move(pre), *cmd.rhs, Config());

//    begin debug
//    std::cout << "== Post: " << std::endl; heal::Print(*pre, std::cout); std::cout << std::endl;
//    end debug

    heal::InlineAndSimplify(*pre->now);
    heal::Print(*pre, std::cout); std::cout << std::endl << std::endl;
    return PostImage(std::move(pre)); // local variable update => no effect
}