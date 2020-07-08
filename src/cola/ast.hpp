#pragma once
#ifndef COLA_AST
#define COLA_AST


#include <memory>
#include <vector>
#include <map>
#include <string>
#include <cassert>
#include "cola/visitors.hpp"


namespace cola {

	struct AstNode {
		static std::size_t make_id() {
			static std::size_t MAX_ID = 0;
			return MAX_ID++;
		}
		const std::size_t id;
		AstNode() : id(make_id()) {}
		AstNode(const AstNode& other) = delete;
		virtual ~AstNode() = default;
		virtual void accept(NonConstVisitor& visitor) = 0;
		virtual void accept(Visitor& visitor) const = 0;
	};

	#define ACCEPT_COLA_VISITOR \
		virtual void accept(NonConstVisitor& visitor) override { visitor.visit(*this); } \
		virtual void accept(Visitor& visitor) const override { visitor.visit(*this); }


	/*--------------- types ---------------*/

	enum struct Sort {
		VOID, BOOL, DATA, PTR
	};

	struct Type {
		std::string name;
		Sort sort;
		std::map<std::string, std::reference_wrapper<const Type>> fields;
		Type(const Type& other) = delete;
		Type(std::string name_, Sort sort_) : name(name_), sort(sort_) {}
		const Type* field(std::string name) const {
			auto it = fields.find(name);
			if (it == fields.end()) {
				return nullptr;
			} else {
				return &(it->second.get());
			}
		}
		bool has_field(std::string name) const { return field(name) != nullptr; }
		bool operator==(const Type& other) const { return name == other.name; }
		bool operator!=(const Type& other) const { return name != other.name; }
		static const Type& void_type() { static const Type type = Type("void", Sort::VOID); return type; }
		static const Type& bool_type() { static const Type type = Type("bool", Sort::BOOL); return type; }
		static const Type& data_type() { static const Type type = Type("data_t", Sort::DATA); return type; }
		static const Type& null_type() { static const Type type = Type("nullptr", Sort::PTR); return type; }
	};

	inline bool assignable(const Type& to, const Type& from) { // TODO: remove static
		if (to.sort != Sort::PTR) {
			return to.sort == from.sort;
		} else {
			return (to != Type::null_type())
			    && (to == from || from == Type::null_type());
		}
	}

	inline bool comparable(const Type& type, const Type& other) {
		if (type == other) return true;
		else if (type.sort == Sort::PTR && other == Type::null_type()) return true;
		else if (other.sort == Sort::PTR && type == Type::null_type()) return true;
		else return false;
	}

	struct VariableDeclaration : public AstNode {
		std::string name;
		const Type& type;
		bool is_shared = false;
		VariableDeclaration(std::string name_, const Type& type_, bool shared_) : name(name_), type(type_), is_shared(shared_) {}
		VariableDeclaration(const VariableDeclaration&) = delete; // cautiously delete copy construction to prevent bad things from happening
		ACCEPT_COLA_VISITOR
	};


	/*--------------- functions/macros/programs ---------------*/

	struct Function : public AstNode {
		enum Kind { INTERFACE, MACRO };
		std::string name;
		Kind kind;
		std::vector<std::unique_ptr<VariableDeclaration>> args;
		std::vector<std::unique_ptr<VariableDeclaration>> returns;
		std::unique_ptr<Scope> body;
		Function(std::string name_, Kind kind_, std::unique_ptr<Scope> body_) : name(name_), kind(kind_), body(std::move(body_)) {
			assert(body);
		}
		ACCEPT_COLA_VISITOR
	};

	struct Program : public AstNode {
		std::string name;
		std::vector<std::unique_ptr<Type>> types;
		std::vector<std::unique_ptr<VariableDeclaration>> variables;
		std::unique_ptr<Function> initializer;
		std::vector<std::unique_ptr<Function>> functions;
		std::map<std::string, std::string> options;
		ACCEPT_COLA_VISITOR
	};


	/*--------------- expressions ---------------*/

	struct Expression : virtual public AstNode {
		virtual ~Expression() = default;
		virtual const Type& type() const = 0;
		Sort sort() const { return type().sort; }
	};

	struct SimpleExpression : public Expression {
	};

	struct BooleanValue : public SimpleExpression {
		bool value;
		BooleanValue(bool value_) : value(value_) {};
		const Type& type() const override { return Type::bool_type(); }
		ACCEPT_COLA_VISITOR
	};

	struct NullValue : public SimpleExpression {
		const Type& type() const override { return Type::null_type(); }
		ACCEPT_COLA_VISITOR
	};

