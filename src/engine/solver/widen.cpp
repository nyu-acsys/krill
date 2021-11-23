#include "engine/solver.hpp"

#include "logics/util.hpp"
#include "engine/encoding.hpp"
#include "engine/util.hpp"

using namespace plankton;

struct ResourceKeeper : public LogicListener {
    std::set<const SymbolDeclaration*> referenced;
    std::deque<const Axiom*> keep;

    explicit ResourceKeeper(const Formula& formula) {
        auto vars = plankton::Collect<EqualsToAxiom>(formula);
        for (const auto* elem : vars) referenced.insert(&elem->Value());
    }

    void Enter(const LocalMemoryResource& object) override { keep.push_back(&object); }
    void Enter(const SharedMemoryCore& object) override { if (plankton::Membership(referenced, &object.node->Decl())) keep.push_back(&object); }
    void Enter(const EqualsToAxiom& object) override { keep.push_back(&object); }
    void Enter(const ObligationAxiom& object) override { keep.push_back(&object); }
    void Enter(const FulfillmentAxiom& object) override { keep.push_back(&object); }
};


std::unique_ptr<Annotation> Solver::Widen(std::unique_ptr<Annotation> annotation) const {
    auto result = std::make_unique<Annotation>();

    // resources
    ResourceKeeper keeper(*annotation->now);
    annotation->now->Accept(keeper);
    for (const auto* keep : keeper.keep) result->Conjoin(plankton::Copy(*keep));

    // past
    result->past = std::move(annotation->past);
    plankton::RemoveIf(result->past, [&keeper](const auto& elem){
        return !plankton::Membership(keeper.referenced, &elem->formula->node->Decl());
    });

    // future
    result->future = std::move(annotation->future);

    // stack
    Encoding encoding;
    encoding.AddPremise(encoding.EncodeFormulaWithKnowledge(*annotation->now, config));
    plankton::ExtendStack(*result, encoding, ExtensionPolicy::FAST);

    return result;
}