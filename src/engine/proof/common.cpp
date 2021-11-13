#include "engine/proof.hpp"

#include "programs/util.hpp"
#include "util/shortcuts.hpp"
#include "util/log.hpp"

using namespace plankton;


inline const VariableDeclaration* GetVar(const Function& func, const std::string& name) {
    struct : public ProgramListener {
        std::set<const VariableDeclaration*> decls;
        void Enter(const VariableDeclaration& object) override {
            decls.insert(&object);
        }
    } collector;
    func.Accept(collector);
    for (const auto* var : collector.decls) {
        if (var->name == name) return var;
    }
    return nullptr;
}

inline std::unique_ptr<FutureSuggestion> MakeDebugFuture(const Program& program) {
    for (const auto& func : program.macroFunctions) {
        if (func->name != "locate") continue;
        auto* left = GetVar(*func, "left_1");
        auto* lnext = GetVar(*func, "lnext_1");
        auto* right = GetVar(*func, "right_1");
        if (!left || !lnext || !right) return nullptr;

        auto result = std::make_unique<FutureSuggestion>(std::make_unique<MemoryWrite>(
                std::make_unique<Dereference>(std::make_unique<VariableExpression>(*left), "next"),
                std::make_unique<VariableExpression>(*right)
        ));
        result->guard->conjuncts.push_back(std::make_unique<BinaryExpression>(
                BinaryOperator::EQ,
                std::make_unique<Dereference>(std::make_unique<VariableExpression>(*left), "marked"),
                std::make_unique<FalseValue>()
        ));
        result->guard->conjuncts.push_back(std::make_unique<BinaryExpression>(
                BinaryOperator::EQ,
                std::make_unique<Dereference>(std::make_unique<VariableExpression>(*left), "next"),
                std::make_unique<VariableExpression>(*lnext)
        ));
        return result;
    }
    return nullptr;
}

ProofGenerator::ProofGenerator(const Program& program, const SolverConfig& config)
        : program(program), solver(program, config), insideAtomic(false) {
    futureSuggestions.push_back(MakeDebugFuture(program));
}

void ProofGenerator::LeaveAllNestedScopes(const AstNode& node) {
    struct : public ProgramListener {
        std::deque<const Scope*> result;
        void Enter(const Scope& object) override { result.push_back(&object); }
    } collector;
    node.Accept(collector);

    for (auto it = collector.result.rbegin(); it != collector.result.rend(); ++it) {
        for (auto& annotation : current) {
            annotation = solver.PostLeave(std::move(annotation), **it);
        }
    }
}

void ProofGenerator::PruneCurrent() {
    // TODO: remove??

    for (auto& annotation : current) {
        if (!solver.IsUnsatisfiable(*annotation)) continue;
        annotation.reset(nullptr);
    }
    for (const auto& annotation : current) {
        if (!annotation) continue;
        for (auto& otherAnnotation : current) {
            if (!otherAnnotation) continue;
            if (annotation.get() == otherAnnotation.get()) continue;
            if (!solver.Implies(*otherAnnotation, *annotation)) continue;
            otherAnnotation.reset(nullptr);
        }
    }
    plankton::RemoveIf(current, [](const auto& elem) { return !elem; });
}

inline bool SameReturns(const Return* command, const Return* other) {
    if (command == other) return true;
    if (!command || !other) return false;
    if (command->expressions.size() != other->expressions.size()) return false;
    for (std::size_t index = 0; index < command->expressions.size(); ++index) {
        if (!plankton::SyntacticalEqual(*command->expressions.at(index), *other->expressions.at(index))) return false;
    }
    return true;
}

void ProofGenerator::PruneReturning() {
    // TODO: remove??

    // TODO: avoid code duplication
    for (auto& pair : returning) {
        if (!solver.IsUnsatisfiable(*pair.first)) continue;
        pair.first.reset(nullptr);
    }
    for (const auto& [annotation, command] : returning) {
        if (!annotation) continue;
        for (auto& [otherAnnotation, otherCommand] : returning) {
            if (!otherAnnotation) continue;
            if (annotation.get() == otherAnnotation.get()) continue;
            if (!SameReturns(command, otherCommand)) continue;
            if (!solver.Implies(*otherAnnotation, *annotation)) continue;
            otherAnnotation.reset(nullptr);
        }
    }
    plankton::RemoveIf(returning, [](const auto& elem) { return !elem.first; });
}

void ProofGenerator::ApplyTransformer(const std::function<std::unique_ptr<Annotation>(std::unique_ptr<Annotation>)>& transformer) {
    if (current.empty()) return;
    for (auto& annotation : current) {
        annotation = transformer(std::move(annotation));
    }
}

void ProofGenerator::ApplyTransformer(const std::function<PostImage(std::unique_ptr<Annotation>)>& transformer) {
    if (current.empty()) return;
    decltype(current) newCurrent;
    for (auto& annotation : current) {
        auto postImage = transformer(std::move(annotation));
        MoveInto(std::move(postImage.annotations), newCurrent);
        AddNewInterference(std::move(postImage.effects));
    }
    current = std::move(newCurrent);
}

void ProofGenerator::AddNewInterference(std::deque<std::unique_ptr<HeapEffect>> effects) {
    MoveInto(effects, newInterference);
}

bool ProofGenerator::ConsolidateNewInterference() {
    INFO(infoPrefix << "Checking for new effects. (" << newInterference.size() << ") " << std::endl)
    auto result = solver.AddInterference(std::move(newInterference));
    newInterference.clear();
    return result;
}

void ProofGenerator::MakeInterferenceStable(const Statement& after) {
    INFO(infoPrefix << "Applying interference." << INFO_SIZE << std::endl)
    if (insideAtomic) return;
    if (current.empty()) return;
    if (plankton::IsRightMover(after)) return;
    ApplyTransformer([this](auto annotation){
        // TODO: improve future?
        annotation = solver.ImprovePast(std::move(annotation));
        annotation = solver.MakeInterferenceStable(std::move(annotation));
        return solver.ReducePast(std::move(annotation));
    });
}

void ProofGenerator::JoinCurrent() {
    if (current.empty()) return;
    INFO(infoPrefix << "Preparing Join." << INFO_SIZE << std::endl)

    PruneCurrent();
    ImproveCurrentTime();
    ReduceCurrentTime();

    INFO(infoPrefix << "Joining." << INFO_SIZE << std::endl)
    auto join = solver.Join(std::move(current));
    current.clear();
    current.push_back(std::move(join));

    ReduceCurrentTime();
}
void ProofGenerator::ImproveCurrentTime() {
    INFO(infoPrefix << "Improving time predicates." << INFO_SIZE << std::endl)
    ApplyTransformer([this](auto annotation){
        return solver.ImprovePast(std::move(annotation));
    });
    for (const auto& future : futureSuggestions) {
        ApplyTransformer([this,&future](auto annotation) {
            return solver.ImproveFuture(std::move(annotation), *future);
        });
    }
    for (const auto& future : futureSuggestions) { // repeat because Z3 hates us...
        ApplyTransformer([this,&future](auto annotation) {
            return solver.ImproveFuture(std::move(annotation), *future);
        });
    }
}

void ProofGenerator::ReduceCurrentTime() {
    INFO(infoPrefix << "Minimizing time predicates." << INFO_SIZE << std::endl)
    ApplyTransformer([this](auto annotation){
        annotation = solver.ReducePast(std::move(annotation));
        return solver.ReduceFuture(std::move(annotation));
    });
}
