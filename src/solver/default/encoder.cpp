#include "encoder.hpp"

#include "z3.h"

using namespace engine;



struct Z3SolvingFailedError : std::exception {
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
        case z3::unknown: throw Z3SolvingFailedError();
    }
}

bool engine::ComputeImplied(z3::solver& solver, const z3::expr& premise, const z3::expr& conclusion) {
    return IsImplied(solver, z3::implies(premise, conclusion));
}

std::vector<bool> ComputeImpliedOneAtATime(z3::solver& solver, const z3::expr_vector& expressions, bool* isSolverUnsatisfiable) {
    if (isSolverUnsatisfiable) {
        *isSolverUnsatisfiable = IsImplied(solver, solver.ctx().bool_val(false));
    }
    std::vector<bool> result;
    for (const auto& expr : expressions) {
        result.push_back(IsImplied(solver, expr));
    }
    return result;
}

inline std::vector<bool> ComputeImpliedInOneShot(z3::solver& solver, const z3::expr_vector& expressions, bool* isSolverUnsatisfiable) {
    // prepare required vectors
    solver.push();
    z3::context& context = solver.ctx();
    z3::expr_vector variables(context);
    z3::expr_vector assumptions(context);
    z3::expr_vector consequences(context);

    // prepare engine
    for (unsigned int index = 0; index < expressions.size(); ++index) {
        std::string name = "__chk__" + std::to_string(index);
        auto var = context.bool_const(name.c_str());
        variables.push_back(var);
//        engine.add(z3::implies(expressions[(int) index], var)); // TODO: or engine.add(var == expressions[(int) index]) ?
        solver.add(var == expressions[(int) index]); // TODO: or engine.add(var == expressions[(int) index]) ?
    }

    // check
    auto answer = solver.consequences(assumptions, variables, consequences);
    solver.pop();

    // create result
    std::vector<bool> result(expressions.size(), false);
    switch (answer) {
        case z3::unknown:
            throw Z3SolvingFailedError();

        case z3::unsat:
            if (isSolverUnsatisfiable) *isSolverUnsatisfiable = true;
            result.flip();
            return result;

        case z3::sat:
            if (isSolverUnsatisfiable) *isSolverUnsatisfiable = false;
            for (unsigned int index = 0; index < expressions.size(); ++index) {
                auto search = z3::implies(true, variables[(int) index]);
                auto find = std::find_if(consequences.begin(), consequences.end(), [search](const auto& elem){
                    return z3::eq(search, elem);
                });
                if (find != consequences.end()) result[index] = true;
            }
            return result;
    }
}

struct MethodChooser {
    // TODO: identify working method beforehand (during construction)
    // static std::function<std::vector<bool>(z3::engine&, const z3::expr_vector&, bool*)> method = GetSolvingMethod();
    // return method(engine, expressions, isSolverUnsatisfiable);

    bool fallback = false;
    inline std::vector<bool> operator()(z3::solver& solver, const z3::expr_vector& expressions, bool* isSolverUnsatisfiable) {
        if (fallback) return ComputeImpliedOneAtATime(solver, expressions, isSolverUnsatisfiable);
        try {
            return ComputeImpliedInOneShot(solver, expressions, isSolverUnsatisfiable);
        } catch (const Z3SolvingFailedError& err) {
            auto result = ComputeImpliedOneAtATime(solver, expressions, isSolverUnsatisfiable);
            unsigned int versionMajor, versionMinor, versionBuild, versionRevision;
            Z3_get_version(&versionMajor, &versionMinor, &versionBuild, &versionRevision);
            std::cerr << "WARNING: solving failure with Z3's engine::consequences! This issue is known to happen for versions >4.8.7, " <<
                      "your version is " << versionMajor << "." << versionMinor << "." << versionBuild << ". " <<
                      "Using fallback, performance may degrade..." << std::endl;
            fallback = true;
            return result;
        }
    }
} method;


std::vector<bool> engine::ComputeImplied(z3::solver& solver, const z3::expr_vector& expressions, bool* isSolverUnsatisfiable) {
    return method(solver, expressions, isSolverUnsatisfiable);
}

void engine::ComputeImpliedCallback(z3::solver& solver, const ImplicationCheckSet& checks, bool* isSolverUnsatisfiable) {
    auto implied = ComputeImplied(solver, checks.expressions, isSolverUnsatisfiable);
    assert(checks.expressions.size() == implied.size());
    for (std::size_t index = 0; index < implied.size(); ++index) {
        checks.callbacks.at(index)(implied.at(index));
    }
}
