#include "engine/solver.hpp"

#include "programs/util.hpp"

using namespace plankton;


HeapEffect::HeapEffect(std::unique_ptr<SharedMemoryCore> before, std::unique_ptr<SharedMemoryCore> after,
                       std::unique_ptr<Formula> ctx) : pre(std::move(before)), post(std::move(after)),
                                                       context(std::move(ctx)) {
    assert(pre);
    assert(post);
    assert(pre->node->Decl() == post->node->Decl());
    assert(context);
}

std::ostream& plankton::operator<<(std::ostream& out, const HeapEffect& object) {
    out << "[ " << *object.pre << " ~~> " << *object.post << " | " << *object.context << " ]";
    return out;
}

PostImage::PostImage() = default;

PostImage::PostImage(std::unique_ptr<Annotation> post) {
    assert(post);
    annotations.push_back(std::move(post));
}

PostImage::PostImage(std::deque<std::unique_ptr<Annotation>> posts) : annotations(std::move(posts)) {
    assert(plankton::AllNonNull(annotations));
}

PostImage::PostImage(std::unique_ptr<Annotation> post, std::deque<std::unique_ptr<HeapEffect>> effects) : effects(std::move(effects)) {
    assert(post);
    annotations.push_back(std::move(post));
    assert(plankton::AllNonNull(effects));
}

PostImage::PostImage(std::deque<std::unique_ptr<Annotation>> posts, std::deque<std::unique_ptr<HeapEffect>> effects)
        : annotations(std::move(posts)), effects(std::move(effects)) {
    assert(plankton::AllNonNull(annotations));
    assert(plankton::AllNonNull(effects));
}


struct AssumptionChecker : DefaultProgramVisitor {
    void Visit(const Malloc& object) override {
        if (!object.lhs->Decl().isShared) return;
        throw std::logic_error("Allocation to shared variable '" + object.lhs->Decl().name + "' not supported.");
    }
    
    template<typename T>
    inline void CheckShared(const T& object) {
        for (const auto& var : object.lhs) {
            if (!var->Decl().isShared) continue;
            throw std::logic_error("Assignment to shared variable '" + var->Decl().name + "' not supported.");
        }
    }
    
    template<typename T>
    inline void CheckDuplicates(const T& object) {
        for (auto it = object.lhs.begin(); it != object.lhs.end(); ++it) {
            auto exprString = plankton::ToString(**it);
            for (auto ot = std::next(it); ot != object.lhs.end(); ++ot) {
                if (exprString != plankton::ToString(**ot)) continue;
                throw std::logic_error("Malformed assignment: multiple occurrences of '" + exprString + "' " +
                "at left-hand side of '" + plankton::ToString(object) + "'.");
            }
        }
    }
    
    void Visit(const VariableAssignment& object) override { CheckShared(object); CheckDuplicates(object); }
    void Visit(const MemoryWrite& object) override { CheckDuplicates(object); }
    void Visit(const Macro& object) override {
        CheckShared(object);
        CheckDuplicates(object);
        if (object.function.get().kind == Function::MACRO) return;
        throw std::logic_error("Cannot call non-macro function '" + object.function.get().name + "'."); // TODO: better error handling
    }
};

Solver::Solver(const Program& program, const SolverConfig& config) : config(config), dataFlow(program) {
    // sanity check
    AssumptionChecker checker;
    program.Accept(checker);
}
