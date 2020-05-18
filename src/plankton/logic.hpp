#pragma once
#ifndef PLANKTON_LOGIC
#define PLANKTON_LOGIC


#include <memory>
#include <cassert>
#include "cola/ast.hpp"


namespace plankton {

	struct HistoryExpression final {
		std::unique_ptr<cola::Expression> condition;
		std::unique_ptr<cola::Expression> expression;
		
		HistoryExpression(std::unique_ptr<cola::Expression> cond, std::unique_ptr<cola::Expression> expr) : condition(std::move(cond)), expression(std::move(expr)) {
			assert(condition);
			assert(expression);
		}
	};

	struct FutureExpression final {
		std::unique_ptr<cola::Expression> condition;
		const cola::Command& command;
		
		FutureExpression(std::unique_ptr<cola::Expression> cond, const cola::Command& cmd) : condition(std::move(cond)), command(cmd) {
			assert(condition);
		}
	};


	template<typename T>
	using Conjunction = std::vector<T>;


	struct Formula {
		Conjunction<std::unique_ptr<cola::Expression>> present;
		Conjunction<HistoryExpression> history;
		Conjunction<FutureExpression> future;
		
		void add_conjuncts(std::unique_ptr<cola::Expression> expression);
		Formula copy() const;

		static Formula make_true();
		static Formula make_false();
		static Formula make_from_expression(const cola::Expression& expression);
	};


	bool implies(const Formula& formula, const cola::Expression& implied);
	bool implies(const Formula& formula, const Formula& implied);
	bool is_equal(const Formula& formula, const Formula& other);
	Formula unify(const Formula& formula, const Formula& other); // creates a formula that implies the passed formulas
	Formula unify(const std::vector<Formula>& formulas); // creates a formula that implies the passed formulas

} // namespace plankton

#endif