#include "engine/proof.hpp"

#include "programs/util.hpp"
#include "util/shortcuts.hpp"

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
        : program(program), solver(program, config), insideAtomic(false), debugFuture(MakeDebugFuture(program)) {}

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

void ProofGenerator::ApplyTransformer(
        const std::function<std::unique_ptr<Annotation>(std::unique_ptr<Annotation>)>& transformer) {
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
    auto result = solver.AddInterference(std::move(newInterference));
    newInterference.clear();
    return result;
}

void ProofGenerator::MakeInterferenceStable(const Statement& after) {
    if (insideAtomic) return;
    if (current.empty()) return;
    if (plankton::IsRightMover(after)) return;
    ApplyTransformer([this](auto annotation){
        return solver.MakeInterferenceStable(std::move(annotation));
    });
}
