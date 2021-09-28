#include "engine/proof.hpp"

#include "programs/util.hpp"
#include "logics/util.hpp"
#include "util/shortcuts.hpp"
#include "util/log.hpp"

using namespace plankton;


constexpr std::size_t PROOF_ABORT_AFTER = 7;


inline std::unique_ptr<HeapEffect> MakeDummyEffect(SymbolFactory& factory, const Type& nodeType) {
    auto pre = plankton::MakeSharedMemory(factory.GetFreshFO(nodeType), Type::Data(), factory);
    auto post = plankton::Copy(*pre);
    auto context = std::make_unique<SeparatingConjunction>();
    return std::make_unique<HeapEffect>(std::move(pre), std::move(post), std::move(context));
}

inline std::unique_ptr<HeapEffect> MakeDummyEffect1(SymbolFactory& factory, const Type& nodeType) {
    // [ x |=> G(inflow=A, marked=b, next=c, val=d) ~~> x |=> G(inflow=A, marked=b, next=c', val=d) | A != ∅ * b == false * c != nullptr * d < ∞ ]
    auto eff = MakeDummyEffect(factory, nodeType);
    eff->post->fieldToValue.at("next") = std::make_unique<SymbolicVariable>(factory.GetFreshFO(nodeType));
    auto ctx = std::make_unique<SeparatingConjunction>();
    ctx->Conjoin(std::make_unique<InflowEmptinessAxiom>(eff->pre->flow->Decl(), false));
    ctx->Conjoin(std::make_unique<StackAxiom>(BinaryOperator::EQ, std::make_unique<SymbolicVariable>(eff->pre->fieldToValue.at("marked")->Decl()), std::make_unique<SymbolicBool>(false)));
    ctx->Conjoin(std::make_unique<StackAxiom>(BinaryOperator::NEQ, std::make_unique<SymbolicVariable>(eff->pre->fieldToValue.at("next")->Decl()), std::make_unique<SymbolicNull>()));
    ctx->Conjoin(std::make_unique<StackAxiom>(BinaryOperator::LT, std::make_unique<SymbolicVariable>(eff->pre->fieldToValue.at("val")->Decl()), std::make_unique<SymbolicMax>()));
    eff->context = std::move(ctx);
    return eff;
}

inline std::unique_ptr<HeapEffect> MakeDummyEffect2(SymbolFactory& factory, const Type& nodeType) {
    // [ x |=> G(inflow=A, marked=b, next=c, val=d) ~~> x |=> G(inflow=A, marked=b', next=c, val=d) | A != ∅ * b == false * c != nullptr * d > -∞ * d < ∞ ]
    auto eff = MakeDummyEffect(factory, nodeType);
    eff->post->fieldToValue.at("marked") = std::make_unique<SymbolicVariable>(factory.GetFreshFO(Type::Bool()));
    auto ctx = std::make_unique<SeparatingConjunction>();
    ctx->Conjoin(std::make_unique<InflowEmptinessAxiom>(eff->pre->flow->Decl(), false));
    ctx->Conjoin(std::make_unique<StackAxiom>(BinaryOperator::EQ, std::make_unique<SymbolicVariable>(eff->pre->fieldToValue.at("marked")->Decl()), std::make_unique<SymbolicBool>(false)));
    ctx->Conjoin(std::make_unique<StackAxiom>(BinaryOperator::NEQ, std::make_unique<SymbolicVariable>(eff->pre->fieldToValue.at("next")->Decl()), std::make_unique<SymbolicNull>()));
    ctx->Conjoin(std::make_unique<StackAxiom>(BinaryOperator::GT, std::make_unique<SymbolicVariable>(eff->pre->fieldToValue.at("val")->Decl()), std::make_unique<SymbolicMin>()));
    ctx->Conjoin(std::make_unique<StackAxiom>(BinaryOperator::LT, std::make_unique<SymbolicVariable>(eff->pre->fieldToValue.at("val")->Decl()), std::make_unique<SymbolicMax>()));
    eff->context = std::move(ctx);
    return eff;
}

