#include "cola/util.hpp"

#include <sstream>
#include <functional>
#include "cola/ast.hpp"

using namespace cola;

static const std::string TUPLE_LEFT = "<";
static const std::string TUPLE_RIGHT = ">";


struct PrintExpressionVisitor final : public BaseVisitor {
	std::ostream& stream;
	std::size_t precedence = 0;

	PrintExpressionVisitor(std::ostream& stream) : stream(stream) {}

	void visit(const CompareAndSwap& com) {
		std::stringstream dst, cmp, src;
		PrintExpressionVisitor dstVisitor(dst), cmpVisitor(cmp), srcVisitor(src);
		assert(!com.elems.empty());
		com.elems.at(0).dst->accept(dstVisitor);
		com.elems.at(0).cmp->accept(cmpVisitor);
		com.elems.at(0).src->accept(srcVisitor);
		for (std::size_t i = 1; i < com.elems.size(); i++) {
			dst << ", ";
			cmp << ", ";
			src << ", ";
			com.elems.at(i).dst->accept(dstVisitor);
			com.elems.at(i).cmp->accept(cmpVisitor);
			com.elems.at(i).src->accept(srcVisitor);
		}
		std::string dstResult = dst.str();
		std::string cmpResult = cmp.str();
		std::string srcResult = src.str();
		if (com.elems.size() > 1) {
			dstResult = TUPLE_LEFT + dstResult + TUPLE_RIGHT;
			cmpResult = TUPLE_LEFT + cmpResult + TUPLE_RIGHT;
			srcResult = TUPLE_LEFT + srcResult + TUPLE_RIGHT;
		}
		stream << "CAS(" << dstResult << ", " << cmpResult << ", " << srcResult << ")";
	}


	void set_precedence_op(BinaryExpression::Operator op) {
		switch (op) {
			case BinaryExpression::Operator::EQ: precedence = 1; break;
			case BinaryExpression::Operator::NEQ: precedence = 1; break;
			case BinaryExpression::Operator::LEQ: precedence = 1; break;
			case BinaryExpression::Operator::LT: precedence = 1; break;
			case BinaryExpression::Operator::GEQ: precedence = 1; break;
			case BinaryExpression::Operator::GT: precedence = 1; break;
			case BinaryExpression::Operator::AND: precedence = 0; break;
			case BinaryExpression::Operator::OR: precedence = 0; break;
		}
	}

	void set_precedence_neg() {
		precedence = 2;
	}

	void set_precedence_deref() {
		precedence = 3;
	}

	void set_precedence_immi() {
		precedence = 4;
	}


	void visit(const Expression& /*expr*/) {
		throw std::logic_error("Unexpected invocation (PrintVisitor:::visit(const Expression&))");
	}

	void visit(const BooleanValue& expr) {
		stream << (expr.value ? "true" : "false");
		set_precedence_immi();
	}

	void visit(const NullValue& /*expr*/) {
		stream << "NULL";
		set_precedence_immi();
	}

	void visit(const EmptyValue& /*expr*/) {
		stream << "EMPTY";
		set_precedence_immi();
	}

	void visit(const MaxValue& /*expr*/) {
		stream << "MAX_VAL";
		set_precedence_immi();
	}

	void visit(const MinValue& /*expr*/) {
		stream << "MIN_VAL";
		set_precedence_immi();
	}

	void visit(const NDetValue& /*expr*/) {
		stream << "*";
		set_precedence_immi();
	}

	void visit(const VariableExpression& expr) {
		stream << expr.decl.name;
		set_precedence_immi();
	}

	void print_sub_expression(const Expression& expr) {
		std::stringstream my_stream;
		PrintExpressionVisitor subprinter(my_stream);
		expr.accept(subprinter);
		std::string expr_string = my_stream.str();

		if (subprinter.precedence <= precedence) {
			stream << "(" << expr_string << ")";
		} else {
			stream << expr_string;
		}
	}

