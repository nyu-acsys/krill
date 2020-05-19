#include "cola/transform.hpp"
#include "cola/util.hpp"
#include <set>
#include <map>

using namespace cola;


struct NameClashResolver : public BaseNonConstVisitor {
	bool changed = false;
	std::set<std::string> names;
	std::map<std::string, Function*> name2func;

	void handle(Program& program) {
		changed = false;
		program.accept(*this);
	}

	void visit(VariableDeclaration& node) {
		while (names.count(node.name) != 0) {
			node.name = "_" + node.name;
			changed = true;
		}
		names.insert(node.name);
	}

	void visit(Function& node) {
		for (auto& decl : node.args) {
			decl->accept(*this);
		}
		for (auto& decl : node.returns) {
			decl->accept(*this);
		}
		node.body->accept(*this);
	}
	void visit(Program& node) {
		for (auto& func : node.functions) {
			name2func[func->name] = func.get();
		}
		for (auto& decl : node.variables) {
			decl->accept(*this);
		}
		for (auto& func : node.functions) {
			if (func->kind == Function::Kind::INTERFACE) {
				names.clear();
				func->accept(*this);
			}
		}
	}

	void visit(Sequence& node) {
		node.first->accept(*this);
		node.second->accept(*this);
	}
	void visit(Scope& node) {
		for (auto& decl : node.variables) {
			decl->accept(*this);
		}
		node.body->accept(*this);
	}
	void visit(Atomic& node) {
		node.body->accept(*this);
	}
	void visit(Choice& node) {
		for (auto& branch : node.branches) {
			branch->accept(*this);
		}
	}
	void visit(IfThenElse& node) {
		node.ifBranch->accept(*this);
		node.elseBranch->accept(*this);
	}
	void visit(Loop& node) {
		node.body->accept(*this);
	}
	void visit(While& node) {
		node.body->accept(*this);
	}
	void visit(DoWhile& node) {
		node.body->accept(*this);
	}

	void visit(Macro& node) {
		name2func[node.decl.name]->accept(*this);
	}
	void visit(Skip& /*node*/) { /* do nothing */ }
	void visit(Break& /*node*/) { /* do nothing */ }
	void visit(Continue& /*node*/) { /* do nothing */ }
	void visit(Assume& /*node*/) { /* do nothing */ }
	void visit(Assert& /*node*/) { /* do nothing */ }
	void visit(Return& /*node*/) { /* do nothing */ }
	void visit(Malloc& /*node*/) { /* do nothing */ }
	void visit(Assignment& /*node*/) { /* do nothing */ }
	void visit(CompareAndSwap& /*node*/) { /* do nothing */ }
};

void cola::rename_variables(Program& program) {
	NameClashResolver visitor;
	std::size_t failsafe_counter = 0;
	do {
		visitor.handle(program);

		++failsafe_counter;
		if (failsafe_counter > 20) {
			throw TransformationError("Renaming variables to avoid name clashes failed.");
		}
	} while (visitor.changed);
}
