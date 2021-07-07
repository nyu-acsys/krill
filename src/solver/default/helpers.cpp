#include "default_solver.hpp"

#include "timer.hpp"
#include "encoder.hpp"
#include "heal/util.hpp"

using namespace cola;
using namespace heal;
using namespace solver;


const EqualsToAxiom* DefaultSolver::GetVariableResource(const cola::VariableDeclaration& decl, const LogicObject& object) const {
    return FindResource(decl, object);
}

std::optional<bool> DefaultSolver::GetBoolValue(const cola::Expression& expr, const LogicObject& object) const {
    if (expr.sort() != Sort::BOOL) return std::nullopt;

    // try solve syntactically
    auto value = EagerValuationMap(object).Evaluate(expr);
    if (auto boolean = dynamic_cast<const SymbolicBool*>(value.get())) {
        return boolean->value;
    }

    // solve semantically
    z3::context context;
    z3::solver solver(context);
    Z3Encoder encoder(context, solver);
    solver.add(encoder(object));
    ImplicationCheckSet checks(context);
    std::optional<bool> result = std::nullopt;
    for (bool b : { true, false }) {
        checks.Add(encoder(*value) == context.bool_val(b), [&result,b](bool holds) {
            if (holds) result = b;
        });
    }
    solver::ComputeImpliedCallback(solver, checks);
    return result;
}

struct SymbolicRenaming {
    SymbolicFactory factory;
    std::map<const SymbolicVariableDeclaration*, const SymbolicVariableDeclaration*> symbolRenaming;
    std::map<const SymbolicFlowDeclaration*, const SymbolicFlowDeclaration*> flowRenaming;

    template<typename T, typename F>
    inline const T* GetOrCreate(std::map<const T*,const T*> map, const T* key, F create) {
        auto find = map.find(key);
        if (find != map.end()) return find->first;
        auto result = create();
        map.emplace(key, result);
        return result;
    }

    static inline const VariableDeclaration* Rename(const VariableDeclaration* decl) {
        return decl;
    }
    inline const SymbolicVariableDeclaration* Rename(const SymbolicVariableDeclaration* decl) {
        return GetOrCreate(symbolRenaming, decl, [decl,this](){ return &factory.GetUnusedSymbolicVariable(decl->type); });
    }
    inline const SymbolicFlowDeclaration* Rename(const SymbolicFlowDeclaration* decl) {
        return GetOrCreate(flowRenaming, decl, [decl,this](){ return &factory.GetUnusedFlowVariable(decl->type); });
    }
};

using Z3EncoderWithRenaming = Z3Encoder<SymbolicRenaming>;

inline bool UpdatesFlow(const HeapEffect& effect) { // TODO: copy from interference.cpp
    return &effect.pre->flow.get() != &effect.post->flow.get();
}

inline bool UpdatesField(const HeapEffect& effect, const std::string& field) { // TODO: copy from interference.cpp
    return &effect.pre->fieldToValue.at(field)->Decl() != &effect.post->fieldToValue.at(field)->Decl();
}

inline bool UpdateSubset(const HeapEffect& premise, const HeapEffect& conclusion) {
    if (UpdatesFlow(conclusion) && !UpdatesFlow(premise)) return false;
    const auto& fields = premise.pre->node->Type().fields;
    for (const auto& [field, type] : premise.pre->node->Type().fields) {
        if (UpdatesField(conclusion, field) && !UpdatesField(premise, field)) return false;
    }
    return true;
}

