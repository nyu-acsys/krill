#include "plankton/util.hpp"

#include <sstream>
#include "cola/util.hpp"

using namespace cola;
using namespace plankton;


bool plankton::syntactically_equal(const Expression& expression, const Expression& other) {
	// TODO: find a better way to do this
	// NOTE: this relies on two distinct VariableDeclaration objects not having the same name
	std::stringstream expressionStream, otherStream;
	cola::print(expression, expressionStream);
	cola::print(other, otherStream);
	return expressionStream.str() == otherStream.str();
}


template<typename F, typename X>
std::pair<bool, const F*> plankton::is_of_type(const Formula& formula) {
	auto result = dynamic_cast<const F*>(&formula);
	if (result) {
		return std::make_pair(true, result);
	}
	return std::make_pair(false, nullptr);
}

template<> std::pair<bool, const AxiomConjunctionFormula*> plankton::is_of_type<AxiomConjunctionFormula>(const Formula& formula);
template<> std::pair<bool, const ImplicationFormula*> plankton::is_of_type<ImplicationFormula>(const Formula& formula);
template<> std::pair<bool, const ConjunctionFormula*> plankton::is_of_type<ConjunctionFormula>(const Formula& formula);
template<> std::pair<bool, const NegatedAxiom*> plankton::is_of_type<NegatedAxiom>(const Formula& formula);
template<> std::pair<bool, const ExpressionAxiom*> plankton::is_of_type<ExpressionAxiom>(const Formula& formula);
template<> std::pair<bool, const OwnershipAxiom*> plankton::is_of_type<OwnershipAxiom>(const Formula& formula);
template<> std::pair<bool, const LogicallyContainedAxiom*> plankton::is_of_type<LogicallyContainedAxiom>(const Formula& formula);
template<> std::pair<bool, const KeysetContainsAxiom*> plankton::is_of_type<KeysetContainsAxiom>(const Formula& formula);
template<> std::pair<bool, const FlowAxiom*> plankton::is_of_type<FlowAxiom>(const Formula& formula);
template<> std::pair<bool, const ObligationAxiom*> plankton::is_of_type<ObligationAxiom>(const Formula& formula);
template<> std::pair<bool, const FulfillmentAxiom*> plankton::is_of_type<FulfillmentAxiom>(const Formula& formula);
template<> std::pair<bool, const PastPredicate*> plankton::is_of_type<PastPredicate>(const Formula& formula);
template<> std::pair<bool, const FuturePredicate*> plankton::is_of_type<FuturePredicate>(const Formula& formula);
template<> std::pair<bool, const Annotation*> plankton::is_of_type<Annotation>(const Formula& formula);
