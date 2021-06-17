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

inline std::unique_ptr<SeparatingConjunction> ExpressionToFormula(const Expression& expression, const Annotation& context) {
    ExpressionTranslator translator(context);
    expression.accept(translator);
    heal::Simplify(*translator.result);
    return std::move(translator.result);
}

inline std::unique_ptr<StackDisjunction> ExtractDisjunction(SeparatingConjunction& formula) {
    struct : public DefaultLogicNonConstListener {
        StackDisjunction* disjunction = nullptr;
        void enter(StackDisjunction& obj) override {
            if (disjunction || obj.axioms.size() <= 1) return;
            disjunction = &obj;
        }
    } search;
    formula.accept(search);
    if (!search.disjunction) return nullptr;
    auto disjunction = std::make_unique<StackDisjunction>(std::move(search.disjunction->axioms));
    search.disjunction->axioms.clear();
    return disjunction;
}

inline std::deque<std::unique_ptr<SeparatingConjunction>> InlineDisjunction(std::unique_ptr<SeparatingConjunction> formula, std::unique_ptr<StackDisjunction> disjunction) {
    // TODO: consume formula
    std::deque<std::unique_ptr<SeparatingConjunction>> result;
    for (auto& axiom : disjunction->axioms) {
        result.push_back(heal::Copy(*formula));
        result.back()->conjuncts.push_back(std::move(axiom));
    }
    return result;
}

std::deque<std::unique_ptr<SeparatingConjunction>> SplitDisjunctions(std::unique_ptr<SeparatingConjunction> input) {
    std::deque<std::unique_ptr<SeparatingConjunction>> result;
    std::deque<std::unique_ptr<SeparatingConjunction>> worklist;
    worklist.push_back(std::move(input));
    while (!worklist.empty()) {
        auto formula = std::move(worklist.back());
        worklist.pop_back();
        auto disjunction = ExtractDisjunction(*formula);
        if (!disjunction) result.push_back(std::move(formula));
        else {
            auto inlined = InlineDisjunction(std::move(formula), std::move(disjunction));
            std::move(inlined.begin(), inlined.end(), std::back_inserter(worklist));
        }
    }
    for (auto& elem : result) heal::Simplify(*elem);
    return result;
}

inline std::deque<std::unique_ptr<SeparatingConjunction>> PrunePaths(const Annotation& annotation, std::deque<std::unique_ptr<SeparatingConjunction>> paths) {
    z3::context context;
    z3::solver solver(context);
    Z3Encoder encoder(context, solver);
    solver.add(encoder(annotation));
    ImplicationCheckSet checks(context);
    for (auto& elem : paths) {
        checks.Add(!encoder(*elem), [&elem](bool holds){
            if (holds) elem.reset(nullptr);
        });
    }
    solver::ComputeImpliedCallback(solver, checks);
    paths.erase(std::remove_if(paths.begin(), paths.end(), [](const auto& elem){ return elem.get() == nullptr; }), paths.end());

    return paths;
}