void AddEffectImplicationCheck(const HeapEffect& premise, const HeapEffect& conclusion, ImplicationCheckSet& checks, Z3EncoderWithRenaming& encoder, std::function<void()>&& eureka) {
    // give up for effects that contain resources in the context
    if (heal::ContainsResources(*premise.context) || heal::ContainsResources(*conclusion.context)) return;

    // ensure that the premise updates at least the fields updated by the conclusion (points-to predicates are not encoded)
    if (!UpdateSubset(premise, conclusion)) {
        std::cout << " % Effect implication (not checked):" << std::endl;
        std::cout << " %    premise effect   : " << *premise.pre << " ~~> " << *premise.post << std::endl;
        std::cout << " %                 under " << *premise.context << std::endl;
        std::cout << " %    conclusion effect: " << *conclusion.pre << " ~~> " << *conclusion.post << std::endl;
        std::cout << " %                 under " << *conclusion.context << std::endl;
        std::cout << " %    => update subset=" << UpdateSubset(premise, conclusion) << std::endl;
        return;
    }

    // encode
    auto premisePre = encoder(*premise.pre) && encoder(*premise.context);
    auto premisePost = encoder(*premise.post) && encoder(*premise.context);
    auto conclusionPre = encoder(*conclusion.pre) && encoder(*conclusion.context);
    auto conclusionPost = encoder(*conclusion.post) && encoder(*conclusion.context);
    auto samePre = encoder.MakeMemoryEquality(*premise.pre, *conclusion.pre);
    auto samePost = encoder.MakeMemoryEquality(*premise.post, *conclusion.post);

    // check
//    auto isImplied = z3::implies(samePre && samePost && premisePre, conclusionPre)
//                     && z3::implies(samePre && samePost && premisePost, conclusionPost); // TODO: correct?
    auto isImplied = z3::implies(samePre && samePost && conclusionPre, premisePre)
                     && z3::implies(samePre && samePost && conclusionPost, premisePost); // TODO: correct?
    checks.Add(isImplied, [call=std::move(eureka),&premise,&conclusion](bool holds){
//        if (!holds) return;
        std::cout << " % Effect implication (checked):" << std::endl;
        std::cout << " %    premise effect   : " << *premise.pre << " ~~> " << *premise.post << std::endl;
        std::cout << " %                 under " << *premise.context << std::endl;
        std::cout << " %    conclusion effect: " << *conclusion.pre << " ~~> " << *conclusion.post << std::endl;
        std::cout << " %                 under " << *conclusion.context << std::endl;
        std::cout << " %    => implied=" << holds << std::endl;
        std::cout << " %    => update subset=" << UpdateSubset(premise, conclusion) << std::endl;
        if (holds) call();
    });
}


std::vector<bool> DefaultSolver::ComputeEffectImplications(const std::deque<std::pair<const HeapEffect*, const HeapEffect*>>& implications) const {
    static Timer timer("DefaultSolver::ComputeEffectImplications");
    auto measurement = timer.Measure();

    std::vector<bool> result(implications.size(), false);
    z3::context context;
    z3::solver solver(context);
    ImplicationCheckSet checks(context);
    Z3EncoderWithRenaming encoder(context, solver);
    for (std::size_t index = 0; index != implications.size(); ++index) {
        auto& [premise, conclusion] = implications[index];
        AddEffectImplicationCheck(*premise, *conclusion, checks, encoder, [&result,index](){ result[index] = true; });
    }
    solver::ComputeImpliedCallback(solver, checks);
    return result;
}

inline bool ImpliesSpecification(const Annotation& premise, const Annotation& conclusion) {
    auto oblPremise = heal::CollectObligations(premise);
    for (const auto* obl : heal::CollectObligations(conclusion)) {
        auto find = std::find_if(oblPremise.begin(), oblPremise.end(), [obl](const auto* elem){
            return obl->kind == elem->kind && &obl->key->Decl() == &elem->key->Decl();
        });
        if (find == oblPremise.end()) return false;
    }
    auto fulPremise = heal::CollectFulfillments(premise);
    for (const auto* ful : heal::CollectFulfillments(conclusion)) {
        auto find = std::find_if(fulPremise.begin(), fulPremise.end(), [ful](const auto* elem){
            return ful->kind == elem->kind && &ful->key->Decl() == &elem->key->Decl() && &ful->return_value == &elem->return_value;
        });
        if (find == fulPremise.end()) return false;
    }
    return true;
}

