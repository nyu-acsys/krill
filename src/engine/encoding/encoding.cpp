#include "engine/encoding.hpp"

#include "internal.hpp"

using namespace plankton;


//
// EExpr
//

EExpr EExpr::operator!() const { return EExpr(repr->Negate()); }
EExpr EExpr::operator&&(const EExpr& other) const { return EExpr(repr->And(*other.repr)); }
EExpr EExpr::operator||(const EExpr& other) const { return EExpr(repr->Or(*other.repr)); }
EExpr EExpr::operator==(const EExpr& other) const { return EExpr(repr->Eq(*other.repr)); }
EExpr EExpr::operator!=(const EExpr& other) const { return EExpr(repr->Neq(*other.repr)); }
EExpr EExpr::operator<(const EExpr& other) const { return EExpr(repr->Lt(*other.repr)); }
EExpr EExpr::operator<=(const EExpr& other) const { return EExpr(repr->Leq(*other.repr)); }
EExpr EExpr::operator>(const EExpr& other) const { return EExpr(repr->Gt(*other.repr)); }
EExpr EExpr::operator>=(const EExpr& other) const { return EExpr(repr->Geq(*other.repr)); }
EExpr EExpr::operator>>(const EExpr& other) const { return EExpr(repr->Implies(*other.repr)); }
EExpr EExpr::operator()(const EExpr& other) const { return EExpr(repr->Contains(*other.repr)); }

const InternalExpr& EExpr::Repr() const { return *repr; }

EExpr::EExpr(std::unique_ptr<InternalExpr> repr_) : repr(std::move(repr_)) {
    assert(repr);
}

EExpr::EExpr(const EExpr& other) : EExpr(other.repr->Copy()) {
}

EExpr& EExpr::operator=(const EExpr& other) {
    repr = other.repr->Copy();
    return *this;
}

//
// Encoding
//

Encoding::Encoding() : internal(std::make_unique<Z3InternalStorage>()) {
    AsSolver(internal).add(AsExpr(TidSelf() > TidUnlocked()));
    AsSolver(internal).add(AsExpr(TidSome() > TidUnlocked()));
}

Encoding::Encoding(const std::string& smtLib) : internal(std::make_unique<Z3InternalStorage>()) {
    AsSolver(internal).from_string(smtLib.c_str());
    AsSolver(internal).add(AsExpr(TidSelf() > TidUnlocked()));
    AsSolver(internal).add(AsExpr(TidSome() > TidUnlocked()));
}

Encoding::Encoding(const Formula& premise) : Encoding() {
    AddPremise(premise);
}

Encoding::Encoding(const Formula& premise, const SolverConfig& config) : Encoding() {
    AddPremise(EncodeFormulaWithKnowledge(premise, config));
}

Encoding::Encoding(const FlowGraph& graph) : Encoding() {
    AddPremise(graph);
}

void Encoding::AddPremise(const EExpr& expr) {
    AsSolver(internal).add(AsExpr(expr));
}

void Encoding::AddPremise(const Formula& formula) {
    AddPremise(Encode(formula));
}

void Encoding::AddPremise(const NonSeparatingImplication& formula) {
    AddPremise(Encode(formula));
}

void Encoding::AddPremise(const ImplicationSet& formula) {
    AddPremise(Encode(formula));
}

void Encoding::AddPremise(const FlowGraph& graph) {
    AddPremise(Encode(graph));
}

void Encoding::AddCheck(const EExpr& expr, std::function<void(bool)> callback) {
    checks_premise.push_back(expr);
    checks_callback.push_back(std::move(callback));
}

void Encoding::Push() {
    AsSolver(internal).push();
}

void Encoding::Pop() {
    AsSolver(internal).pop();
}
