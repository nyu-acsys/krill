#include "cola/util.hpp"

using namespace cola;


struct CopyCommandVisitor final : public BaseVisitor {
	std::unique_ptr<Command> result;

	void visit(const Skip& /*node*/) override {
		this->result = std::make_unique<Skip>();
	}
	void visit(const Break& /*node*/) override {
		this->result = std::make_unique<Break>();
	}
	void visit(const Continue& /*node*/) override {
		this->result = std::make_unique<Continue>();
	}
	void visit(const Return& node) override {
		auto result = std::make_unique<Return>();
		for (const auto& expr : node.expressions) {
			result->expressions.push_back(cola::copy(*expr));
		}
		this->result = std::move(result);
	}
	void visit(const Assume& node) override {
		this->result = std::make_unique<Assume>(cola::copy(*node.expr));
	}
	void visit(const Assert& node) override {
		this->result = std::make_unique<Assert>(cola::copy(*node.expr));
	}
	void visit(const Malloc& node) override {
		this->result = std::make_unique<Malloc>(node.lhs);
	}
    void visit(const Assignment& node) override {
        this->result = std::make_unique<Assignment>(cola::copy(*node.lhs), cola::copy(*node.rhs));
    }
    void visit(const ParallelAssignment& node) override {
	    auto result = std::make_unique<ParallelAssignment>();
        for (const auto& lhs : node.lhs) result->lhs.push_back(cola::copy(*lhs));
        for (const auto& rhs : node.rhs) result->rhs.push_back(cola::copy(*rhs));
        this->result = std::move(result);
    }
	void visit(const Macro& node) override {
		auto result = std::make_unique<Macro>(node.decl);
		for (const auto& expr : node.lhs) {
			result->lhs.push_back(expr.get());
		}
		for (const auto& arg : node.args) {
			result->args.push_back(cola::copy(*arg));
		}
		this->result = std::move(result);
	}
	void visit(const CompareAndSwap& node) override {
		auto result = std::make_unique<CompareAndSwap>();
		for (const auto& [dst, cmp, src] : node.elems) {
			result->elems.push_back({ cola::copy(*dst), cola::copy(*cmp), cola::copy(*src),  });
		}
		this->result = std::move(result);
	}
};

std::unique_ptr<Command> cola::copy(const Command& cmd) {
	CopyCommandVisitor visitor;
	cmd.accept(visitor);
	return std::move(visitor.result);
}