	void visit(const NegatedExpression& expr) {
		set_precedence_neg();
		stream << "!";
		print_sub_expression(*expr.expr);
	}

	void visit(const BinaryExpression& expr) {
		set_precedence_op(expr.op);
		print_sub_expression(*expr.lhs);
		stream << " " << toString(expr.op) << " ";
		print_sub_expression(*expr.rhs);
	}

	void visit(const Dereference& expr) {
		set_precedence_deref();
		print_sub_expression(*expr.expr);
		stream << "->" << expr.fieldname;
	}
};

struct PrintVisitor final : public BaseVisitor {
	std::ostream& stream;
	Indent indent;
	bool scope_sameline = false;
	PrintExpressionVisitor exprinter;

	PrintVisitor(std::ostream& stream) : stream(stream), indent(stream), exprinter(stream) {}

	std::string typenameof(const Type& type) {
		std::string name = type.name;
		if (type.sort == Sort::PTR) {
			name.pop_back(); // remove trainling * for pointer types
		}
		return name;
	}

	void print_class(const Type& type) {
		stream << std::endl << indent++ << "struct " << typenameof(type) << " {" << std::endl;
		for (const auto& pair : type.fields) {
			stream << indent << typenameof(pair.second.get())  << " " << pair.first << ";" << std::endl;
		}
		stream << --indent << "};" << std::endl;
	}

	template<class T, typename F>
	void print_elem_or_tuple(const std::vector<T>& elems, F elem2string, bool wrapTuple=true, std::string emptyString="") {
		if (elems.size() == 0) {
			stream << emptyString;

		} else if (elems.size() == 1) {
			stream << elem2string(elems.at(0));
		} else {
			if (wrapTuple) stream << TUPLE_LEFT;
			stream << elem2string(elems.at(0));
			for (std::size_t i = 1; i < elems.size(); ++i) {
				stream << ", ";
				stream << elem2string(elems.at(i));
			}
			if (wrapTuple) stream << TUPLE_RIGHT;
		}
	}


	void visit(const Program& program) {
		stream << "//" << std::endl;
		stream << "// BEGIN program \"" << program.name << "\"" << std::endl;
		stream << "//" << std::endl;

		for (const auto& type : program.types) {
			print_class(*type);
		}
		stream << std::endl;

		for (const auto& decl : program.variables) {
			decl->accept(*this);
			stream << ";" << std::endl;
		}

		program.initializer->accept(*this);
		for (const auto& function :  program.functions) {
			function->accept(*this);
		}

		stream << std::endl;
		stream << "//" << std::endl;
		stream << "// END program \"" << program.name << "\"" << std::endl;
		stream << "//" << std::endl;
	}

	void print_scope(const Scope& scope) {
		scope_sameline = true;
		scope.accept(*this);
	}

	void visit(const Function& function) {
		std::string modifier;

		// print modifier
		switch (function.kind) {
			case Function::INTERFACE: modifier = ""; break;
			case Function::MACRO: modifier = "inline "; break;
		}
		stream << std::endl << modifier;

		// print return type
		print_elem_or_tuple(function.return_types, [](auto& type){ return type.get().name; }, true, "void");

		// print name
		stream << " " << function.name << "(";

		// print arguments
		if (function.args.size() > 0) {
			function.args.at(0)->accept(*this);
			for (std::size_t i = 1; i < function.args.size(); i++) {
				stream << ", ";
				function.args.at(i)->accept(*this);
			}
		}
		stream << ")";

		// print body
		stream << " ";
		print_scope(*function.body);
		stream << std::endl;
	}

	void visit(const VariableDeclaration& decl) {
		// output without indent and delimiter
		stream << decl.type.name << " " << decl.name;
	}


	void visit(const Scope& scope) {
		bool sameline = scope_sameline;
		scope_sameline = false;
		if (!sameline) {
			stream << indent;
		}
		stream << "{" << std::endl;
		indent++;
		for (const auto& var : scope.variables) {
			stream << indent;
			var->accept(*this);
			stream << ";" << std::endl;
		}
		if (!scope.variables.empty()) {
			stream << std::endl;
		}
		scope.body->accept(*this);
		stream << --indent << "}";
		if (!sameline) {
			stream << std::endl;
		}

	}

