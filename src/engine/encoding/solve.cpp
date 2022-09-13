#include "engine/encoding.hpp"

#include <algorithm>
#include <random>
#include <thread>
#include <mutex>
#include "internal.hpp"
#include "util/shortcuts.hpp"
#include "util/timer.hpp"

using namespace plankton;

static constexpr std::size_t BATCH_SIZE = 16;
static constexpr std::size_t PARALLEL_THRESHOLD = 3 * BATCH_SIZE;
static constexpr std::size_t FALLBACK_THREAD_COUNT = 8;


struct PreferredMethodFailed : std::exception {
    [[nodiscard]] const char* what() const noexcept override {
        return "SMT solving failed: Z3 was unable to prove/disprove satisfiability; solving result was 'UNKNOWN'.";
    }
};

inline z3::expr Translate(const z3::expr& expr, const z3::context& srcContext, z3::context& dstContext) {
    return z3::to_expr(dstContext, Z3_translate(srcContext, expr, dstContext));
}


//
// Z3 handling
//

inline bool IsUnsat(z3::solver& solver) {
    solver.push();
    auto res = solver.check();
    solver.pop();
    switch (res) {
        case z3::unsat: return true;
        case z3::sat: return false;
        case z3::unknown: throw std::logic_error("Solving failed: Z3 returned z3::unknown."); // TODO: better error handling
    }
    throw;
}

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
    throw;
}


//
// Batch solving
//

inline std::vector<bool> ComputeImpliedOneAtATimeSequential(z3::solver& solver, const std::deque<EExpr>& expressions);
inline std::vector<bool> ComputeImpliedOneAtATimeParallel(z3::solver& solver, const std::deque<EExpr>& expressions);

inline std::vector<bool> ComputeImpliedOneAtATime(z3::solver& solver, const std::deque<EExpr>& expressions) {
    if (solver.check() == z3::unsat) return std::vector<bool>(expressions.size(), true);
    if (expressions.size() < PARALLEL_THRESHOLD) return ComputeImpliedOneAtATimeSequential(solver, expressions);
    else return ComputeImpliedOneAtATimeParallel(solver, expressions);
}

inline std::vector<bool> ComputeImpliedOneAtATimeSequential(z3::solver& solver, const std::deque<EExpr>& expressions) {
    std::vector<bool> result;
    result.reserve(expressions.size());
    for (const auto& check : expressions) {
        result.push_back(IsImplied(solver, AsExpr(check)));
    }
    return result;
}

inline std::size_t GetThreadCount() {
    auto result = std::thread::hardware_concurrency();
    if (result > 0) return result * 2;
    return FALLBACK_THREAD_COUNT;
}

struct Task {
    std::size_t id;
    z3::expr expr;
    explicit Task(std::size_t id, const z3::expr& expr) : id(id), expr(expr) {}
};

struct Result {
    std::size_t id;
    bool implied;
    Result(const Task& task, bool implied) : id(task.id), implied(implied) {}
};

struct TaskPool {
    z3::context& srcContext;
    z3::solver& srcSolver;
    std::vector<Task> tasks;
    std::vector<Result> results;
    std::mutex takeMutex;
    std::mutex putMutex;

    explicit TaskPool(const std::deque<EExpr>& expressions, z3::solver& solver) : srcContext(solver.ctx()), srcSolver(solver) {
        tasks.reserve(expressions.size());
        for (std::size_t index = 0; index < expressions.size(); ++index) {
            tasks.emplace_back(index, AsExpr(expressions.at(index)));
        }
        std::shuffle(tasks.begin(), tasks.end(), std::default_random_engine());
    }

    std::vector<Task> Take(z3::solver& dstSolver) {
        std::lock_guard guard(takeMutex);
        auto& dstContext = dstSolver.ctx();
        auto result = plankton::MakeVector<Task>(BATCH_SIZE);
        for (std::size_t index = 0; index < BATCH_SIZE && !tasks.empty(); ++index) {
            result.push_back(tasks.back());
            result.back().expr = Translate(result.back().expr, srcContext, dstContext);
            tasks.pop_back();
        }
        return result;
    }

