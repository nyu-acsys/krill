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
		std::string mkCause(const V& visitor, std::string name, std::string arg) {
			std::stringstream msg;
			msg << "Unexpected invocation of member 'visit' in derived class of '" << name << "': " << std::endl;
			msg << "  - called object typename is '" << typeid(visitor).name() << "' " << std::endl;
			msg << "  - parameter typename is '" << arg << "' ";
			return msg.str();
		}
		template<typename V>
		VisitorNotImplementedError(const V& visitor, std::string name, std::string arg) : cause(mkCause(visitor, name, arg)) {}
		virtual const char* what() const noexcept { return cause.c_str(); }
	};


	struct BaseVisitor : public Visitor {
		virtual void visit(const VariableDeclaration& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const VariableDeclaration&"); }
		virtual void visit(const Expression& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const Expression&"); }
		virtual void visit(const BooleanValue& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const BooleanValue&"); }
		virtual void visit(const NullValue& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const NullValue&"); }
		virtual void visit(const EmptyValue& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const EmptyValue&"); }
		virtual void visit(const MaxValue& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const MaxValue&"); }
		virtual void visit(const MinValue& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const MinValue&"); }
		virtual void visit(const NDetValue& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const NDetValue&"); }
		virtual void visit(const VariableExpression& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const VariableExpression&"); }
		virtual void visit(const NegatedExpression& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const NegatedExpression&"); }
		virtual void visit(const BinaryExpression& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const BinaryExpression&"); }
		virtual void visit(const Dereference& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const Dereference&"); }
		virtual void visit(const Sequence& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const Sequence&"); }
		virtual void visit(const Scope& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const Scope&"); }
		virtual void visit(const Atomic& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const Atomic&"); }
		virtual void visit(const Choice& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const Choice&"); }
		virtual void visit(const IfThenElse& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const IfThenElse&"); }
		virtual void visit(const Loop& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const Loop&"); }
		virtual void visit(const While& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const While&"); }
		virtual void visit(const DoWhile& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const DoWhile&"); }
		virtual void visit(const Skip& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const Skip&"); }
		virtual void visit(const Break& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const Break&"); }
		virtual void visit(const Continue& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const Continue&"); }
		virtual void visit(const Assume& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const Assume&"); }
		virtual void visit(const Assert& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const Assert&"); }
		virtual void visit(const Return& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const Return&"); }
		virtual void visit(const Malloc& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const Malloc&"); }
		virtual void visit(const Assignment& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const Assignment&"); }
		virtual void visit(const Macro& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const Macro&"); }
		virtual void visit(const CompareAndSwap& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const CompareAndSwap&"); }
		virtual void visit(const Function& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const Function&"); }
		virtual void visit(const Program& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseVisitor", "const Program&"); }

		virtual ~BaseVisitor() = default;
		protected: BaseVisitor(){}
	};

	struct BaseNonConstVisitor : public NonConstVisitor {
		virtual void visit(VariableDeclaration& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "VariableDeclaration&"); }
		virtual void visit(Expression& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "Expression&"); }
		virtual void visit(BooleanValue& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "BooleanValue&"); }
		virtual void visit(NullValue& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "NullValue&"); }
		virtual void visit(EmptyValue& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "EmptyValue&"); }
		virtual void visit(MaxValue& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "MaxValue&"); }
		virtual void visit(MinValue& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "MinValue&"); }
		virtual void visit(NDetValue& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "NDetValue&"); }
		virtual void visit(VariableExpression& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "VariableExpression&"); }
		virtual void visit(NegatedExpression& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "NegatedExpression&"); }
		virtual void visit(BinaryExpression& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "BinaryExpression&"); }
		virtual void visit(Dereference& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "Dereference&"); }
		virtual void visit(Sequence& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "Sequence&"); }
		virtual void visit(Scope& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "Scope&"); }
		virtual void visit(Atomic& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "Atomic&"); }
		virtual void visit(Choice& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "Choice&"); }
		virtual void visit(IfThenElse& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "IfThenElse&"); }
		virtual void visit(Loop& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "Loop&"); }
		virtual void visit(While& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "While&"); }
		virtual void visit(DoWhile& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "DoWhile&"); }
		virtual void visit(Skip& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "Skip&"); }
		virtual void visit(Break& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "Break&"); }
		virtual void visit(Continue& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "Continue&"); }
		virtual void visit(Assume& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "Assume&"); }
		virtual void visit(Assert& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "Assert&"); }
		virtual void visit(Return& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "Return&"); }
		virtual void visit(Malloc& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "Malloc&"); }
		virtual void visit(Assignment& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "Assignment&"); }
		virtual void visit(Macro& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "Macro&"); }
		virtual void visit(CompareAndSwap& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "CompareAndSwap&"); }
		virtual void visit(Function& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "Function&"); }
		virtual void visit(Program& /*node*/) override { throw VisitorNotImplementedError(*this, "BaseNonConstVisitor", "Program&"); }

		virtual ~BaseNonConstVisitor() = default;
		protected: BaseNonConstVisitor(){}
	};

} // namespace cola

#endif