Solver::Result DefaultSolver::Implies(const Annotation& premise, const Annotation& conclusion) const {
    static Timer timer("DefaultSolver::Implies");
    auto measurement = timer.Measure();

    std::cout << " IMPLICATION CHECK: " << premise << conclusion << std::endl;

    // TODO: handle futures / histories

    // prune resource mismatches
    auto memoryConclusion = heal::CollectMemory(conclusion);
    auto variablesConclusion = heal::CollectVariables(conclusion);
    if (memoryConclusion.size() != heal::CollectMemory(premise).size()) return Solver::NO;
    if (variablesConclusion.size() != heal::CollectVariables(premise).size()) return Solver::NO;
    if (memoryConclusion.size() > variablesConclusion.size()) return Solver::NO;
    if (!ImpliesSpecification(premise, conclusion)) return Solver::NO;

    // create memory map
    std::map<const SymbolicVariableDeclaration*, const VariableDeclaration*> conclusionMemAdrToVar;
    std::map<const PointsToAxiom*, const PointsToAxiom*> memoryMap;
    for (const auto* conclusionVariable : variablesConclusion) {
        // get memory resource in conclusion
        auto conclusionMemory = solver::FindMemory(conclusionVariable->value->Decl(), conclusion);
        if (!conclusionMemory) continue;
        conclusionMemAdrToVar[&conclusionMemory->node->Decl()] = &conclusionVariable->variable->decl;

        // match memory resource against premise
        auto premiseVariable = solver::FindResource(conclusionVariable->variable->decl, premise);
        if (!premiseVariable) return Solver::NO;
        auto premiseMemory = solver::FindMemory(premiseVariable->value->Decl(), premise);
        if (!premiseMemory) return Solver::NO;
        auto insertion = memoryMap.emplace(conclusionMemory, premiseMemory);
        assert(insertion.second);
    }
    if (memoryMap.size() != memoryConclusion.size()) return Solver::NO;

    // encode
    z3::context context;
    z3::solver solver(context);
    Z3Encoder premiseEncoder(context, solver);
    Z3EncoderWithRenaming conclusionEncoder(context, solver);
    conclusionEncoder.renaming.factory = SymbolicFactory(premise); // TODO: this hack should be avoided

    // encode and check now
    z3::expr_vector checkPremise(context);
    checkPremise.push_back(premiseEncoder(premise));
    for (const auto& [memoryPost, memoryPre] : memoryMap) {
        checkPremise.push_back(premiseEncoder.MakeMemoryEquality(*memoryPre, *memoryPost, conclusionEncoder));
    }
    bool nowImplicationHolds = ComputeImplied(solver, z3::mk_and(checkPremise), conclusionEncoder(conclusion));
    std::cout << "  ==> now-implication was solved, holds=" << nowImplicationHolds << std::endl;
    if (!nowImplicationHolds) return Solver::NO;

    // collect past memories
    auto getPastMemory = [](const Annotation& annotation) {
        std::set<const PointsToAxiom*> result;
        for (const auto& time : annotation.time) {
            if (auto past = dynamic_cast<const PastPredicate*>(time.get())) {
                if (auto mem = dynamic_cast<const PointsToAxiom*>(past->formula.get())) {
                    result.insert(mem);
                    continue;
                }
            }
            throw std::logic_error("Cannot compute implication among annotations: unsupported time predicate"); // TODO: better error handling
        }
        return result;
    };
    auto pastMemConclusion = getPastMemory(conclusion);
    if (pastMemConclusion.empty()) return Solver::YES;
    auto pastMemPremise = getPastMemory(premise);

    // create past memory map
    std::map<const PointsToAxiom*, std::set<const PointsToAxiom*>> pastMap;
    for (const auto* memConclusion : pastMemConclusion) {
        auto find = conclusionMemAdrToVar.find(&memConclusion->node->Decl());
        if (find == conclusionMemAdrToVar.end()) return Solver::NO;

        auto premiseVariable = FindResource(*find->second, *premise.now);
        if (!premiseVariable) return Solver::NO;

        std::set<const PointsToAxiom*> matches;
        for (const auto* memPremise : pastMemPremise) {
            if (&memPremise->node->Decl() != &premiseVariable->value->Decl()) continue;
            matches.insert(memPremise);
        }

        if (matches.empty()) return Solver::NO;
        pastMap.emplace(memConclusion, std::move(matches));
    }
    assert(pastMap.size() <= pastMemConclusion.size());

    // check time (must avoid matching distinct nodes as it yields a contradiction, making the implication true)
    ImplicationCheckSet checks(context);
    std::set<const PointsToAxiom*> impliedTime;
    for (const auto& [memConclusion, candidates] : pastMap) {
        for (const auto* memPremise : candidates) {
            auto timeMemory = premiseEncoder.MakeMemoryEquality(*memPremise, *memConclusion, conclusionEncoder);
//            auto timeImpCheck = z3::implies(nowPremise && nowConclusion, timeMemory);
            auto timeImpCheck = z3::implies(premiseEncoder(*premise.now) && timeMemory, conclusionEncoder(*conclusion.now)); // TODO: is this correct?
            checks.Add(timeImpCheck, [&impliedTime,memConclusion=memConclusion,memPremise](bool holds){
                std::cout << "  ==> time-implication was solved for " << memConclusion->node->Decl() << "--" << memPremise->node->Decl() << ", holds=" << holds << std::endl;
                if (holds) impliedTime.insert(memConclusion);
            });
        }
    }
    solver::ComputeImpliedCallback(solver, checks);
    return pastMemConclusion.size() == impliedTime.size() ? Solver::YES : Solver::NO;
}


