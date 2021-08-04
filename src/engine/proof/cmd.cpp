#include "engine/proof.hpp"

#include "programs/util.hpp"
#include "util/shortcuts.hpp"

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

void ProofGenerator::Visit(const Assert& cmd) {
    throw std::logic_error("unsupported: 'assert'"); // TODO: better error handling
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

void ProofGenerator::Visit(const MemoryRead& cmd) {
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

//inline std::unique_ptr<Annotation> PerformMacroAssignment(std::unique_ptr<Annotation> annotation, const VariableDeclaration& lhs, const Expression& rhs, const Solver& solver) {
//    Assignment dummy(std::make_unique<VariableExpression>(lhs), cola::copy(rhs)); // TODO: avoid copies
//    auto post = solver.Post(std::move(annotation), dummy);
//    if (!post.effects.empty()) throw std::logic_error("Returning from macros must not produce effects."); // TODO: better error handling
//    if (post.annotations.size() != 1) throw std::logic_error("Returning from macro must produce exactly one annotation."); // TODO: better error handling
//    return std::move(post.annotations.front());
//}
//
//inline std::unique_ptr<Annotation> PassMacroArguments(std::unique_ptr<Annotation> annotation, const Macro& command, const Solver& solver) {
//    assert(command.args.size() == command.decl.args.size());
//    for (std::size_t index = 0; index < command.args.size(); ++index) {
//        annotation = PerformMacroAssignment(std::move(annotation), *command.decl.args.at(index), *command.args.at(index), solver);
//    }
//    return annotation;
//}
//
//inline std::unique_ptr<Annotation> ReturnFromMacro(std::unique_ptr<Annotation> annotation, const Return* command, const Macro& macro, const Solver& solver) {
//    if (!command) return annotation;
//    assert(macro.lhs.size() == command->expressions.size());
//
//    for (std::size_t index = 0; index < macro.lhs.size(); ++index) {
//        annotation = PerformMacroAssignment(std::move(annotation), macro.lhs.at(index), *command->expressions.at(index), solver);
//    }
//    return annotation;
//}

template<typename T> T* AsPtr(T& obj) { return &obj; }
template<typename T> T* AsPtr(T* obj) { return obj; }

template<typename T, typename U>
inline std::unique_ptr<Statement> MakeMacroAssignments(const T& lhs, const U& rhs) {
    assert(lhs.size() == rhs.size());
    std::unique_ptr<Statement> result = std::make_unique<Skip>();
    for (std::size_t index = 0; index < lhs.size(); ++index) {
        // TODO: avoid copies
        auto theLhs = std::make_unique<VariableExpression>(*AsPtr(lhs.at(index).get()));
        auto theRhs = plankton::Copy(*rhs.at(index));
        auto assign = std::make_unique<Assignment>(std::move(theLhs), std::move(theRhs));
        result = std::make_unique<Sequence>(std::move(result), std::move(assign));
    }
    return result;
}

void ProofGenerator::HandleMacroProlog(const Macro& macro) {
    breaking.clear();
    returning.clear();

    ApplyTransformer([this,&macro](auto annotation){
        return solver.PostEnter(std::move(annotation), macro.decl);
    });

    MakeMacroAssignments(macro.decl.args, macro.args)->accept(*this);
}

void ProofGenerator::HandleMacroEpilog(const Macro& macro) {
    decltype(current) newCurrent;
    for (auto& [annotation, command] : returning) {
        current.clear();
        current.push_back(std::move(annotation));
        MakeMacroAssignments(macro.lhs, command->expressions)->accept(*this);
        MoveInto(std::move(current), newCurrent);
    }
    current = std::move(newCurrent);

    ApplyTransformer([this,&macro](auto annotation){
        return solver.PostLeave(std::move(annotation), macro.decl);
    });
}

void ProofGenerator::visit(const Macro& cmd) {
    // save caller context
    auto breakingOuter = std::move(breaking);
    auto returningOuter = std::move(returning);
    
    // invoke callee
    HandleMacroProlog(cmd);
    cmd.decl.accept(*this);
    HandleMacroEpilog(cmd);

    // restore caller context
    breaking = std::move(breakingOuter);
    returning = std::move(returningOuter);

    std::cout << std::endl << "________" << std::endl << "Post annotation for macro '" << cmd.decl.name << "':" << std::endl;
    for (const auto& elem : current) std::cout << "  ~~> " << *elem << std::endl; std::cout << std::endl << std::endl << std::endl;
}
