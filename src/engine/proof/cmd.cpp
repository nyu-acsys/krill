#include "engine/proof.hpp"

#include "programs/util.hpp"
#include "util/shortcuts.hpp"
#include "util/log.hpp"

using namespace plankton;


template<typename C>
static inline std::function<PostImage(std::unique_ptr<Annotation>)> MakePostTransformer(const C& cmd, const Solver& solver) {
    return [&cmd, &solver](auto annotation){ return solver.Post(std::move(annotation), cmd); };
}


void ProofGenerator::Visit(const Skip& /*cmd*/) {
    /* do nothing */
}

void ProofGenerator::Visit(const Break& /*cmd*/) {
    MoveInto(std::move(current), breaking);
    current.clear();
}

void ProofGenerator::Visit(const Fail& /*cmd*/) {
    for (const auto& annotation : current) {
        if (solver.IsUnsatisfiable(*annotation)) continue;
        throw std::logic_error("Program potentially fails / violates an assertion.");
    }
    current.clear();
}

void ProofGenerator::Visit(const Assume& cmd) {
    ApplyTransformer(MakePostTransformer(cmd, solver));
    MakeInterferenceStable(cmd);
}

void ProofGenerator::Visit(const Malloc& cmd) {
    ApplyTransformer(MakePostTransformer(cmd, solver));
    MakeInterferenceStable(cmd);
}

void ProofGenerator::Visit(const VariableAssignment& cmd) {
    ApplyTransformer(MakePostTransformer(cmd, solver));
    MakeInterferenceStable(cmd);
}

void ProofGenerator::Visit(const MemoryWrite& cmd) {
    ApplyTransformer(MakePostTransformer(cmd, solver));
    MakeInterferenceStable(cmd);
}

void ProofGenerator::Visit(const Return& cmd) {
    // pointer accessibility is checked when returned values/variables are used
    for (auto& annotation : current) returning.emplace_back(std::move(annotation), &cmd);
    current.clear();
}


//
// Macros
//

inline std::vector<std::unique_ptr<VariableExpression>>
AsExpr(const std::vector<std::unique_ptr<VariableDeclaration>>& decls) {
    auto result = plankton::MakeVector<std::unique_ptr<VariableExpression>>(decls.size());
    for (const auto& elem : decls) result.push_back(std::make_unique<VariableExpression>(*elem));
    return result;
}

inline std::unique_ptr<Statement> MakeMacroAssignment(const std::vector<std::unique_ptr<VariableExpression>>& lhs,
                                                      const std::vector<std::unique_ptr<SimpleExpression>>& rhs) {
    auto result = std::make_unique<VariableAssignment>();
    plankton::MoveInto(::CopyAll(lhs), result->lhs);
    plankton::MoveInto(::CopyAll(rhs), result->rhs);
    return result;
}

void ProofGenerator::HandleMacroProlog(const Macro& macro) {
    breaking.clear();
    returning.clear();

    ApplyTransformer([this,&macro](auto annotation){
        return solver.PostEnter(std::move(annotation), macro.Func());
    });

    MakeMacroAssignment(AsExpr(macro.Func().parameters), macro.arguments)->Accept(*this);
}

void ProofGenerator::HandleMacroEpilog(const Macro& macro) {
    decltype(current) newCurrent;
    for (auto& [annotation, command] : returning) {
        current.clear();
        current.push_back(std::move(annotation));
        MakeMacroAssignment(macro.lhs, command->expressions)->Accept(*this);
        MoveInto(std::move(current), newCurrent);
    }
    current = std::move(newCurrent);

    ApplyTransformer([this,&macro](auto annotation){
        return solver.PostLeave(std::move(annotation), macro.Func());
    });
}

void ProofGenerator::Visit(const Macro& cmd) {
    // save caller context
    auto breakingOuter = std::move(breaking);
    auto returningOuter = std::move(returning);
    
    // invoke callee
    // TODO: frame non-shared 'EqualsTo' predicates
    // TODO: frame 'LocalMemoryResource' predicates that are guaranteed to be inaccessible in macro
    HandleMacroProlog(cmd);
    cmd.Func().Accept(*this);
    HandleMacroEpilog(cmd);

    // restore caller context
    breaking = std::move(breakingOuter);
    returning = std::move(returningOuter);

    DEBUG(std::endl << "=== post annotations for macro '" << cmd.Func().name << "':" << std::endl)
    for (const auto& elem : current) DEBUG("  -- " << *elem << std::endl)
    DEBUG(std::endl << std::endl)
}