    void Put(std::vector<Result> res) {
        std::lock_guard guard(putMutex);
        plankton::MoveInto(std::move(res), results);
    }
};

inline void Work(TaskPool& pool) {
    z3::context context;
    z3::solver solver(context);
    std::unique_lock guard(pool.takeMutex);
    auto premise = z3::mk_and(pool.srcSolver.assertions());
    solver.add(Translate(premise, pool.srcContext, context));
    guard.unlock();

    while (true) {
        auto tasks = pool.Take(solver);
        if (tasks.empty()) break;
        auto results = plankton::MakeVector<Result>(tasks.size());
        for (const auto& task : tasks) {
            bool implied = IsImplied(solver, task.expr);
            // DEBUG("." << std::flush)
            results.emplace_back(task, implied);
        }
        pool.Put(std::move(results));
    }
}

inline std::vector<bool> ComputeImpliedOneAtATimeParallel(z3::solver& solver, const std::deque<EExpr>& expressions) {
    static const std::size_t THREAD_COUNT = GetThreadCount();

    // DEBUG("#threads=" << THREAD_COUNT << " #expr=" << expressions.size() << " " << std::flush)
    TaskPool taskPool(expressions, solver);
    std::deque<std::thread> threads;
    for (std::size_t index = 0; index < THREAD_COUNT; ++index) {
        threads.emplace_back([&taskPool](){
            Work(taskPool);
        });
    }
    for (auto& thread : threads) thread.join();
    // DEBUG(std::endl)

    std::vector<bool> result(expressions.size());
    for (const auto& elem : taskPool.results) result.at(elem.id) = elem.implied;
    return result;
}


//
// Backbone solving
//

inline std::vector<bool> ComputeImpliedInOneShot(z3::solver& solver, const std::deque<EExpr>& expressions) {
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
        solver.add(var == AsExpr(expressions.at(index)));
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
                for (const auto& elem : consequences) {
                    if (!z3::eq(search, elem)) continue;
                    result[index] = true;
                    break;
                }
                // bool implied = plankton::ContainsIf(consequences, [search](const z3::expr& elem) {
                //     return z3::eq(search, elem);
                // });
                // if (implied) result[index] = true;
            }
            return result;
    }
    throw;
}


//
// Adaptive method choosing
//

struct LateWarning {
    const std::string warning;
    explicit LateWarning(std::string warning) : warning(std::move(warning)) {}
    ~LateWarning() { WARNING(warning) }
};

inline std::string GetZ3Version() {
    unsigned int versionMajor, versionMinor, versionBuild, versionRevision;
    Z3_get_version(&versionMajor, &versionMinor, &versionBuild, &versionRevision);
    return std::to_string(versionMajor) + "." + std::to_string(versionMinor) + "." + std::to_string(versionBuild);
}

struct MethodChooser {
    // TODO: identify working method beforehand (during construction)
    bool fallback = false;

    inline std::vector<bool> operator()(z3::solver& solver, const std::deque<EExpr>& expressions) {
        if (fallback) return ComputeImpliedOneAtATime(solver, expressions);
        try {
            return ComputeImpliedInOneShot(solver, expressions);
        } catch (const PreferredMethodFailed& err) {
            std::stringstream warning;
            warning << "solving failure with Z3's solver::consequences! "
                    << "This issue is known to happen for versions >4.8.7, your version is " << GetZ3Version()
                    << ". Using fallback, performance may degrade..." << std::endl;
            WARNING(warning.str())
            static LateWarning lateWarning(warning.str());
            fallback = true;
            return ComputeImpliedOneAtATime(solver, expressions);
        }
    }
} solvingMethod;


//
// Encoding API
//

