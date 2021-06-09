#pragma once
#ifndef COLA_VISITORS
#define COLA_VISITORS

#include <exception>
#include <sstream>

namespace cola {

	struct AstNode;
	struct VariableDeclaration;
	struct Expression;
	struct BooleanValue;
	struct NullValue;
	struct EmptyValue;
	struct MaxValue;
	struct MinValue;
	struct NDetValue;
	struct VariableExpression;
	struct NegatedExpression;
	struct BinaryExpression;
	struct Dereference;
	struct Statement;
	struct Sequence;
	struct Scope;
	struct Atomic;
	struct Choice;
	struct IfThenElse;
	struct Loop;
	struct While;
	struct DoWhile;
	struct Command;
	struct Skip;
	struct Break;
	struct Continue;
	struct Assume;
	struct Assert;
	struct Return;
	struct Malloc;
	struct Assignment;
	struct Macro;
	struct CompareAndSwap;
	struct Function;
	struct Program;


	struct Visitor {
		virtual void visit(const VariableDeclaration& node) = 0;
		virtual void visit(const Expression& node) = 0;
		virtual void visit(const BooleanValue& node) = 0;
		virtual void visit(const NullValue& node) = 0;
		virtual void visit(const EmptyValue& node) = 0;
		virtual void visit(const MaxValue& node) = 0;
		virtual void visit(const MinValue& node) = 0;
		virtual void visit(const NDetValue& node) = 0;
		virtual void visit(const VariableExpression& node) = 0;
		virtual void visit(const NegatedExpression& node) = 0;
		virtual void visit(const BinaryExpression& node) = 0;
		virtual void visit(const Dereference& node) = 0;

        virtual void visit(const Sequence& node) = 0;
		virtual void visit(const Scope& node) = 0;
		virtual void visit(const Atomic& node) = 0;
		virtual void visit(const Choice& node) = 0;
		virtual void visit(const IfThenElse& node) = 0;
		virtual void visit(const Loop& node) = 0;
		virtual void visit(const While& node) = 0;
		virtual void visit(const DoWhile& node) = 0;
		virtual void visit(const Skip& node) = 0;
		virtual void visit(const Break& node) = 0;
		virtual void visit(const Continue& node) = 0;
		virtual void visit(const Assume& node) = 0;
		virtual void visit(const Assert& node) = 0;
		virtual void visit(const Return& node) = 0;
		virtual void visit(const Malloc& node) = 0;
		virtual void visit(const Assignment& node) = 0;
		virtual void visit(const Macro& node) = 0;
		virtual void visit(const CompareAndSwap& node) = 0;
		virtual void visit(const Function& node) = 0;
		virtual void visit(const Program& node) = 0;
	};

	struct NonConstVisitor {
		virtual void visit(VariableDeclaration& node) = 0;
		virtual void visit(Expression& node) = 0;
		virtual void visit(BooleanValue& node) = 0;
		virtual void visit(NullValue& node) = 0;
		virtual void visit(EmptyValue& node) = 0;
		virtual void visit(MaxValue& node) = 0;
		virtual void visit(MinValue& node) = 0;
		virtual void visit(NDetValue& node) = 0;
		virtual void visit(VariableExpression& node) = 0;
		virtual void visit(NegatedExpression& node) = 0;
		virtual void visit(BinaryExpression& node) = 0;
		virtual void visit(Dereference& node) = 0;
		virtual void visit(Sequence& node) = 0;
		virtual void visit(Scope& node) = 0;
		virtual void visit(Atomic& node) = 0;
		virtual void visit(Choice& node) = 0;
		virtual void visit(IfThenElse& node) = 0;
		virtual void visit(Loop& node) = 0;
		virtual void visit(While& node) = 0;
		virtual void visit(DoWhile& node) = 0;
		virtual void visit(Skip& node) = 0;
		virtual void visit(Break& node) = 0;
		virtual void visit(Continue& node) = 0;
		virtual void visit(Assume& node) = 0;
		virtual void visit(Assert& node) = 0;
		virtual void visit(Return& node) = 0;
		virtual void visit(Malloc& node) = 0;
		virtual void visit(Assignment& node) = 0;
		virtual void visit(Macro& node) = 0;
		virtual void visit(CompareAndSwap& node) = 0;
		virtual void visit(Function& node) = 0;
		virtual void visit(Program& node) = 0;
	};