	void visit(const Sequence& sequence) {
		sequence.first->accept(*this);
		sequence.second->accept(*this);
	}

	void visit(const Atomic& atomic) {
		stream << indent << "atomic ";
		print_scope(*atomic.body);
		stream << std::endl;
	}
	
	void visit(const Choice& choice) {
		stream << indent << "choose ";
		for (const auto& scope : choice.branches) {
			print_scope(*scope);
		}
		stream << std::endl;
	}
	
	void visit(const IfThenElse& ite) {
		stream << indent << "if (";
		ite.expr->accept(exprinter);
		stream << ") ";
		print_scope(*ite.ifBranch);
		if (ite.elseBranch) {
			stream << " else ";
			print_scope(*ite.elseBranch);
		}
		stream << std::endl;
	}

	void visit(const Loop& loop) {
		stream << indent << "loop ";
		print_scope(*loop.body);
		stream << std::endl;
	}

	void visit(const While& whl) {
		stream << indent << "while (";
		whl.expr->accept(exprinter);
		stream << ") ";
		print_scope(*whl.body);
		stream << std::endl;
	}

	void visit(const DoWhile& dwhl) {
		stream << indent << "do ";
		print_scope(*dwhl.body);
		stream << " while (";
		dwhl.expr->accept(exprinter);
		stream << ");";
		stream << std::endl;
	}

	void visit(const Skip& /*com*/) {
		stream << indent << "skip;" << std::endl;
	}

	void visit(const Break& /*com*/) {
		stream << indent << "break;" << std::endl;
	}

	void visit(const Continue& /*com*/) {
		stream << indent << "continue;" << std::endl;
	}

	void visit(const Assume& com) {
		stream << indent << "assume(";
		com.expr->accept(exprinter);
		stream << ");";
		stream << " // " << com.id;
		stream << std::endl;
	}

	void visit(const Assert& com) {
		stream << indent << "assert(";
		com.expr->accept(exprinter);
		stream << ");" << std::endl;
	}

	void visit(const Return& com) {
		stream << indent << "return";
		if (com.expressions.size() != 0) stream << " ";
		print_elem_or_tuple(com.expressions, [this](auto& expr){ expr->accept(exprinter); return ""; });
		stream << ";" << std::endl;
	}

	void visit(const Malloc& com) {
		stream << indent << com.lhs.name << " = malloc;" << std::endl;
	}

	void visit(const Assignment& com) {
		stream << indent;
		com.lhs->accept(exprinter);
		stream << " = ";
		com.rhs->accept(exprinter);
		stream << ";";
		stream << " // " << com.id;
		stream << std::endl;
	}

	void visit(const Macro& com) {
		stream << indent;
		print_elem_or_tuple(com.lhs, [](auto& var){ return var.get().name; });
		if (com.lhs.size() != 0) {
			stream << " = ";
		}
		stream << com.decl.name << "(";
		print_elem_or_tuple(com.args, [this](auto& expr){ expr->accept(exprinter); return ""; });
		stream << ");" << std::endl;
	}

	void visit(const CompareAndSwap& com) {
		stream << indent;
		com.accept(exprinter);
		stream << ";" << std::endl;
	}
};

void cola::print(const Program& program, std::ostream& stream) {
	PrintVisitor(stream).visit(program);
}

void cola::print(const Expression& expression, std::ostream& stream) {
	PrintExpressionVisitor visitor(stream);
	expression.accept(visitor);
}

void cola::print(const Command& command, std::ostream& stream) {
	PrintVisitor visitor(stream);
	command.accept(visitor);
}

void cola::print(const Statement& statement, std::ostream& stream) {
	PrintVisitor visitor(stream);
	statement.accept(visitor);
}
