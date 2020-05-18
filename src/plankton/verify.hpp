#pragma once
#ifndef PLANKTON_VERIFY
#define PLANKTON_VERIFY


#include <memory>
#include <exception>
#include "cola/ast.hpp"
#include "plankton/logic.hpp"


namespace plankton {

	struct VerificationError : public std::exception {
		const std::string cause;
		VerificationError(std::string cause_) : cause(std::move(cause_)) {}
		virtual const char* what() const noexcept { return cause.c_str(); }
	};

	struct UnsupportedConstructError : public VerificationError {
		UnsupportedConstructError(std::string construct);
	};

	struct AssertionError : public VerificationError {
		AssertionError(const cola::Assert& cmd);
	};


	struct Effect {
		std::unique_ptr<cola::Expression> precondition;
		const cola::Command& command;
		Effect(std::unique_ptr<cola::Expression> pre, const cola::Command& cmd) : precondition(std::move(pre)), command(cmd) {}
	};

	using Interference = std::vector<Effect>;


	class Verifier final : public cola::BaseVisitor {
		public:
			void visit(const cola::Sequence& node) override;
			void visit(const cola::Scope& node) override;
			void visit(const cola::Atomic& node) override;
			void visit(const cola::Choice& node) override;
			void visit(const cola::IfThenElse& node) override;
			void visit(const cola::Loop& node) override;
			void visit(const cola::While& node) override;
			void visit(const cola::DoWhile& node) override;
			void visit(const cola::Skip& node) override;
			void visit(const cola::Break& node) override;
			void visit(const cola::Continue& node) override;
			void visit(const cola::Assume& node) override;
			void visit(const cola::Assert& node) override;
			void visit(const cola::Return& node) override;
			void visit(const cola::Malloc& node) override;
			void visit(const cola::Assignment& node) override;
			void visit(const cola::Macro& node) override;
			void visit(const cola::CompareAndSwap& node) override;
			void visit(const cola::Program& node) override;

		private:
			Formula current_annotation;
			Interference interference; // interference for current proof
			bool is_interference_saturated; // indicates whether a fixed point wrt. to the found interference is reached
			std::vector<Formula> breaking_annotations; // collects annotations breaking out of loops
			std::vector<Formula> returning_annotations; // collects annotations breaking out of loops

			void visit_interface_function(const cola::Function& function); // performs proof for given interface function
			void visit_macro_function(const cola::Function& function); // performs subproof for given macro function
			void handle_loop(const cola::ConditionalLoop& loop, bool peelFirst=false); // uniformly handles While/DoWhile
			void handle_assignment(const cola::Expression& lhs, const cola::Expression& rhs); // does not deal with interference
			
			void extend_interference(Effect effect); // adds given effect to interference; updates is_interference_saturated
			void apply_interference(); // weakens current_annotation according to interference
			bool has_effect(const cola::Expression& assignee);
	};


	bool check_linearizability(const cola::Program& program);

} // namespace plankton

#endif