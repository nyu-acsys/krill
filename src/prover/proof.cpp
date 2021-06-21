#include "prover/verify.hpp"

#include <set>
#include <sstream>
#include "cola/util.hpp"
#include "prover/logger.hpp" // TODO:remove
#include "heal/collect.hpp"

using namespace cola;
using namespace heal;
using namespace solver;
using namespace prover;


constexpr std::size_t LOOP_ABORT_AFTER = 10;

using UnsupportedConstructError = std::logic_error; // TODO: better error handling
using VerificationError = std::logic_error; // TODO: better error handling
using InternalError = std::logic_error; // TODO: better error handling

inline void ThrowMissingReturn(const Function& function) {
	throw std::logic_error("Detected non-returning path through non-void function '" + function.name + "'."); // TODO: better error handling
}

inline bool is_void(const Function& function) {
	return function.return_type.size() == 1 && &function.return_type.at(0).get() == &Type::void_type();
}

inline const SymbolicVariableDeclaration& GetValueOrFail(const VariableDeclaration& decl, const LogicObject& obj, const Solver& solver) {
    auto result = solver.GetVariableResource(decl, obj);
    if (result) return result->value->Decl();
    throw InternalError("Could not find resource for accessed variable " + decl.name + "."); // TODO: better error handling
}

struct FulfillmentCollector : public BaseResourceVisitor {
    std::deque<const FulfillmentAxiom*> result;
    void enter(const FulfillmentAxiom& axiom) override { result.push_back(&axiom); }
    static decltype(result) Collect(const Annotation& annotation) {
        FulfillmentCollector collector;
        annotation.accept(collector);
        return std::move(collector.result);
    }
};

//
// Programs, Functions
//

void Verifier::visit(const Program&) {
    throw std::logic_error("not supported"); // TODO: better error handling
}

std::unique_ptr<HeapEffect> MakeDummyEffect(const Type& nodeType, SymbolicAxiom::Operator op) {
//    ** effect: @a0 |-> G(@D8, next:@a24, val:@d22) ~~> @a0 |-> G(@D8, next:@a4, val:@d22) under @D8 != ∅   *   @a24 != NULL   *   @d22 == -∞   *   @d22 < ∞
//    ** effect: @a1 |-> G(@D5, next:@a3, val:@d7) ~~> @a1 |-> G(@D32, next:@a3, val:@d7) under @D5 != ∅   *   @a3 != NULL   *   @d7 > -∞   *   @d7 < ∞

    SymbolicFactory factory;
    auto& address = factory.GetUnusedSymbolicVariable(nodeType);
    auto& flow = factory.GetUnusedFlowVariable(Type::data_type());
    std::map<std::string, std::unique_ptr<SymbolicVariable>> fieldToValue;
    for (const auto& [field, type] : nodeType.fields) {
        fieldToValue[field] = std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(type));
    }
    auto pre = std::make_unique<PointsToAxiom>(std::make_unique<SymbolicVariable>(address), false, flow, std::move(fieldToValue));

    std::unique_ptr<PointsToAxiom> post(dynamic_cast<PointsToAxiom*>(heal::Copy(*pre).release()));
    assert(post);
    post->flow = factory.GetUnusedFlowVariable(Type::data_type());
    post->fieldToValue.at("next") = std::make_unique<SymbolicVariable>(factory.GetUnusedSymbolicVariable(nodeType));

    auto context = std::make_unique<SeparatingConjunction>();
    context->conjuncts.push_back(std::make_unique<SymbolicAxiom>(heal::Copy(*pre->fieldToValue.at("next")), SymbolicAxiom::NEQ, std::make_unique<SymbolicNull>()));
    context->conjuncts.push_back(std::make_unique<InflowEmptinessAxiom>(pre->flow, false));

    return std::make_unique<HeapEffect>(std::move(pre), std::move(post), std::move(context));
}

std::deque<std::unique_ptr<HeapEffect>> MakeDummyInterference(const Type& nodeType) {
    std::deque<std::unique_ptr<HeapEffect>> result;
    result.push_back(MakeDummyEffect(nodeType, SymbolicAxiom::EQ));
//    result.push_back(MakeDummyEffect(SymbolicAxiom::GT));
    return result;
}