PostImage DefaultSolver::Post(std::unique_ptr<Annotation> pre, const Assume& cmd) const {
    std::cout << "******** Post image for " << *pre << " " << cmd;
    pre->now = solver::ExpandMemoryFrontierForAccess(std::move(pre->now), Config(), *cmd.expr);
    PostImage result;
    auto paths = SplitDisjunctions(ExpressionToFormula(*cmd.expr, *pre));
//    paths = PrunePaths(*pre, std::move(paths)); // TODO: do this here?
    for (auto& formula : paths) {
        auto post = heal::Copy(*pre);
        post->now = heal::Conjoin(std::move(post->now), heal::Copy(*formula)); // TODO: avoid copy
//        post->now = solver::ExpandMemoryFrontier(std::move(post->now), Config(), *formula); // TODO: do this here?
        bool isUnsatisfiable = false;
        post = solver::TryAddPureFulfillment(std::move(post), Config(), &isUnsatisfiable);
        if (isUnsatisfiable) continue;
        heal::InlineAndSimplify(*post->now);
        std::cout << "  ~> " << *post << std::endl;
        assert(heal::CollectObligations(*post).size() + heal::CollectFulfillments(*post).size() > 0);
        result.posts.push_back(std::move(post));
    }
    return result;
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

inline void CheckCell(const Formula& state, const SymbolicVariableDeclaration& address, const SolverConfig& config) {
    z3::context context;
    z3::solver solver(context);
    Z3Encoder encoder(context, solver);
    auto memory = solver::FindMemory(address, state); // TODO: avoid search
    if (!memory) throw std::logic_error("Cannot retrieve newly allocated memory resource."); // TODO: better error handling
    auto good = solver::ComputeImplied(solver, encoder(state), encoder(*config.GetNodeInvariant(*memory)));
    if (!good) throw std::logic_error("Newly allocated node does not satisfy invariant."); // TODO: better error handling
}

inline std::pair<const SymbolicVariableDeclaration&, std::unique_ptr<Formula>> AllocateFreshCell(const Type &allocationType, const Annotation &context, const SolverConfig& config) {
    SymbolicFactory factory(context);
    auto result = std::make_unique<SeparatingConjunction>();
    auto& address = factory.GetUnusedSymbolicVariable(allocationType);
    result->conjuncts.push_back(std::make_unique<SymbolicAxiom>(std::make_unique<SymbolicVariable>(address), SymbolicAxiom::NEQ, std::make_unique<SymbolicNull>()));
    auto node = std::make_unique<SymbolicVariable>(address);
    auto& flow = factory.GetUnusedFlowVariable(config.GetFlowValueType());
    result->conjuncts.push_back(std::make_unique<InflowEmptinessAxiom>(flow, true));
    std::map<std::string, std::unique_ptr<SymbolicVariable>> fieldToValue;
    for (const auto& [field, type] : allocationType.fields) {
        auto& value = factory.GetUnusedSymbolicVariable(type);
        fieldToValue[field] = std::make_unique<SymbolicVariable>(value);
        if (type.get().sort != Sort::PTR) continue;
        result->conjuncts.push_back(std::make_unique<SymbolicAxiom>(std::make_unique<SymbolicVariable>(value), SymbolicAxiom::EQ, std::make_unique<SymbolicNull>()));
    }
    result->conjuncts.push_back(std::make_unique<PointsToAxiom>(std::move(node), true, flow, std::move(fieldToValue)));
    for (const auto* symbol : heal::CollectSymbolicSymbols(context)) {
        if (symbol->type != allocationType) continue;
        if (auto other = dynamic_cast<const SymbolicVariableDeclaration*>(symbol)) {
            result->conjuncts.push_back(std::make_unique<SymbolicAxiom>(std::make_unique<SymbolicVariable>(address), SymbolicAxiom::NEQ, std::make_unique<SymbolicVariable>(*other))); // TODO: is this correct?
        }
    }
    CheckCell(*result, address, config);
    return { address, std::move(result) };
}

PostImage DefaultSolver::Post(std::unique_ptr<Annotation> pre, const Malloc& cmd) const {
    std::cout << "******** Post image for "; heal::Print(*pre, std::cout); std::cout << " "; cola::print(cmd, std::cout);
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
    std::cout << "******** Post image for "; heal::Print(*pre, std::cout); std::cout << " "; cola::print(cmd, std::cout);
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
    pre->now = solver::ExpandMemoryFrontierForAccess(std::move(pre->now), Config(), *cmd.rhs);
    std::cout << "    ==> pre after expansion: " << *pre << std::endl;

    SymbolicFactory factory(*pre);
    auto value = EagerValuationMap(*pre).Evaluate(*cmd.rhs);
    auto& symbol = factory.GetUnusedSymbolicVariable(lhs.type());
    auto* resource = FindResource(lhs.decl, *pre->now);
    if (!resource) throw std::logic_error("Unsafe update: variable '" + lhs.decl.name + "' is not accessible."); // TODO: better error handling
    resource->value = std::make_unique<SymbolicVariable>(symbol);
    pre->now->conjuncts.push_back(std::make_unique<SymbolicAxiom>(std::make_unique<SymbolicVariable>(symbol), SymbolicAxiom::EQ, std::move(value)));

    // try derive helpful knowledge
    if (cmd.rhs->sort() == Sort::PTR) { //if (!solver::GetDereferences(*cmd.rhs).empty()) { // TODO: when to do this?
        pre->now = solver::ExpandMemoryFrontier(std::move(pre->now), factory, Config(), { &symbol });
        pre = TryAddPureFulfillment(std::move(pre), Config());
    }

//    begin debug
//    std::cout << "== Post: " << std::endl; heal::Print(*pre, std::cout); std::cout << std::endl;
//    end debug

    heal::InlineAndSimplify(*pre->now);
    std::cout << *pre << std::endl << std::endl;
    assert(heal::CollectObligations(*pre).size() + heal::CollectFulfillments(*pre).size() > 0);
    return PostImage(std::move(pre)); // local variable update => no effect
}