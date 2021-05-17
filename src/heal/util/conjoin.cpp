#include "heal/util.hpp"

using namespace cola;
using namespace heal;


std::unique_ptr<SeparatingConjunction> heal::Conjoin(std::unique_ptr<Formula> formula, std::unique_ptr<Formula> other) {
    auto result = std::make_unique<SeparatingConjunction>();
    result->conjuncts.push_back(std::move(formula));
    result->conjuncts.push_back(std::move(other));
    heal::Simplify(*result);
    return result;
}

std::unique_ptr<Annotation> heal::Conjoin(std::unique_ptr<Annotation> annotation, std::unique_ptr<Annotation> other) {
    std::move(other->now->conjuncts.begin(), other->now->conjuncts.end(), std::back_inserter(annotation->now->conjuncts));
    std::move(other->time.begin(), other->time.end(), std::back_inserter(annotation->time));
    return annotation;
}