void Verifier::VerifyProgramOrFail() {
    // TODO: check initializer

    interference = MakeDummyInterference(*program.types.front()); // TODO: remove <<<<<<<<<<<<<<<<==========================================|||||||||||||

    // compute fixed point
    do {
        for (const auto& function : program.functions) {
            if (function->kind != Function::Kind::INTERFACE) continue;
            HandleInterfaceFunction(*function);
            throw std::logic_error("point du break");
        }
        throw std::logic_error("point du break");
    } while (ConsolidateNewInterference());
}

class SpecStore {
	SpecificationAxiom::Kind kind;
	const VariableDeclaration& searchKey;

    static inline std::pair<SpecificationAxiom::Kind, const VariableDeclaration&> ExtractSpecification(const Function& function) {
        assert(function.kind == Function::INTERFACE);
        SpecificationAxiom::Kind kind;
        if (function.name == "contains") {
            kind =  SpecificationAxiom::Kind::CONTAINS;
        } else if (function.name == "insert" || function.name == "add") {
            kind =  SpecificationAxiom::Kind::INSERT;
        } else if (function.name == "delete" || function.name == "remove") {
            kind =  SpecificationAxiom::Kind::DELETE;
        } else {
            throw UnsupportedConstructError("Specification for function '" + function.name + "' unknown, expected one of: 'contains', 'insert', 'add', 'delete', 'remove'.");
        }
        if (function.args.size() != 1) {
            throw UnsupportedConstructError("Cannot verify function '" + function.name + "': expected 1 parameter, got " + std::to_string(function.args.size()) + ".");
        }
        if (function.return_type.size() != 1) {
            throw UnsupportedConstructError("Cannot verify function '" + function.name + "': expected 1 return value, got " + std::to_string(function.return_type.size()) + ".");
        }
        if (&function.return_type.at(0).get() != &Type::bool_type()) {
            throw UnsupportedConstructError("Cannot verify function '" + function.name + "': expected return type to be 'bool'.");
        }
        return { kind, *function.args.at(0) };
    }

    inline const SymbolicVariableDeclaration& GetSymbol(const Annotation& annotation) {
        struct Evaluator : public DefaultLogicListener {
            const VariableDeclaration* search = nullptr;
            const SymbolicVariableDeclaration* result = nullptr;
            void enter(const EqualsToAxiom& obj) override { if (&obj.variable->decl == search) result = &obj.value->Decl(); }
        } eval;
        eval.search = &searchKey;
        annotation.now->accept(eval);
        assert(eval.result);
        if (eval.result) return *eval.result;
        throw InternalError("Could not find resource for function parameter."); // TODO: better error handling
    }

    static inline bool GetReturnValue(const Return& command, const Annotation& annotation, const Solver& solver) {
        assert(command.expressions.size() == 1);
        auto value = solver.GetBoolValue(*command.expressions.front(), annotation);
        if (value.has_value()) return value.value();
        throw VerificationError("Cannot derive interface function return value.");
    }

public:
	explicit SpecStore(std::pair<SpecificationAxiom::Kind, const VariableDeclaration&> pair) : kind(pair.first), searchKey(pair.second) {}
	explicit SpecStore(const Function& function) : SpecStore(ExtractSpecification(function)) {}

    inline std::unique_ptr<SeparatingConjunction> MakeSpecification(const Annotation& annotation, const Solver& solver) {
        auto& symbol = GetValueOrFail(searchKey, annotation, solver);
        auto result = std::make_unique<SeparatingConjunction>();
        result->conjuncts.push_back(std::make_unique<ObligationAxiom>(kind, std::make_unique<SymbolicVariable>(symbol)));
        result->conjuncts.push_back(std::make_unique<SymbolicAxiom>(std::make_unique<SymbolicVariable>(symbol), SymbolicAxiom::GT, std::make_unique<SymbolicMin>()));
        result->conjuncts.push_back(std::make_unique<SymbolicAxiom>(std::make_unique<SymbolicVariable>(symbol), SymbolicAxiom::LT, std::make_unique<SymbolicMax>()));
        return result;
    }

