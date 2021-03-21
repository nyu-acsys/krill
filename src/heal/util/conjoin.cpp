#include "heal/util.hpp"

using namespace cola;
using namespace heal;


static constexpr bool SIMPLIFY_CONJUNCTION = true; // TODO: have a configuration file for this

template<typename T>
std::unique_ptr<T> PostProcess(std::unique_ptr<T> ptr) {
    if (SIMPLIFY_CONJUNCTION) heal::Simplify(*ptr);
    return ptr;
}

std::unique_ptr<SeparatingConjunction> heal::Conjoin(std::unique_ptr<Formula> formula, std::unique_ptr<Formula> other) {
    return PostProcess(heal::MakeConjunction(std::move(formula), std::move(other)));
}

std::unique_ptr<FlatSeparatingConjunction> heal::Conjoin(std::unique_ptr<FlatFormula> formula, std::unique_ptr<FlatFormula> other) {
    return PostProcess(heal::MakeFlatConjunction(std::move(formula), std::move(other)));
}

std::unique_ptr<Annotation> Conjoin(std::unique_ptr<Annotation> annotation, std::unique_ptr<Annotation> other) {
    std::move(other->now->conjuncts.begin(), other->now->conjuncts.end(), std::back_inserter(annotation->now->conjuncts));
    std::move(other->time.begin(), other->time.end(), std::back_inserter(annotation->time));
    return PostProcess(std::move(annotation));
}
