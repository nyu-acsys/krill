#include "engine/proof.hpp"

#include "programs/util.hpp"
#include "logics/util.hpp"
#include "engine/util.hpp"
#include "util/shortcuts.hpp"
#include "util/log.hpp"

using namespace plankton;

constexpr const bool TABULATE_SUBROUTINES = true;


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


//
// Prolog
//

void ProofGenerator::HandleMacroProlog(const Macro& macro) {
    breaking.clear();
    returning.clear();

    ApplyTransformer([this,&macro](auto annotation){
        auto result = solver.PostEnter(std::move(annotation), macro.Func());

        // TODO: simplified framing hack
        SymbolFactory factory(*result);
        for (const auto& lhs : macro.lhs) {
            auto& resource = plankton::GetResource(lhs->Decl(), *result->now);
            resource.value->decl = factory.GetFreshFO(lhs->Type());
        }
        return result;
    });

    MakeMacroAssignment(AsExpr(macro.Func().parameters), macro.arguments)->Accept(*this);
}


//
// Epilog
//

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
        annotation = solver.PostLeave(std::move(annotation), *macro.function.get().body);
        return solver.PostLeave(std::move(annotation), macro.Func());
    });
}


//
// Non-tabulated macro post
//

void ProofGenerator::HandleMacroEager(const Macro& cmd) {
    HandleMacroProlog(cmd);
    cmd.Func().Accept(*this);
    HandleMacroEpilog(cmd);
}


//
// Tabulated macro post
//

inline void CleanAnnotation(Annotation& annotation) {
    plankton::Simplify(annotation);

    // find symbols that do not occur in variable assignments or memory
    struct : public LogicListener {
        std::set<const SymbolDeclaration*> prune;
        void Enter(const SymbolDeclaration& object) override { prune.erase(&object); }
        void Visit(const StackAxiom&) override { /* do nothing */ }
        void Visit(const InflowEmptinessAxiom&) override { /* do nothing */ }
        void Visit(const InflowContainsValueAxiom&) override { /* do nothing */ }
        void Visit(const InflowContainsRangeAxiom&) override { /* do nothing */ }
    } collector;
    collector.prune = plankton::Collect<SymbolDeclaration>(annotation);
    annotation.Accept(collector);
    auto prune = std::move(collector.prune);

    // find unreachable memory addresses
    auto reach = plankton::ComputeReachability(*annotation.now);
    std::set<const SymbolDeclaration*> reachable;
    for (const auto* var : plankton::Collect<EqualsToAxiom>(*annotation.now)) {
        reachable.insert(&var->Value());
        plankton::InsertInto(reach.GetReachable(var->Value()), reachable);
    }
    auto pruneIfUnreachable = [&reachable,&prune](const auto* mem) {
        if (plankton::Membership(reachable, &mem->node->Decl())) return;
        prune.insert(&mem->node->Decl());
    };
    for (const auto* mem : plankton::Collect<SharedMemoryCore>(*annotation.now)) pruneIfUnreachable(mem);
    for (const auto& past : annotation.past) pruneIfUnreachable(past->formula.get());

    // remove parts that are not interesting
    auto containsPruned = [&prune](const auto& conjunct) {
        if (dynamic_cast<const LocalMemoryResource*>(conjunct.get())) return false;
        auto symbols = plankton::Collect<SymbolDeclaration>(*conjunct);
        return plankton::NonEmptyIntersection(symbols, prune);
    };
    plankton::RemoveIf(annotation.now->conjuncts, containsPruned);
    plankton::RemoveIf(annotation.past, containsPruned);
}

inline std::optional<ProofGenerator::AnnotationList>
ProofGenerator::LookupMacroPost(const Macro& macro, const Annotation& annotation) {
    auto find = macroPostTable.find(&macro.Func());
    if (find != macroPostTable.end()) {
        for (const auto&[pre, post]: find->second) {
            if (!solver.Implies(annotation, *pre)) continue;
            return plankton::CopyAll(post);
        }
    }
    return std::nullopt;
}

inline void
ProofGenerator::AddMacroPost(const Macro& macro, const Annotation& pre, const ProofGenerator::AnnotationList& post) {
    DEBUG("%% storing macro post: " << pre << " >>>>> ")
    for (const auto& elem : post) DEBUG(*elem)
    DEBUG(std::endl)
    macroPostTable[&macro.Func()].emplace_back(plankton::Copy(pre), plankton::CopyAll(post));
}

void ProofGenerator::HandleMacroLazy(const Macro& cmd) {
    HandleMacroProlog(cmd);
    decltype(current) post;

    // lookup tabulated posts
    plankton::RemoveIf(current, [this, &cmd, &post](const auto& elem) {
        auto lookup = LookupMacroPost(cmd, *elem);
        if (!lookup) return false;
        DEBUG("    ~~> found tabulated post" << std::endl)
        plankton::MoveInto(std::move(lookup.value()), post);
        return true;
    });

    // compute remaining posts
    DEBUG("    ~~> " << (current.empty() ? "not descending" : "descending...") << std::endl)
    if (!current.empty()) {
        for (auto& elem : current) CleanAnnotation(*elem);
        auto pre = plankton::CopyAll(current);
        cmd.Func().Accept(*this);
        HandleMacroEpilog(cmd);
        // for (auto& elem : current) CleanAnnotation(*elem);
        for (const auto& elem : pre) AddMacroPost(cmd, *elem, current);
    }

    plankton::MoveInto(std::move(post), current);
}


void ProofGenerator::Visit(const Macro& cmd) {
    // save caller context
    auto breakingOuter = std::move(breaking);
    auto returningOuter = std::move(returning);

    DEBUG(std::endl << "=== pre annotations for macro '" << cmd.Func().name << "':" << std::endl)
    for (const auto& elem : current) DEBUG("  -- " << *elem << std::endl)

    // TODO: proper framing?
    if (TABULATE_SUBROUTINES) HandleMacroLazy(cmd);
    else HandleMacroEager(cmd);

    // restore caller context
    breaking = std::move(breakingOuter);
    returning = std::move(returningOuter);

    DEBUG(std::endl << "=== post annotations for macro '" << cmd.Func().name << "':" << std::endl)
    for (const auto& elem : current) DEBUG("  -- " << *elem << std::endl)
    DEBUG(std::endl << std::endl)
}