	struct EmptyValue : public SimpleExpression {
		const Type& type() const override { return Type::data_type(); }
		ACCEPT_COLA_VISITOR
	};

	struct MaxValue : public SimpleExpression {
		const Type& type() const override { return Type::data_type(); }
		ACCEPT_COLA_VISITOR
	};

	struct MinValue : public SimpleExpression {
		const Type& type() const override { return Type::data_type(); }
		ACCEPT_COLA_VISITOR
	};

	struct NDetValue : public SimpleExpression {
		const Type& type() const override { return Type::data_type(); }
		ACCEPT_COLA_VISITOR
	};

	struct VariableExpression : public SimpleExpression {
		const VariableDeclaration& decl;
		VariableExpression(const VariableDeclaration& decl_) : decl(decl_) {}
		const Type& type() const override { return decl.type; }
		ACCEPT_COLA_VISITOR
	};

	struct NegatedExpression : public Expression {
		std::unique_ptr<Expression> expr;
		NegatedExpression(std::unique_ptr<Expression> expr_) : expr(std::move(expr_)) {
			assert(expr);
			assert(expr->sort() == Sort::BOOL);
		}
		const Type& type() const override { return Type::bool_type(); }
		ACCEPT_COLA_VISITOR
	};

	struct BinaryExpression : public Expression {
		enum struct Operator {
			EQ, NEQ, LEQ, LT, GEQ, GT, AND, OR
		};
		Operator op;
		std::unique_ptr<Expression> lhs;
		std::unique_ptr<Expression> rhs;
		BinaryExpression(Operator op_, std::unique_ptr<Expression> lhs_, std::unique_ptr<Expression> rhs_) : op(op_), lhs(std::move(lhs_)), rhs(std::move(rhs_)) {
			assert(lhs);
			assert(rhs);
			assert(lhs->sort() == rhs->sort());
		}
		const Type& type() const override { return Type::bool_type(); }
		ACCEPT_COLA_VISITOR
	};

	inline std::string toString(BinaryExpression::Operator op) {
		switch (op) {
			case BinaryExpression::Operator::EQ: return "==";
			case BinaryExpression::Operator::NEQ: return "!=";
			case BinaryExpression::Operator::LEQ: return "<=";
			case BinaryExpression::Operator::LT: return "<";
			case BinaryExpression::Operator::GEQ: return ">=";
			case BinaryExpression::Operator::GT: return ">";
			case BinaryExpression::Operator::AND: return "&&";
			case BinaryExpression::Operator::OR: return "||";
		}
	}

	struct Dereference : public Expression {
		std::unique_ptr<Expression> expr;
		std::string fieldname;
		Dereference(std::unique_ptr<Expression> expr_, std::string fieldname_) : expr(std::move(expr_)), fieldname(fieldname_) {
			assert(expr);
			assert(expr->type().field(fieldname) != nullptr);
		}
		const Type& type() const override {
			const Type* type = expr->type().field(fieldname);
			assert(type);
			return *type;
		}
		ACCEPT_COLA_VISITOR
	};


	/*--------------- statements ---------------*/

	struct Statement : virtual public AstNode {
		virtual ~Statement() = default;
	};

	struct Sequence : public Statement {
		std::unique_ptr<Statement> first;
		std::unique_ptr<Statement> second;
		Sequence(std::unique_ptr<Statement> first_, std::unique_ptr<Statement> second_) : first(std::move(first_)), second(std::move(second_)) {
			assert(first);
			assert(second);
		}
		ACCEPT_COLA_VISITOR
	};

	struct Scope : public Statement {
		std::vector<std::unique_ptr<VariableDeclaration>> variables;
		std::unique_ptr<Statement> body;
		Scope(std::unique_ptr<Statement> body_) : body(std::move(body_)) {
			assert(body);
		}
		ACCEPT_COLA_VISITOR
	};

	struct Atomic : public Statement {
		std::unique_ptr<Scope> body;
		Atomic(std::unique_ptr<Scope> body_) : body(std::move(body_)) {
			assert(body);
		}
		ACCEPT_COLA_VISITOR
	};

	struct Choice : public Statement {
		std::vector<std::unique_ptr<Scope>> branches;
		ACCEPT_COLA_VISITOR
	};

