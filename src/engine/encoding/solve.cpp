#include "engine/encoding.hpp"

#include "util/shortcuts.hpp"

using namespace plankton;


//
// Z3 handling
//

struct PreferredMethodFailed : std::exception {
    [[nodiscard]] const char* what() const noexcept override {
        return "SMT solving failed: Z3 was unable to prove/disprove satisfiability; solving result was 'UNKNOWN'.";
    }
};

inline bool IsImplied(z3::solver& solver, const z3::expr& expr) {
    solver.push();
    solver.add(!expr);
    auto res = solver.check();
    solver.pop();
    switch (res) {
        case z3::unsat: return true;
        case z3::sat: return false;
        case z3::unknown: throw std::logic_error("Solving failed: Z3 returned z3::unknown."); // TODO: better error handling
    }
}

inline std::vector<bool> ComputeImpliedOneAtATime(z3::solver& solver, const std::deque<z3::expr>& expressions) {
    std::vector<bool> result;
    result.reserve(expressions.size());
    for (const auto& check : expressions) {
        result.push_back(IsImplied(solver, check));
    }
    return result;
}

inline std::vector<bool> ComputeImpliedInOneShot(z3::solver& solver, const std::deque<z3::expr>& expressions) {
    // prepare required vectors
    solver.push();
    auto& context = solver.ctx();
    z3::expr_vector variables(context);
    z3::expr_vector assumptions(context);
    z3::expr_vector consequences(context);

    // prepare engine
    for (unsigned int index = 0; index < expressions.size(); ++index) {
        std::string name = "__chk__" + std::to_string(index);
        auto var = context.bool_const(name.c_str());
        variables.push_back(var);
        solver.add(var == expressions.at(index));
    }

    // check
    auto answer = solver.consequences(assumptions, variables, consequences);
    solver.pop();

    // create result
    std::vector<bool> result(expressions.size(), false);
    switch (answer) {
        case z3::unknown:
            throw PreferredMethodFailed();
        
        case z3::unsat:
            result.flip();
            return result;
        
        case z3::sat:
            for (unsigned int index = 0; index < expressions.size(); ++index) {
                auto search = z3::implies(true, variables[(int) index]);
                bool implied = plankton::ContainsIf(consequences, [search](const auto& elem) {
                    return z3::eq(search, elem);
                });
                if (implied) result[index] = true;
            }
            return result;
    }
}

inline std::string GetZ3Version() {
    unsigned int versionMajor, versionMinor, versionBuild, versionRevision;
    Z3_get_version(&versionMajor, &versionMinor, &versionBuild, &versionRevision);
    return std::to_string(versionMajor) + "." + std::to_string(versionMinor) + "." + std::to_string(versionBuild);
}

struct MethodChooser {
    // TODO: identify working method beforehand (during construction)
    // static std::function<std::vector<bool>(z3::engine&, const z3::expr_vector&, bool*)> method = GetSolvingMethod();
    // return method(encoding);
    bool fallback = false;

    inline std::vector<bool> operator()(z3::solver& solver, const std::deque<z3::expr>& expressions) {
        if (fallback) return ComputeImpliedOneAtATime(solver, expressions);
        try {
            return ComputeImpliedInOneShot(solver, expressions);
        } catch (const PreferredMethodFailed& err) {
            std::cerr << "WARNING: solving failure with Z3's solver::consequences! "
                      << "This issue is known to happen for versions >4.8.7, your version is " << GetZ3Version()
                      << ". Using fallback, performance may degrade..." << std::endl;
            fallback = true;
            return ComputeImpliedOneAtATime(solver, expressions);
        }
    }
} solvingMethod;


//
// Encoding API
//

void Encoding::Check() {
    assert(checks_premise.size() == checks_callback.size());
    if (checks_premise.empty()) return;
    auto implied = solvingMethod(solver, checks_premise);
    for (std::size_t index = 0; index < implied.size(); ++index) {
        checks_callback.at(index)(implied.at(index));
    }
    checks_premise.clear();
    checks_callback.clear();
}

bool Encoding::ImpliesFalse() {
    return IsImplied(solver, context.bool_val(false));
}

bool Encoding::Implies(const EExpr& expr) {
    return IsImplied(solver, expr.AsExpr());
}

bool Encoding::Implies(const Formula& formula) {
    return IsImplied(solver, Encode(formula).AsExpr());
}

bool Encoding::Implies(const SeparatingImplication& formula) {
    return IsImplied(solver, Encode(formula).AsExpr());
}

std::set<const SymbolDeclaration*> Encoding::ComputeNonNull(std::set<const SymbolDeclaration*> symbols) {
    plankton::DiscardIf(symbols, [](auto* decl){ return decl->type.sort != Sort::PTR; });
    
    std::deque<z3::expr> expressions;
    for (const auto* elem : symbols) expressions.push_back(EncodeIsNonNull(*elem).AsExpr());
    auto implied = solvingMethod(solver, expressions);
    
    auto sym = symbols.begin();
    for (bool isNonNull : implied) {
        assert(sym != symbols.end());
        if (isNonNull) ++sym;
        else sym = symbols.erase(sym);
    }
    return symbols;
}