    inline void EstablishSpecificationOrFail(const Solver& solver, const Annotation& annotation, const Return* command, const Function& function) {
        // throw std::logic_error("not yet implemented: Verifier::establish_linearizability_or_fail");
        log() << std::endl << "________" << std::endl << "Post annotation for function '" << function.name << "':" << std::endl << annotation << std::endl << std::endl;

        // check for missing return
        assert(!is_void(function));
        if (!command) ThrowMissingReturn(function);

        // search for fulfillment
        auto& searchValue = GetValueOrFail(searchKey, annotation, solver);
        bool returnedValue = GetReturnValue(*command, annotation, solver);
        for (const auto* fulfillment : FulfillmentCollector::Collect(annotation)) {
            if (fulfillment->kind != kind) continue;
            if (&fulfillment->key->Decl() != &searchValue) continue; // TODO: handle aliases
            if (fulfillment->return_value != returnedValue) continue;
            return;
        }
        throw VerificationError("Could not establish linearizability for function '" + function.name + "'.");
    }
};

void Verifier::HandleInterfaceFunction(const Function& function) {
    assert(function.kind == Function::Kind::INTERFACE);
    insideAtomic = false;
    current.clear();
    breaking.clear();
    returning.clear();

	std::cout << std::endl << std::endl << std::endl << std::endl << std::endl;
	std::cout << "############################################################" << std::endl;
	std::cout << "############################################################" << std::endl;
	std::cout << "#################### " << function.name << std::endl;
	std::cout << "############################################################" << std::endl;
	std::cout << "############################################################" << std::endl;
	std::cout << std::endl;
    auto start = std::chrono::steady_clock::now();

	// prepare initial annotation
    SpecStore specification(function);
	auto initial = std::make_unique<Annotation>();
    initial = solver->PostEnterScope(std::move(initial), program);
    initial = solver->PostEnterScope(std::move(initial), function);
    auto spec = specification.MakeSpecification(*initial, *solver);
    initial->now = heal::Conjoin(std::move(initial->now), std::move(spec));
    current.push_back(std::move(initial));

    // descend into function body
    function.body->accept(*this);

    // check post annotations
    std::cout << std::endl << std::endl << " CHECKING POST ANNOTATION OF " << function.name << std::endl << std::endl;
    for (auto& annotation : current) {
        returning.emplace_back(std::move(annotation), nullptr);
    }
    for (auto& [annotation, command] : returning) {
//        annotation = solver->Interpolate(std::move(annotation), interference);
        annotation = solver->PostLeaveScope(std::move(annotation), function);
        specification.EstablishSpecificationOrFail(*solver, *annotation, command, function);
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
    std::cout << "Time taken for " << function.name << ": " << elapsed/1000.0 << "s" << std::endl;
}


//
// Statements
//

void Verifier::visit(const Sequence& stmt) {
	stmt.first->accept(*this);
	stmt.second->accept(*this);
}

void Verifier::visit(const Scope& stmt) {
    PerformStep([this,&stmt](auto annotation){ return solver->PostEnterScope(std::move(annotation), stmt); });
	stmt.body->accept(*this);
    PerformStep([this,&stmt](auto annotation){ return solver->PostLeaveScope(std::move(annotation), stmt); });
}

void Verifier::visit(const Atomic& stmt) {
	bool old_inside_atomic = insideAtomic;
    insideAtomic = true;
	stmt.body->accept(*this);
    insideAtomic = old_inside_atomic;
    ApplyInterference(stmt);
}

inline std::deque<std::unique_ptr<Annotation>> CopyList(const std::deque<std::unique_ptr<Annotation>>& list) {
    std::deque<std::unique_ptr<Annotation>> result;
    for (const auto& elem : list) result.push_back(heal::Copy(*elem));
    return result;
}

void Verifier::visit(const Choice& stmt) {
    if (stmt.branches.empty()) return;

    auto pre = std::move(current);
    decltype(pre) post;
    current.clear();

    // handle all but first branch => current annotations need to be copied
    for (auto it = std::next(stmt.branches.begin()); it != stmt.branches.end(); ++it) {
        current = CopyList(pre);
        it->get()->accept(*this);
        std::move(current.begin(), current.end(), std::back_inserter(post));
    }

    // for the remaining first branch elide the copy
    current = std::move(pre);
    stmt.branches.front()->accept(*this);
    std::move(post.begin(), post.end(), std::back_inserter(current));
}

void Verifier::visit(const IfThenElse& stmt) {
    throw UnsupportedConstructError("'if'"); // TODO: rewrite ifs into choices
}

void Verifier::visit(const Loop& /*stmt*/) {
	throw UnsupportedConstructError("non-det loop");
}

void Verifier::visit(const While& stmt) {
    HandleLoop(stmt);
}

void Verifier::visit(const DoWhile& stmt) {
    HandleLoop(stmt);
}

void Verifier::HandleLoop(const ConditionalLoop& stmt) {
    if (current.empty()) return;
    if (dynamic_cast<const BooleanValue*>(stmt.expr.get()) == nullptr || !dynamic_cast<const BooleanValue*>(stmt.expr.get())->value) {
        throw UnsupportedConstructError("while/do-while loop with condition other than 'true'");
    }

    // peel first loop iteration
    std::cout << std::endl << std::endl << " ------ loop 0 (peeled) ------ " << std::endl;
    auto breakingOuter = std::move(breaking);
    breaking.clear();
    stmt.body->accept(*this);
    auto firstBreaking = std::move(breaking);
    auto returningOuter = std::move(returning);
    breaking.clear();
    returning.clear();

    // looping until fixed point
    if (!current.empty()) {
        std::size_t counter = 0;
        auto join = solver->Join(std::move(current));
        while (true) {
            if (counter > LOOP_ABORT_AFTER) throw std::logic_error("Aborting: loop does not seem to stabilize"); // TODO: remove / better error handling
            std::cout << std::endl << std::endl << " ------ loop " << ++counter << " ------ " << std::endl;

            breaking.clear();
            returning.clear();
            current.clear();
            current.push_back(heal::Copy(*join));

            stmt.body->accept(*this);
            current.push_back(heal::Copy(*join));
            auto newJoin = solver->Join(std::move(current));
            if (solver->Implies(*newJoin, *join) == Solver::YES) break;
            join = std::move(newJoin);
        }
    }

    // post loop
    current = std::move(firstBreaking);
    std::move(breaking.begin(), breaking.end(), std::back_inserter(current));
    breaking = std::move(breakingOuter);
    std::move(returningOuter.begin(), returningOuter.end(), std::back_inserter(returning));
}


//
// Commands
//

void Verifier::visit(const Skip& /*cmd*/) {
	/* do nothing */
}

void Verifier::visit(const Break& /*cmd*/) {
    std::move(current.begin(), current.end(), std::back_inserter(breaking));
    current.clear();
}

void Verifier::visit(const Continue& /*cmd*/) {
	throw UnsupportedConstructError("'continue'");
}

void Verifier::visit(const Assert& cmd) {
    throw UnsupportedConstructError("'assert'");
}

void Verifier::visit(const CompareAndSwap& /*cmd*/) {
    throw UnsupportedConstructError("CompareAndSwap");
}

void Verifier::visit(const Assume& cmd) {
    PerformStep([this,&cmd](auto annotation){ return solver->Post(std::move(annotation), cmd); });
    ApplyInterference(cmd);
}

void Verifier::visit(const Malloc& cmd) {
    PerformStep([this,&cmd](auto annotation){ return solver->Post(std::move(annotation), cmd); });
    ApplyInterference(cmd);
}

void Verifier::visit(const Assignment& cmd) {
    PerformStep([this,&cmd](auto annotation){ return solver->Post(std::move(annotation), cmd); });
    ApplyInterference(cmd);
}

void Verifier::visit(const ParallelAssignment& cmd) {
    PerformStep([this,&cmd](auto annotation){ return solver->Post(std::move(annotation), cmd); });
    ApplyInterference(cmd);
}

void Verifier::visit(const Return& cmd) {
    // check that pointers are accessible
    struct : public BaseVisitor {
        std::set<const VariableDeclaration*> variables;
        void visit(const BooleanValue& /*node*/) override { /* do nothing */ }
        void visit(const NullValue& /*node*/) override { /* do nothing */ }
        void visit(const EmptyValue& /*node*/) override { /* do nothing */ }
        void visit(const MaxValue& /*node*/) override { /* do nothing */ }
        void visit(const MinValue& /*node*/) override { /* do nothing */ }
        void visit(const NDetValue& /*node*/) override { /* do nothing */ }
        void visit(const VariableExpression& node) override { variables.insert(&node.decl); }
        void visit(const NegatedExpression& node) override { node.expr->accept(*this); }
        void visit(const BinaryExpression& node) override { node.lhs->accept(*this); node.rhs->accept(*this); }
        void visit(const Dereference& node) override { throw UnsupportedConstructError("Cannot return dereference expressions."); }
    } visitor;
    for (const auto* variable : visitor.variables) {
        for (const auto& annotation : current) {
            GetValueOrFail(*variable, *annotation, *solver);
        }
    }

    // post image
    for (auto& annotation : current) {
        std::cout << " returning with " << *annotation << std::endl;
        returning.emplace_back(std::move(annotation), &cmd);
    }
    current.clear();
}


//
// Macros
//

inline std::unique_ptr<Annotation> PerformMacroAssignment(std::unique_ptr<Annotation> annotation, const VariableDeclaration& lhs, const Expression& rhs, const Solver& solver) {
    Assignment dummy(std::make_unique<VariableExpression>(lhs), cola::copy(rhs)); // TODO: avoid copies
    auto post = solver.Post(std::move(annotation), dummy);
    if (!post.effects.empty()) throw std::logic_error("Returning from macros must not produce effects."); // TODO: better error handling
    if (post.posts.size() != 1) throw std::logic_error("Returning from macro must produce exactly one annotation."); // TODO: better error handling
    return std::move(post.posts.front());
}

inline std::unique_ptr<Annotation> PassMacroArguments(std::unique_ptr<Annotation> annotation, const Macro& command, const Solver& solver) {
    assert(command.args.size() == command.decl.args.size());
    for (std::size_t index = 0; index < command.args.size(); ++index) {
        annotation = PerformMacroAssignment(std::move(annotation), *command.decl.args.at(index), *command.args.at(index), solver);
    }
    return annotation;
}

inline std::unique_ptr<Annotation> ReturnFromMacro(std::unique_ptr<Annotation> annotation, const Return* command, const Macro& macro, const Solver& solver) {
    if (is_void(macro.decl)) return annotation;
    if (!command) ThrowMissingReturn(macro.decl);
    assert(macro.lhs.size() == command->expressions.size());

    for (std::size_t index = 0; index < macro.lhs.size(); ++index) {
        std::cout << "<PRE> returning value " << index << ": " << macro.lhs.at(index).get().name << " := " << *command->expressions.at(index) << " " << *annotation << std::endl;
        annotation = PerformMacroAssignment(std::move(annotation), macro.lhs.at(index), *command->expressions.at(index), solver);
        std::cout << "<POST> returning value " << index << ": " << macro.lhs.at(index).get().name << " := " << *command->expressions.at(index) << " " << *annotation << std::endl;
    }
    return annotation;
}

inline std::deque<std::unique_ptr<Annotation>> ReturnFromMacro(std::deque<std::pair<std::unique_ptr<Annotation>, const cola::Return*>>&& returning, const Macro& macro, const Solver& solver) {
    std::deque<std::unique_ptr<Annotation>> result;
    for (auto& [annotation, command] : returning) {
        result.push_back(ReturnFromMacro(std::move(annotation), command, macro, solver));
        std::cout << " returning from macro: " << *result.back() << std::endl;
    }
    return result;
}

void Verifier::visit(const Macro& cmd) {
    // save context
    auto breakingOuter = std::move(breaking);
    auto returningOuter = std::move(returning);
    breaking.clear();
    returning.clear();

    // pass arguments
    PerformStep([this,&cmd](auto annotation){
        annotation = solver->PostEnterScope(std::move(annotation), cmd.decl);
        return PassMacroArguments(std::move(annotation), cmd, *solver);
    });

    // descend into function call
    cmd.decl.body->accept(*this);
    assert(breaking.empty());
    for (auto& annotation : current) {
        returning.emplace_back(std::move(annotation), nullptr);
    }
    std::cout << " done macro descend" << std::endl;

    // restore caller context
    current = ReturnFromMacro(std::move(returning), cmd, *solver);
    PerformStep([this,&cmd](auto annotation){ return solver->PostLeaveScope(std::move(annotation), cmd.decl); });
    breaking = std::move(breakingOuter);
    returning = std::move(returningOuter);

	log() << std::endl << "________" << std::endl << "Post annotation for macro '" << cmd.decl.name << "':" << std::endl << "<deque>" /* *currentAnnotation */ << std::endl << std::endl;
}
