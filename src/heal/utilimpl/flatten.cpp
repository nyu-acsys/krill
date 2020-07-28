#include "heal/util.hpp"

using namespace cola;
using namespace heal;


std::unique_ptr<AxiomConjunctionFormula> heal::flatten(std::unique_ptr<cola::Expression> expression) {
	auto result = std::make_unique<AxiomConjunctionFormula>();

	std::deque<std::unique_ptr<Expression>> worklist;
	worklist.push_back(std::move(expression));

	while (!worklist.empty()) {
		auto current = std::move(worklist.back());
		worklist.pop_back();

		if (auto binary = dynamic_cast<BinaryExpression*>(current.get())) {
			if (binary->op == BinaryExpression::Operator::AND) {
				worklist.push_back(std::move(binary->lhs));
				worklist.push_back(std::move(binary->rhs));
				continue;
			}
		}

		result->conjuncts.push_back(std::make_unique<ExpressionAxiom>(std::move(current)));
	}

	return result;
}
