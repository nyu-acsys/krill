#include "engine/solver.hpp"

#include "logics/util.hpp"
#include "engine/encoding.hpp"
#include "engine/flowgraph.hpp"
#include "engine/util.hpp"
#include "util/timer.hpp"

using namespace plankton;


inline std::unique_ptr<Annotation> AnnotationFromFormula(const Formula& formula) {
    auto result = std::make_unique<Annotation>();
    result->Conjoin(plankton::Copy(formula));
    plankton::Simplify(*result);
    return result;
}

struct FulfillmentFinder {
    const SolverConfig& config;
    SymbolFactory factory;
    Encoding encoding;
    std::set<const ObligationAxiom*> obligations;
    std::deque<std::unique_ptr<FulfillmentAxiom>> fulfillments;
    
    explicit FulfillmentFinder(const Annotation& annotation, const SolverConfig& config)
            : config(config), factory(annotation), encoding(*annotation.now, config),
              obligations(plankton::Collect<ObligationAxiom>(*annotation.now)) {
    }
    
    void Handle(const Formula& formula) {
        auto dummy = AnnotationFromFormula(formula);
        auto graph = plankton::MakePureHeapGraph(std::move(dummy), factory, config);
        encoding.Push();
        encoding.AddPremise(graph);
        for (const auto* obligation : obligations) {
            plankton::AddPureSpecificationCheck(graph, encoding, *obligation, [this](auto ful){
                if (ful.has_value()) fulfillments.push_back(std::make_unique<FulfillmentAxiom>(ful.value()));
            });
        }
        encoding.Check();
        encoding.Pop();
    }
};

[[nodiscard]] std::unique_ptr<Annotation> Solver::TryAddFulfillment(std::unique_ptr<Annotation> annotation) const {
    MEASURE("Solver::TryAddFulfillment")
    DEBUG("Solver::TryAddFulfillment for " << *annotation << std::endl)
    FulfillmentFinder finder(*annotation, config);
    finder.Handle(*annotation->now); // TODO: per SharedMemoryCore?
    for (const auto& past : annotation->past) finder.Handle(*past->formula);
    plankton::MoveInto(std::move(finder.fulfillments), annotation->now->conjuncts);
    return annotation;
}