//Solver::Result DefaultSolver::Implies(const Annotation& premise, const Annotation& conclusion) const {
//    std::cout << ">>>>>>> implies" << std::endl;
//    auto interpolant = Solver::Join(heal::Copy(premise), heal::Copy(conclusion));
//    heal::RenameSymbolicSymbols(*interpolant, conclusion);
//    std::cout << "-------" << std::endl;
//    std::cout << "premise: " << premise << std::endl;
//    std::cout << "interpolant: " << *interpolant << std::endl;
//    std::cout << "conclusion: " << conclusion << std::endl;
//    std::cout << "<<<<<<< implies" << std::endl;
//
//    // TODO: handle futures / histories
//    if (!premise.time.empty() || !conclusion.time.empty()) {
//        return Solver::Result::NO;
//    }
//
//    // prune resource mismatches
//    auto memoryConclusion = heal::CollectMemory(conclusion);
//    auto variablesConclusion = heal::CollectVariables(conclusion);
//    if (memoryConclusion.size() != heal::CollectMemory(*interpolant).size()) return Solver::NO;
//    if (variablesConclusion.size() != heal::CollectVariables(*interpolant).size()) return Solver::NO;
//    if (memoryConclusion.size() > variablesConclusion.size()) return Solver::NO;
//    if (!ImpliesSpecification(*interpolant, conclusion)) return Solver::NO;
//    std::cout << "implies will be solved" << std::endl;
//
//    // create memory map
//    std::map<const PointsToAxiom*, const PointsToAxiom*> memoryMap;
//    for (const auto* conclusionVariable : variablesConclusion) {
//        // get memory resource in conclusion
//        auto conclusionMemory = solver::FindMemory(conclusionVariable->value->Decl(), conclusion);
//        if (!conclusionMemory) continue;
//
//        // match memory resource against interpolant
//        auto interpolantVariable = solver::FindResource(conclusionVariable->variable->decl, *interpolant);
//        if (!interpolantVariable) return Solver::NO;
//        auto interpolantMemory = solver::FindMemory(interpolantVariable->value->Decl(), *interpolant);
//        if (!interpolantMemory) return Solver::NO;
//        auto insertion = memoryMap.emplace(conclusionMemory, interpolantMemory);
//        assert(insertion.second);
//    }
//    if (memoryMap.size() != memoryConclusion.size()) return Solver::NO;
//    std::cout << "implies will be solved, definitely" << std::endl;
//
//    // encode
//    z3::context context;
//    z3::solver solver(context);
//    Z3Encoder encoder(context, solver);
//
//    z3::expr_vector checkPremise(context);
//    checkPremise.push_back(encoder(*interpolant));
//    for (const auto& [memory, other] : memoryMap) {
//        checkPremise.push_back(encoder.MakeMemoryEquality(*memory, *other));
//    }
//
//    auto result = ComputeImplied(solver, z3::mk_and(checkPremise), encoder(conclusion));
//    return result ? Solver::YES : Solver::NO;
//}