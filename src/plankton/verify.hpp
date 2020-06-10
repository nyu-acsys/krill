#pragma once
#ifndef PLANKTON_VERIFY
#define PLANKTON_VERIFY


#include <deque>
#include <vector>
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
		std::unique_ptr<Formula> precondition;
		std::unique_ptr<cola::Assignment> command;
		Effect(std::unique_ptr<Formula> pre, std::unique_ptr<cola::Assignment> cmd) : precondition(std::move(pre)), command(std::move(cmd)) {}
	};

	struct RenamingInfo {
		std::vector<std::unique_ptr<cola::VariableDeclaration>> renamed_variables;
		std::map<const cola::VariableDeclaration*, const cola::VariableDeclaration*> variable2renamed;
		RenamingInfo() = default;
		RenamingInfo(const RenamingInfo& other) = delete;
		const cola::VariableDeclaration& rename(const cola::VariableDeclaration& decl);
	};


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
			std::unique_ptr<Annotation> current_annotation;
			std::deque<std::unique_ptr<Effect>> interference; // interference for current proof, with local variables renamed
			RenamingInfo interference_renaming_info;
			bool is_interference_saturated; // indicates whether a fixed point wrt. to the found interference is reached
			std::vector<std::unique_ptr<Annotation>> breaking_annotations; // collects annotations breaking out of loops
			std::vector<std::unique_ptr<Annotation>> returning_annotations; // collects annotations breaking out of loops
			bool inside_atomic;

			void visit_interface_function(const cola::Function& function); // performs proof for given interface function
			void visit_macro_function(const cola::Function& function); // performs subproof for given macro function
			void handle_loop(const cola::ConditionalLoop& loop, bool peelFirst=false); // uniformly handles While/DoWhile
			
			void extend_interference(const cola::Assignment& command); // calls extend_interferenceadds with renamed (current_annotation, command)
			void extend_interference(std::unique_ptr<Effect> effect); // adds effect to interference; updates is_interference_saturated
			void apply_interference(); // weakens current_annotation according to interference
			bool is_interference_free(const Formula& formula);
			bool has_effect(const cola::Expression& assignee);
			
			void check_invariant_stability(const cola::Assignment& command);
			void check_pointer_accesses(const cola::Expression& expr);
			// void extend_current_annotation(std::unique_ptr<cola::Expression> expr); // adds (and deduced) new knowlwedge; guarantees interference freedom ouside atomic blocks
			// void prune_current_annotation(const Expression& expr); // removes knowlwedge involving the given expr
	};


	bool check_linearizability(const cola::Program& program);

} // namespace plankton

#endif