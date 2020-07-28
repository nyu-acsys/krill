#pragma once
#ifndef PLANKTON_VERIFY
#define PLANKTON_VERIFY


#include <deque>
#include <vector>
#include <memory>
#include <exception>
#include "cola/ast.hpp"
#include "heal/logic.hpp"
#include "heal/util.hpp"
#include "plankton/error.hpp"
#include "plankton/solver.hpp"


namespace plankton {

	struct Effect {
		std::unique_ptr<heal::ConjunctionFormula> precondition;
		std::unique_ptr<cola::Assignment> command;
		
		Effect(std::unique_ptr<heal::ConjunctionFormula> pre, std::unique_ptr<cola::Assignment> cmd);
	};

	struct RenamingInfo {
		std::vector<std::unique_ptr<cola::VariableDeclaration>> renamed_variables;
		std::map<const cola::VariableDeclaration*, const cola::VariableDeclaration*> variable2renamed;
		
		RenamingInfo() = default;
		RenamingInfo(const RenamingInfo& other) = delete;

		heal::transformer_t as_transformer();
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
			std::unique_ptr<heal::Annotation> current_annotation;
			std::deque<std::unique_ptr<Effect>> interference; // interference for current proof, with local variables renamed
			RenamingInfo interference_renaming_info;
			bool is_interference_saturated; // indicates whether a fixed point wrt. to the found interference is reached
			std::vector<std::unique_ptr<heal::Annotation>> breaking_annotations; // collects annotations breaking out of loops
			std::vector<std::pair<std::unique_ptr<heal::Annotation>, const cola::Return*>> returning_annotations; // collects returns and their pre annotation
			bool inside_atomic;
			std::unique_ptr<Solver> solver;

			void visit_interface_function(const cola::Function& function); // performs proof for given interface function
			void handle_loop(const cola::ConditionalLoop& loop, bool peelFirst=false); // uniformly handles While/DoWhile
			void check_pointer_accesses(const cola::Command& command); // ensures null is not dereferenced in command
			
			void extend_interference(const cola::Assignment& command); // calls extend_interferenceadds with renamed (current_annotation, command)
			void extend_interference(std::unique_ptr<Effect> effect); // adds effect to interference; updates is_interference_saturated
			void apply_interference(); // weakens current_annotation according to interference
			bool is_interference_free(const heal::ConjunctionFormula& formula);
			bool has_effect(const cola::Expression& assignee);
			bool has_effect(const heal::SimpleFormula& formula);
	};


	bool check_linearizability(const cola::Program& program);

} // namespace plankton

#endif