inline std::unique_ptr<HeapEffect> MakeDummyEffect3(SymbolFactory& factory, const Type& nodeType) {
    // [ x |=> G(inflow=A, marked=b, next=c, val=d) ~~> x |=> G(inflow=A', marked=b, next=c, val=d) | A != ∅ * d > -∞ ]
    auto eff = MakeDummyEffect(factory, nodeType);
    eff->post->flow = std::make_unique<SymbolicVariable>(factory.GetFreshSO(Type::Data()));
    auto ctx = std::make_unique<SeparatingConjunction>();
    ctx->Conjoin(std::make_unique<InflowEmptinessAxiom>(eff->pre->flow->Decl(), false));
    ctx->Conjoin(std::make_unique<StackAxiom>(BinaryOperator::GT, std::make_unique<SymbolicVariable>(eff->pre->fieldToValue.at("val")->Decl()), std::make_unique<SymbolicMin>()));
    eff->context = std::move(ctx);
    return eff;
}

inline std::deque<std::unique_ptr<HeapEffect>> MakeDummyInterference(const Type& nodeType) {
    SymbolFactory factory;
    std::deque<std::unique_ptr<HeapEffect>> result;
    result.push_back(MakeDummyEffect1(factory, nodeType));
    result.push_back(MakeDummyEffect2(factory, nodeType));
    result.push_back(MakeDummyEffect3(factory, nodeType));
    return result;
}


//
// Programs
//

void ProofGenerator::GenerateProof() {
    // TODO: remove debug
    newInterference = MakeDummyInterference(*program.types.at(0));
    ConsolidateNewInterference();

    // TODO: check initializer
    
    // check API functions
    for (std::size_t counter = 0; counter < PROOF_ABORT_AFTER; ++counter) {
        program.Accept(*this);
        if (!ConsolidateNewInterference()) return;
    }
    throw std::logic_error("Aborting: proof does not seem to stabilize."); // TODO: remove / better error handling
}

#include "util/timer.hpp"
void ProofGenerator::Visit(const Program& object) {
    assert(&object == &program);
    for (const auto& function : program.apiFunctions) {
        {
            Timer timer("ProofGenerator::Visit for '" + function->name + "'");
            auto measurement = timer.Measure();
            HandleInterfaceFunction(*function);
        }

        // DEBUG
        DEBUG(std::endl << std::endl << std::endl << "@@@ DEBUG EXIT @@@" << std::endl << std::endl << std::endl);
        exit(0);
        throw std::logic_error("--- point du break ---");
    }
}

//
// Common api/macro function handling
//

void ProofGenerator::Visit(const Function& func) {
    func.body->Accept(*this);
    assert(breaking.empty());
    if (!current.empty() && !plankton::IsVoid(func)) {
        throw std::logic_error("Detected non-returning path through non-void function '" + func.name + "'."); // TODO: better error handling
    }
    for (auto& annotation : current) {
        returning.emplace_back(std::move(annotation), nullptr);
    }
    current.clear();
}

//
// Interface function
//

inline Specification GetKind(const Function& function) { // TODO: move to utils?
    if (function.name == "contains") {
        return Specification::CONTAINS;
    } else if (function.name == "insert" || function.name == "add") {
        return Specification::INSERT;
    } else if (function.name == "delete" || function.name == "remove") {
        return Specification::DELETE;
    } else {
        throw std::logic_error("Specification for function '" + function.name + "' unknown, expected one of: 'contains', 'insert', 'add', 'delete', 'remove'."); // TODO: better error handling
    }
}