	struct VisitorNotImplementedError : public std::exception {
		const std::string cause;
		template<typename V>
		std::string mkCause(const V& visitor, const std::string& name, const std::string& arg) {
			std::stringstream msg;
			msg << "Unexpected invocation of member 'visit' in derived class of '" << name << "': " << std::endl;
			msg << "  - called object typename is '" << typeid(visitor).name() << "' " << std::endl;
			msg << "  - parameter typename is '" << arg << "' ";
			return msg.str();
		}
		template<typename V>
		VisitorNotImplementedError(const V& visitor, std::string name, std::string arg) : cause(mkCause(visitor, name, arg)) {}
		[[nodiscard]] const char* what() const noexcept override { return cause.c_str(); }
	};


	struct BaseVisitor : public Visitor {
		void visit(const VariableDeclaration& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const VariableDeclaration&"); }
		void visit(const Expression& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const Expression&"); }
		void visit(const BooleanValue& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const BooleanValue&"); }
		void visit(const NullValue& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const NullValue&"); }
		void visit(const EmptyValue& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const EmptyValue&"); }
		void visit(const MaxValue& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const MaxValue&"); }
		void visit(const MinValue& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const MinValue&"); }
		void visit(const NDetValue& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const NDetValue&"); }
		void visit(const VariableExpression& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const VariableExpression&"); }
		void visit(const NegatedExpression& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const NegatedExpression&"); }
		void visit(const BinaryExpression& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const BinaryExpression&"); }
		void visit(const Dereference& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const Dereference&"); }
		void visit(const Sequence& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const Sequence&"); }
		void visit(const Scope& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const Scope&"); }
		void visit(const Atomic& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const Atomic&"); }
		void visit(const Choice& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const Choice&"); }
		void visit(const IfThenElse& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const IfThenElse&"); }
		void visit(const Loop& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const Loop&"); }
		void visit(const While& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const While&"); }
		void visit(const DoWhile& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const DoWhile&"); }
		void visit(const Skip& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const Skip&"); }
		void visit(const Break& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const Break&"); }
		void visit(const Continue& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const Continue&"); }
		void visit(const Assume& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const Assume&"); }
		void visit(const Assert& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const Assert&"); }
		void visit(const Return& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const Return&"); }
		void visit(const Malloc& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const Malloc&"); }
		void visit(const Assignment& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const Assignment&"); }
		void visit(const Macro& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const Macro&"); }
		void visit(const CompareAndSwap& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const CompareAndSwap&"); }
		void visit(const Function& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const Function&"); }
		void visit(const Program& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const Program&"); }

        virtual ~BaseVisitor() = default;
        protected: BaseVisitor() = default;
	};

	struct BaseNonConstVisitor : public NonConstVisitor {
		void visit(VariableDeclaration& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "VariableDeclaration&"); }
		void visit(Expression& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "Expression&"); }
		void visit(BooleanValue& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "BooleanValue&"); }
		void visit(NullValue& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "NullValue&"); }
		void visit(EmptyValue& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "EmptyValue&"); }
		void visit(MaxValue& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "MaxValue&"); }
		void visit(MinValue& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "MinValue&"); }
		void visit(NDetValue& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "NDetValue&"); }
		void visit(VariableExpression& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "VariableExpression&"); }
		void visit(NegatedExpression& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "NegatedExpression&"); }
		void visit(BinaryExpression& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "BinaryExpression&"); }
		void visit(Dereference& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "Dereference&"); }
		void visit(Sequence& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "Sequence&"); }
		void visit(Scope& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "Scope&"); }
		void visit(Atomic& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "Atomic&"); }
		void visit(Choice& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "Choice&"); }
		void visit(IfThenElse& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "IfThenElse&"); }
		void visit(Loop& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "Loop&"); }
		void visit(While& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "While&"); }
		void visit(DoWhile& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "DoWhile&"); }
		void visit(Skip& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "Skip&"); }
		void visit(Break& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "Break&"); }
		void visit(Continue& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "Continue&"); }
		void visit(Assume& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "Assume&"); }
		void visit(Assert& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "Assert&"); }
		void visit(Return& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "Return&"); }
		void visit(Malloc& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "Malloc&"); }
		void visit(Assignment& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "Assignment&"); }
		void visit(Macro& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "Macro&"); }
		void visit(CompareAndSwap& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "CompareAndSwap&"); }
		void visit(Function& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "Function&"); }
		void visit(Program& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "Program&"); }

		virtual ~BaseNonConstVisitor() = default;
		protected: BaseNonConstVisitor() = default;
	};

} // namespace cola

#endif