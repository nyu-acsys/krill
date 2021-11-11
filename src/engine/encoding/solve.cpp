#include "engine/encoding.hpp"

#include "internal.hpp"
#include "util/shortcuts.hpp"
#include "util/timer.hpp"

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
    MEASURE("Z3 ~> z3::solver::check")

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

inline std::vector<bool> ComputeImpliedOneAtATime(z3::solver& solver, const std::deque<EExpr>& expressions) {
    std::vector<bool> result;
    result.reserve(expressions.size());
    for (const auto& check : expressions) {
        result.push_back(IsImplied(solver, AsExpr(check)));
    }
    return result;
}


#include <algorithm>
#include <random>
#include <thread>

static constexpr std::size_t THREAD_COUNT = 12;
static constexpr std::size_t BATCH_SIZE = 16;

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

inline z3::expr Translate(const z3::expr& expr, z3::context& srcContext, z3::context& dstContext) {
    return z3::to_expr(dstContext, Z3_translate(srcContext, expr, dstContext));
}

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
            DEBUG("." << std::flush)
            results.emplace_back(task, implied);
        }
        pool.Put(std::move(results));
    }
}

inline std::vector<bool> ComputeImpliedInOneShot(z3::solver& solver, const std::deque<EExpr>& expressions) {
    MEASURE("Z3 ~> concurrent z3::solver::check")
    Timer singleTimer("ComputeImpliedInOneShot"); auto singleMeasurement = singleTimer.Measure();

    DEBUG("#expr=" << expressions.size() << std::flush)
    TaskPool taskPool(expressions, solver);
    std::deque<std::thread> threads;
    for (std::size_t index = 0; index < THREAD_COUNT; ++index) {
        threads.emplace_back([&taskPool](){
            Work(taskPool);
        });
    }
    for (auto& thread : threads) thread.join();
    DEBUG(std::endl)

    std::vector<bool> result(expressions.size());
    for (const auto& elem : taskPool.results) result.at(elem.id) = elem.implied;
    return result;
}

// struct WorkPackage {
//     z3::context context;
//     z3::solver solver;
//     std::vector<Task> tasks;
//     std::vector<Result> results;
//
//     explicit WorkPackage(z3::solver& srcSolver, std::size_t size) : solver(context) {
//         auto premise = z3::mk_and(srcSolver.assertions());
//         solver.add(Translate(premise, srcSolver.ctx(), context));
//         tasks.reserve(size);
//         results.reserve(size);
//     }
// };

// inline std::vector<bool> ComputeImpliedInOneShot(z3::solver& solver, const std::deque<EExpr>& expressions) {
//     MEASURE("Z3 ~> concurrent z3::solver::check")
//     Timer singleTimer("ComputeImpliedInOneShot"); auto singleMeasurement = singleTimer.Measure();
//
//     std::deque<WorkPackage> pool;
//     std::size_t size = expressions.size() / THREAD_COUNT + 1;
//     for (std::size_t index = 0; index < THREAD_COUNT; ++index) {
//         pool.emplace_back(solver, size);
//     }
//
//     std::size_t counter = 0;
//     for (std::size_t index = 0; index < expressions.size(); ++index) {
//         auto expr = AsExpr(expressions.at(index));
//         auto translated = Translate(expr, solver.ctx(), pool.at(counter).context);
//         pool.at(counter).tasks.emplace_back(index, translated);
//         ++counter;
//         if (counter >= pool.size()) counter = 0;
//     }
//
//     DEBUG("#expr=" << expressions.size() << std::flush)
//     std::deque<std::thread> threads;
//     for (auto& pkg : pool) {
//         threads.emplace_back([&pkg](){
//             for (const auto& task : pkg.tasks) {
//                 bool implied = IsImplied(pkg.solver, task.expr);
//                 DEBUG("." << std::flush)
//                 pkg.results.emplace_back(task, implied);
//             }
//         });
//     }
//     for (auto& thread : threads) thread.join();
//     DEBUG(std::endl)
//
//     std::vector<bool> result(expressions.size());
//     for (const auto& pkg : pool) {
//         for (const auto& elem : pkg.results) {
//             result.at(elem.id) = elem.implied;
//         }
//     }
//     return result;
// }


// inline std::vector<bool> ComputeImpliedInOneShot(z3::solver& solver, const std::deque<EExpr>& expressions) {
//     MEASURE("Z3 ~> z3::solver::consequences")
//
//     // prepare required vectors
//     solver.push();
//     auto& context = solver.ctx();
//     z3::expr_vector variables(context);
//     z3::expr_vector assumptions(context);
//     z3::expr_vector consequences(context);
//
//     // prepare engine
//     for (unsigned int index = 0; index < expressions.size(); ++index) {
//         std::string name = "__chk__" + std::to_string(index);
//         auto var = context.bool_const(name.c_str());
//         variables.push_back(var);
//         solver.add(var == AsExpr(expressions.at(index)));
//     }
//
//     // check
//     auto answer = solver.consequences(assumptions, variables, consequences);
//     solver.pop();
//
//     // create result
//     std::vector<bool> result(expressions.size(), false);
//     switch (answer) {
//         case z3::unknown:
//             throw PreferredMethodFailed();
//
//         case z3::unsat:
//             result.flip();
//             return result;
//
//         case z3::sat:
//             for (unsigned int index = 0; index < expressions.size(); ++index) {
//                 auto search = z3::implies(true, variables[(int) index]);
//                 bool implied = plankton::ContainsIf(consequences, [search](const auto& elem) {
//                     return z3::eq(search, elem);
//                 });
//                 if (implied) result[index] = true;
//             }
//             return result;
//     }
// }

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

    inline std::vector<bool> operator()(z3::solver& solver, const std::deque<EExpr>& expressions) {
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
    Push();
    auto implied = solvingMethod(AsSolver(internal), checks_premise);
    for (std::size_t index = 0; index < implied.size(); ++index) {
        checks_callback.at(index)(implied.at(index));
    }
    checks_premise.clear();
    checks_callback.clear();
    Pop();
}

bool Encoding::ImpliesFalse() {
    return Implies(Bool(false));
}

bool Encoding::Implies(const EExpr& expr) {
    return IsImplied(AsSolver(internal), AsExpr(expr));
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
    auto implied = solvingMethod(AsSolver(internal), expressions);
    
    auto sym = symbols.begin();
    for (bool isNonNull : implied) {
        assert(sym != symbols.end());
        if (isNonNull) ++sym;
        else sym = symbols.erase(sym);
    }
    return symbols;
}