// /**
//  * This is to fight strange non-deterministic behavior of Z3 where certain solver states
//  * might evaluate to 'sat' or 'unsat' depending on Z3's mood...
//  * Using 'push'/'pop' on the solver would not help to resolve the problem.
//  * TODO: how to avoid/improve this?
//  */
// struct Z3Wrapper {
//     z3::context context;
//     z3::solver solver;
//
//     explicit Z3Wrapper(const z3::solver& srcSolver) : solver(context) {
//         MEASURE("Z3Wrapper ~> ctor")
//         const auto& srcContext = srcSolver.ctx();
//         for (std::size_t index = 0; index < srcSolver.assertions().size(); ++index) {
//             auto srcExpr = srcSolver.assertions()[index];
//             auto expr = ::Translate(srcExpr, srcContext, context);
//             solver.add(expr);
//         }
//     }
//
//     z3::expr Translate(const EExpr& expr) {
//         MEASURE("Z3Wrapper ~> Translate(EExpr)")
//         const auto& z3expr = AsExpr(expr);
//         return ::Translate(z3expr, z3expr.ctx(), context);
//     }
//
//     std::deque<EExpr> Translate(const std::deque<EExpr>& container) {
//         MEASURE("Z3Wrapper ~> Translate(std::deque<EExpr>)")
//         std::deque<EExpr> result;
//         for (const auto& elem : container) result.push_back(AsEExpr(Translate(elem)));
//         return result;
//     }
// };
//
// inline bool IsUnsat(std::unique_ptr<InternalStorage>& internal) {
//     Z3Wrapper wrapper(AsSolver(internal));
//     return IsUnsat(wrapper.solver);
// }
//
// inline bool IsImplied(std::unique_ptr<InternalStorage>& internal, const EExpr& expression) {
//     Z3Wrapper wrapper(AsSolver(internal));
//     return IsImplied(wrapper.solver, wrapper.Translate(expression));
// }
//
// inline std::vector<bool> ComputeImplied(std::unique_ptr<InternalStorage>& internal, const std::deque<EExpr>& expressions) {
//     Z3Wrapper wrapper(AsSolver(internal));
//     return solvingMethod(wrapper.solver, wrapper.Translate(expressions));
// }

inline bool IsUnsat(std::unique_ptr<InternalStorage>& internal) {
    auto& solver = AsSolver(internal);
    solver.push();
    auto result = IsUnsat(solver);
    solver.pop();
    return result;
}

inline bool IsImplied(std::unique_ptr<InternalStorage>& internal, const EExpr& expression) {
    auto& solver = AsSolver(internal);
    solver.push();
    auto result = IsImplied(solver, AsExpr(expression));
    solver.pop();
    return result;
}

inline std::vector<bool> ComputeImplied(std::unique_ptr<InternalStorage>& internal, const std::deque<EExpr>& expressions) {
    auto& solver = AsSolver(internal);
    solver.push();
    auto result = solvingMethod(solver, expressions);
    solver.pop();
    return result;
}


void Encoding::Check() {
    MEASURE("Encoding::Check")
    assert(checks_premise.size() == checks_callback.size());
    if (checks_premise.empty()) return;
    auto implied = ComputeImplied(internal, checks_premise);
    for (std::size_t index = 0; index < implied.size(); ++index) {
        checks_callback.at(index)(implied.at(index));
    }
    checks_premise.clear();
    checks_callback.clear();
}

bool Encoding::Implies(const EExpr& expr) {
    MEASURE("Encoding::Implies")
    auto result = IsImplied(internal, expr);
    return result;
}

bool Encoding::ImpliesFalse() {
    MEASURE("Encoding::ImpliesFalse")
    auto result = IsUnsat(internal);
    return result;
}

bool Encoding::Implies(const Formula& formula) {
    return Implies(Encode(formula));
}

bool Encoding::Implies(const NonSeparatingImplication& formula) {
    return Implies(Encode(formula));
}

bool Encoding::Implies(const ImplicationSet& formula) {
    return Implies(Encode(formula));
}

std::set<const SymbolDeclaration*> Encoding::ComputeNonNull(std::set<const SymbolDeclaration*> symbols) {
    plankton::DiscardIf(symbols, [](auto* decl){ return decl->type.sort != Sort::PTR; });

    std::deque<EExpr> expressions;
    for (const auto* elem : symbols) expressions.push_back(EncodeIsNonNull(*elem));
    auto implied = ComputeImplied(internal, expressions);
    
    auto sym = symbols.begin();
    for (bool isNonNull : implied) {
        assert(sym != symbols.end());
        if (isNonNull) ++sym;
        else sym = symbols.erase(sym);
    }
    return symbols;
}