inline std::unique_ptr<SymbolicVariable> GetSearchKeyValue(const Annotation& annotation, const Function& function) {
    if (function.parameters.size() != 1) {
        throw std::logic_error("Expected one parameter to function '" + function.name + "', got " + std::to_string(function.parameters.size()) + "."); // TODO: better error handling
    }
    auto& param = *function.parameters.front().get();
    auto variables = plankton::Collect<EqualsToAxiom>(*annotation.now);
    auto find = FindIf(variables, [&param](auto* elem){ return elem->Variable() == param; });
    if (find != variables.end()) return std::make_unique<SymbolicVariable>((**find).value->Decl());
    throw std::logic_error("Could not find search key"); // TODO: better error handling
}

inline std::unique_ptr<ObligationAxiom> MakeObligation(const Annotation& annotation, const Function& function) {
    auto kind = GetKind(function);
    auto key = GetSearchKeyValue(annotation, function);
    return std::make_unique<ObligationAxiom>(kind, std::move(key));
}

inline std::unique_ptr<Formula> MakeKeyBounds(const SymbolDeclaration& key) {
    static auto mkLt = [](auto lhs, auto rhs){
        return std::make_unique<StackAxiom>(BinaryOperator::LT, std::move(lhs), std::move(rhs));
    };
    auto result = std::make_unique<SeparatingConjunction>();
    result->Conjoin(mkLt(std::make_unique<SymbolicMin>(), std::make_unique<SymbolicVariable>(key)));
    result->Conjoin(mkLt(std::make_unique<SymbolicVariable>(key), std::make_unique<SymbolicMax>()));
    return result;
}

inline std::unique_ptr<Annotation> MakeInterfaceAnnotation(const Program& program, const Function& function, const Solver& solver) {
    auto result = std::make_unique<Annotation>();
    result = solver.PostEnter(std::move(result), program);
    result = solver.PostEnter(std::move(result), function);
    auto obligation = MakeObligation(*result, function);
    result->Conjoin(MakeKeyBounds(obligation->key->Decl()));
    result->Conjoin(std::move(obligation));
    plankton::Simplify(*result);
    return result;
}

inline bool IsFulfilled(const Annotation& annotation, const Return& command) {
    assert(command.expressions.size() == 1); // TODO: throw?
    auto returnExpression = command.expressions.front().get();
    bool returnValue;
    if (dynamic_cast<const TrueValue*>(returnExpression)) returnValue = true;
    else if (dynamic_cast<const FalseValue*>(returnExpression)) returnValue = false;
    else throw std::logic_error("Cannot detect return value"); // TODO: better error handling
    auto fulfillments = plankton::Collect<FulfillmentAxiom>(*annotation.now);
    return std::any_of(fulfillments.begin(), fulfillments.end(),
                       [returnValue](auto* elem) { return elem->returnValue == returnValue; });
}

void ProofGenerator::HandleInterfaceFunction(const Function& function) {
    assert(function.kind == Function::Kind::API);
    
	DEBUG(std::endl << std::endl << std::endl << std::endl << std::endl)
	DEBUG("############################################################" << std::endl)
	DEBUG("############################################################" << std::endl)
	DEBUG("#################### " << function.name << std::endl)
	DEBUG("############################################################" << std::endl)
	DEBUG("############################################################" << std::endl)
	DEBUG(std::endl)
 
	// reset
    insideAtomic = false;
    returning.clear();
    breaking.clear();
    current.clear();
    
    // descent into function
    current.push_back(MakeInterfaceAnnotation(program, function, solver));
    function.Accept(*this);
    
    // check post annotations
    DEBUG(std::endl << std::endl << "=== CHECKING POST ANNOTATION OF " << function.name << "  " << returning.size() << std::endl)
    for (auto& [annotation, command] : returning) {
        assert(command);
        if (IsFulfilled(*annotation, *command)) continue;
        annotation = solver.TryAddFulfillment(std::move(annotation));
        if (IsFulfilled(*annotation, *command)) continue;
        DEBUG("Linearizability fail for " << *command << "  in  " << *annotation << std::endl)
        throw std::logic_error("Could not establish linearizability for function '" + function.name + "'."); // TODO: better error handling
    }
}
