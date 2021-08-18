#include "engine/encoding.hpp"

using namespace plankton;


//
// EExpr
//

z3::expr Err(const z3::ast&) {
    throw std::logic_error("Internal error: cannot convert to z3 representation."); // better error handling
}
z3::expr Error(const z3::ast&, const z3::ast&) {
    throw std::logic_error("Internal error: cannot convert to z3 representation."); // better error handling
}

z3::expr And(const z3::expr& lhs, const z3::expr& rhs) { return lhs && rhs; }
z3::expr Or(const z3::expr& lhs, const z3::expr& rhs) { return lhs || rhs; }
z3::expr Less(const z3::expr& lhs, const z3::expr& rhs) { return lhs < rhs; }
z3::expr LessEq(const z3::expr& lhs, const z3::expr& rhs) { return lhs <= rhs; }
z3::expr Greater(const z3::expr& lhs, const z3::expr& rhs) { return lhs > rhs; }
z3::expr GreaterEq(const z3::expr& lhs, const z3::expr& rhs) { return lhs >= rhs; }
z3::expr Implies(const z3::expr& lhs, const z3::expr& rhs) { return z3::implies(lhs, rhs); }
z3::expr Eq1(const z3::expr& lhs, const z3::expr& rhs) { return lhs == rhs; }
z3::expr Neq1(const z3::expr& lhs, const z3::expr& rhs) { return lhs != rhs; }
z3::expr Member(const z3::func_decl& lhs, const z3::expr& rhs) { return lhs(rhs); }
z3::expr Eq2(const z3::func_decl& lhs, const z3::func_decl& rhs) {
    // TODO: is this too much of a hack?
    assert(lhs.arity() == 1);
    assert(rhs.arity() == 1);
    assert(z3::eq(lhs.domain(0), rhs.domain(0)));
    auto qv = lhs.ctx().constant("__op-qv", lhs.domain(0));
    return z3::forall(qv, lhs(qv) == rhs(qv));
}
z3::expr Neq2(const z3::func_decl& lhs, const z3::func_decl& rhs) { return !Eq2(lhs, rhs); }

template<auto A, auto B, auto C, auto D, typename T>
z3::expr DispatchVisitor(const T& variant, const T& other) {
    static struct VariantVisitor {
        z3::expr operator()(const z3::expr& lhs, const z3::expr& rhs) { return A(lhs, rhs); }
        z3::expr operator()(const z3::expr& lhs, const z3::func_decl& rhs) { return B(lhs, rhs); }
        z3::expr operator()(const z3::func_decl& lhs, const z3::expr& rhs) { return C(lhs, rhs); }
        z3::expr operator()(const z3::func_decl& lhs, const z3::func_decl& rhs) { return D(lhs, rhs); }
    } visitor;
    return std::visit(visitor, variant, other);
}

#define Handle(A, B, C, D) return EExpr(DispatchVisitor<A, B, C, D>(repr, other.repr));

EExpr EExpr::operator&&(const EExpr& other) const { Handle(And, Error, Error, Error) }
EExpr EExpr::operator||(const EExpr& other) const { Handle(Or, Error, Error, Error) }
EExpr EExpr::operator<(const EExpr& other) const { Handle(Less, Error, Error, Error) }
EExpr EExpr::operator<=(const EExpr& other) const { Handle(LessEq, Error, Error, Error) }
EExpr EExpr::operator>(const EExpr& other) const { Handle(Greater, Error, Error, Error) }
EExpr EExpr::operator>=(const EExpr& other) const { Handle(GreaterEq, Error, Error, Error) }
EExpr EExpr::operator>>(const EExpr& other) const { Handle(Implies, Error, Error, Error) }
EExpr EExpr::operator==(const EExpr& other) const { Handle(Eq1, Error, Error, Eq2) }
EExpr EExpr::operator!=(const EExpr& other) const { Handle(Neq1, Error, Error, Neq2) }
EExpr EExpr::operator()(const EExpr& other) const { Handle(Error, Error, Member, Error) }

EExpr EExpr::operator!() const {
    static struct VariantVisitor {
        z3::expr operator()(const z3::expr& expr) { return !expr; }
        z3::expr operator()(const z3::func_decl& decl) { return Err(decl); }
    } visitor;
    return EExpr(std::visit(visitor, repr));
}


//
// Encoding
//

Encoding::Encoding() : context(), solver(context) {}

Encoding::Encoding(const Formula& premise) : Encoding() {
    AddPremise(premise);
}

Encoding::Encoding(const FlowGraph& graph) : Encoding() {
    AddPremise(graph);
}

void Encoding::AddPremise(const EExpr& expr) {
    solver.add(expr.AsExpr());
}

void Encoding::AddPremise(const Formula& formula) {
    solver.add(Encode(formula).AsExpr());
}

void Encoding::AddPremise(const SeparatingImplication& formula) {
    solver.add(Encode(formula).AsExpr());
}

void Encoding::AddPremise(const FlowGraph& graph) {
    solver.add(Encode(graph).AsExpr());
}

void Encoding::AddCheck(const EExpr& expr, std::function<void(bool)> callback) {
    checks_premise.push_back(expr.AsExpr());
    checks_callback.push_back(std::move(callback));
}

void Encoding::Push() {
    solver.push();
}

void Encoding::Pop() {
    solver.pop();
}