	struct IfThenElse : public Statement {
		std::unique_ptr<Expression> expr;
		std::unique_ptr<Scope> ifBranch;
		std::unique_ptr<Scope> elseBranch;
		IfThenElse(std::unique_ptr<Expression> expr_, std::unique_ptr<Scope> ifBranch_, std::unique_ptr<Scope> elseBranch_) : expr(std::move(expr_)), ifBranch(std::move(ifBranch_)), elseBranch(std::move(elseBranch_)) {
			assert(expr);
			assert(ifBranch);
			assert(elseBranch);
		}
		ACCEPT_COLA_VISITOR
	};

	struct Loop : public Statement {
		std::unique_ptr<Scope> body;
		Loop(std::unique_ptr<Scope> body_) : body(std::move(body_)) {
			assert(body);
		}
		ACCEPT_COLA_VISITOR
	};

	struct ConditionalLoop : public Statement {
		std::unique_ptr<Expression> expr;
		std::unique_ptr<Scope> body;
		ConditionalLoop(std::unique_ptr<Expression> expr_, std::unique_ptr<Scope> body_) : expr(std::move(expr_)), body(std::move(body_)) {
			assert(expr);
			assert(body);
		}
		virtual ~ConditionalLoop() = default;
	};

	struct While : ConditionalLoop {
		While(std::unique_ptr<Expression> expr_, std::unique_ptr<Scope> body_) : ConditionalLoop(std::move(expr_), std::move(body_)) {
		}
		ACCEPT_COLA_VISITOR
	};

	struct DoWhile : ConditionalLoop {
		DoWhile(std::unique_ptr<Expression> expr_, std::unique_ptr<Scope> body_) : ConditionalLoop(std::move(expr_), std::move(body_)) {
		}
		ACCEPT_COLA_VISITOR
	};


	/*--------------- commands ---------------*/

	struct Command : public Statement {
		virtual ~Command() = default;
	};

	struct Skip : public Command {
		ACCEPT_COLA_VISITOR
	};

	struct Break : public Command {
		ACCEPT_COLA_VISITOR
	};

	struct Continue : public Command {
		ACCEPT_COLA_VISITOR
	};

	struct Assume : public Command {
		std::unique_ptr<Expression> expr;
		Assume(std::unique_ptr<Expression> expr_) : expr(std::move(expr_)) {
			assert(expr);
		}
		ACCEPT_COLA_VISITOR
	};

	struct Assert : public Command {
		std::unique_ptr<Expression> expr;
		Assert(std::unique_ptr<Expression> expr_) : expr(std::move(expr_)) {
			assert(expr);
		}
		ACCEPT_COLA_VISITOR
	};

	struct Return : public Command {
		std::vector<std::unique_ptr<Expression>> expressions; // optional
		Return() {};
		ACCEPT_COLA_VISITOR
	};

	struct Malloc : public Command {
		const VariableDeclaration& lhs;
		Malloc(const VariableDeclaration& lhs_) : lhs(lhs_) {
			assert(lhs.type.sort == Sort::PTR);
		}
		ACCEPT_COLA_VISITOR
	};

	struct Assignment : public Command {
		std::unique_ptr<Expression> lhs;
		std::unique_ptr<Expression> rhs;
		Assignment(std::unique_ptr<Expression> lhs_, std::unique_ptr<Expression> rhs_) : lhs(std::move(lhs_)), rhs(std::move(rhs_)) {
			assert(lhs);
			assert(rhs);
			assert(assignable(lhs->type(), rhs->type()));
		}
		ACCEPT_COLA_VISITOR
	};

	struct Macro : public Command {
		std::vector<std::reference_wrapper<const VariableDeclaration>> lhs;
		const Function& decl;
		std::vector<std::unique_ptr<Expression>> args;
		Macro(const Function& decl_) : decl(decl_) {
			assert(decl.kind == Function::MACRO);
		}
		ACCEPT_COLA_VISITOR
	};


	/*--------------- CAS ---------------*/

	struct CompareAndSwap : public Command, public Expression {
		struct Triple {
			std::unique_ptr<Expression> dst;
			std::unique_ptr<Expression> cmp; // TODO: VariableExpression?
			std::unique_ptr<Expression> src; // TODO: VariableExpression?
			Triple(std::unique_ptr<Expression> dst_, std::unique_ptr<Expression> cmp_, std::unique_ptr<Expression> src_) : dst(std::move(dst_)), cmp(std::move(cmp_)), src(std::move(src_)) {
				assert(dst);
				assert(cmp);
				assert(src);
			}
		};
		std::vector<Triple> elems;
		const Type& type() const override { return Type::bool_type(); }
		ACCEPT_COLA_VISITOR
	};

} // namespace cola

#endif
