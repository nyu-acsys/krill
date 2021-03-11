#include "heal/util.hpp"

#include "heal/symbolic.hpp"

using namespace cola;
using namespace heal;


static constexpr bool SIMPLIFY_CONJUNCTION = true; // TODO: have a configuration file for this

std::unique_ptr<Formula> heal::Conjoin(std::unique_ptr<Formula> formula, std::unique_ptr<Formula> other) {
    auto result = heal::MakeConjunction(std::move(formula), std::move(other));
    if (!SIMPLIFY_CONJUNCTION) return result;
    return heal::Simplify(std::move(result));
}

std::unique_ptr<Annotation> heal::Conjoin(std::unique_ptr<Annotation> annotation, std::unique_ptr<Annotation> other) {
    other = heal::RenameSymbolicVariables(std::move(other), *annotation);
    annotation->now = heal::Conjoin(std::move(annotation->now), std::move(other->now));
    for (auto& pred : other->time) {
        annotation->time.push_back(std::move(pred));
    }
    return annotation;
}
