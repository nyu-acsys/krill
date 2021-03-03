#include "prover/backend/z3encoder.hpp"

using namespace cola;
using namespace heal;
using namespace prover;


Z3Expr::Z3Expr(z3::expr expr) : expr(std::move(expr)) {
}

Z3Expr::Z3Expr(Term term) : Z3Expr(*term.repr) {
}

Z3Expr::Z3Expr(Symbol symbol) : Z3Expr(*symbol.repr) {
}

template<typename T>
Z3Expr Downcast(const T& obj) {
	if (auto expr = dynamic_cast<const Z3Expr*>(&obj)) {
		return *expr;
	}
	throw Z3DowncastError(obj);
}

Z3Expr::Z3Expr(const EncodedTerm& term) : Z3Expr(Downcast(term)) {
}

Z3Expr::Z3Expr(const EncodedSymbol& symbol) : Z3Expr(Downcast(symbol)) {
}


template<typename T>
inline bool Equals(const Z3Expr& expr, const T& obj) {
	if (auto other = dynamic_cast<const Z3Expr*>(&obj)) {
		return expr == *other; // TODO: return z3::eq(expr, *other); ??
	}
	return false;
}

bool Z3Expr::operator==(const EncodedTerm& other) const {
	return Equals(*this, other);
}

bool Z3Expr::operator==(const EncodedSymbol& other) const {
	return Equals(*this, other);
}

bool Z3Expr::operator==(const Z3Expr& other) const {
	return expr == other.expr;
}

void Z3Expr::Print(std::ostream& stream) const {
	stream << expr;
}


std::shared_ptr<EncodedTerm> Z3Expr::Negate() const {
	return std::make_shared<Z3Expr>(Neg());
}

std::shared_ptr<EncodedTerm> Z3Expr::And(std::shared_ptr<EncodedTerm> term) const {
	return std::make_shared<Z3Expr>(And(Downcast(*term)));
}

std::shared_ptr<EncodedTerm> Z3Expr::Or(std::shared_ptr<EncodedTerm> term) const {
	return std::make_shared<Z3Expr>(Or(Downcast(*term)));
}

std::shared_ptr<EncodedTerm> Z3Expr::XOr(std::shared_ptr<EncodedTerm> term) const {
	return std::make_shared<Z3Expr>(XOr(Downcast(*term)));
}

std::shared_ptr<EncodedTerm> Z3Expr::Implies(std::shared_ptr<EncodedTerm> term) const {
	return std::make_shared<Z3Expr>(Implies(Downcast(*term)));
}

std::shared_ptr<EncodedTerm> Z3Expr::Iff(std::shared_ptr<EncodedTerm> term) const {
	return std::make_shared<Z3Expr>(Iff(Downcast(*term)));
}

std::shared_ptr<EncodedTerm> Z3Expr::Equal(std::shared_ptr<EncodedTerm> term) const {
	return std::make_shared<Z3Expr>(Equal(Downcast(*term)));
}

std::shared_ptr<EncodedTerm> Z3Expr::Distinct(std::shared_ptr<EncodedTerm> term) const {
	return std::make_shared<Z3Expr>(Distinct(Downcast(*term)));
}


Z3Expr Z3Expr::Neg() const {
	return Z3Expr(!expr);
}

Z3Expr Z3Expr::And(Z3Expr term) const {
	return Z3Expr(expr && term.expr);
}

Z3Expr Z3Expr::Or(Z3Expr term) const {
	return Z3Expr(expr || term.expr);
}

Z3Expr Z3Expr::XOr(Z3Expr term) const {
	return Distinct(term); // TODO: correct?
}

Z3Expr Z3Expr::Implies(Z3Expr term) const {
	return Z3Expr(z3::implies(expr, term.expr));
}

Z3Expr Z3Expr::Iff(Z3Expr term) const {
	return Z3Expr(expr == term.expr);
}

Z3Expr Z3Expr::Equal(Z3Expr term) const {
	return Z3Expr(expr == term.expr);
}

Z3Expr Z3Expr::Distinct(Z3Expr term) const {
	return Z3Expr(expr != term.expr);
}
