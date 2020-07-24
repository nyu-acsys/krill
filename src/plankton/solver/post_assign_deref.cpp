#include "plankton/solver/post.hpp"

#include "plankton/logger.hpp" // TODO: delete
// #include "cola/util.hpp"
// #include "plankton/util.hpp"

using namespace cola;
using namespace plankton;
using StepTag = Encoder::StepTag;


enum struct PurityStatus { PURE, INSERTION, DELETION };

struct Node {
	std::size_t id;
	z3::expr expr;
	Node(std::size_t id, z3::expr expr) : id(id), expr(expr) {}
	operator z3::expr() const { return expr; }
};

template<>
Logger& Logger::operator<<(const Node& node) {
	std::cout << "[" << node.id << ": " << node.expr << "]";
	return *this;
}


struct DerefPostComputer {
	PostInfo info;
	Encoder& encoder;
	const Dereference& lhs;
	const Expression& rhs;

	bool performInvariantCheck;
	bool performSpecificationCheck;
	const Type& nodeType;
	std::size_t footprintSize = 0;

	z3::solver solver;
	ImplicationCheckerImpl checker;

	z3::expr lhsNodeNow, lhsNodeNext;
	z3::expr lhsNow, lhsNext;
	z3::expr rhsNow, rhsNext;
	z3::expr qvNode, qvKey;

	PurityStatus purityStatus = PurityStatus::PURE;
	z3::expr impureKey;

	DerefPostComputer(PostInfo info, const Dereference& lhs, const Expression& rhs);
	std::unique_ptr<Annotation> MakePost();

	void Throw(std::string reason, std::string explanation="");
	bool IsHeapHomogeneous();
	std::size_t GetFootprintDepth();
	std::vector<std::string> OutgoingEdges();
	
	void CheckInvariant(Node node);
	bool IsNullOrOutflowUnchanged(Node node, std::string fieldname);
	z3::expr MakeAlreadyCoveredCheck(Node node);
	bool IsAlreadyCovered(Node node);
	void CheckAssumptionIntegrity(Node node);
	void CheckSpecification(Node node);

	void AddBaseRulesToSolver();
	void AddNonFootprintFlowRulesToSolver();
	void AddSpecificationRulesToSolver();
	void AddPointerAliasesToSolver(Node node);
	void AddFlowRulesToSolver(Node node, std::string selector, Node successor, StepTag tag);
	z3::expr GetFootprintNode(std::size_t index);
	Node MakeNewFootprintNode();
	Node ExtendFootprintWithSuccessor(Node node, std::string selector, StepTag tag);
	void ExploreAndCheckFootprintFromSuccessor(Node node, std::string selector, std::size_t remainingDepth, StepTag tag);
	void ExploreAndCheckFootprintFromNode(Node node, std::size_t remainingDepth);
	void ExploreAndCheckFootprint();

	std::unique_ptr<ConjunctionFormula> MakePostNow();
	std::unique_ptr<Annotation> MakePostTime();
};

std::unique_ptr<Annotation> DerefPostComputer::MakePost() {
	log() << std::endl << "ΩΩΩ POST for assignment: " << AssignmentString(lhs, rhs) << std::endl;
	if (info.solver.ImpliesFalseQuick(info.preNow)) {
		log() << "    => pruning: premise is false" << std::endl;
		return Annotation::make_false();
	}

	auto postNow = MakePostNow();
	auto result = MakePostTime();
	result->now = std::move(postNow);
	// log() << info.preNow << std::endl << " ~~>" << std::endl << *result->now << std::endl;
	return result;
}

std::unique_ptr<Annotation> plankton::MakeDerefAssignPost(PostInfo info, const cola::Dereference& lhs, const cola::Expression& rhs) {
	return DerefPostComputer(std::move(info), lhs, rhs).MakePost(); // TODO: get config
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

std::string SortToString(Sort sort) {
	switch (sort) {
		case Sort::VOID: return "VOID";
		case Sort::BOOL: return "BOOL";
		case Sort::DATA: return "DATA";
		case Sort::PTR: return "PTR";
	}
}

template<Sort S, std::size_t I = 0>
const VariableDeclaration& GetDummyDecl() {
	static Type dummyType("dummy-post-deref-" + SortToString(S) + "-type", Sort::PTR);
	static VariableDeclaration dummyDecl("post-deref#dummy-" + SortToString(S) + "-var" + std::to_string(I), dummyType, false);
	return dummyDecl;
}

const ExpressionAxiom& GetTrueAxiom() {
	static ExpressionAxiom trueAxiom(std::make_unique<BooleanValue>(true));
	return trueAxiom;
}

DerefPostComputer::DerefPostComputer(PostInfo info_, const Dereference& lhs, const Expression& rhs)
	: info(std::move(info_)), encoder(info.solver.GetEncoder()), lhs(lhs), rhs(rhs),
	  performInvariantCheck(!info.implicationCheck), performSpecificationCheck(!info.implicationCheck), nodeType(lhs.expr->type()),
	  solver(encoder.MakeSolver(StepTag::NEXT)),
	  checker(encoder, solver, GetTrueAxiom(), StepTag::NEXT),
	  lhsNodeNow(encoder.Encode(*lhs.expr)), lhsNodeNext(encoder.EncodeNext(*lhs.expr)),
	  lhsNow(encoder.Encode(lhs)), lhsNext(encoder.EncodeNext(lhs)),
	  rhsNow(encoder.Encode(rhs)), rhsNext(encoder.EncodeNext(rhs)),
	  qvNode(encoder.EncodeVariable(GetDummyDecl<Sort::PTR>())),
	  qvKey(encoder.EncodeVariable(GetDummyDecl<Sort::DATA, 0>())),
	  impureKey(encoder.EncodeVariable(GetDummyDecl<Sort::DATA, 1>()))
{
	if (&nodeType != &info.solver.config.flowDomain->GetNodeType()) {
		std::stringstream msg;
		msg << "Flow domain incompatible with assignment '";
		msg << AssignmentString(lhs, rhs) + "': '";
		cola::print(*lhs.expr, msg);
		msg << "' is of type '" << lhs.expr->type().name << "', ";
		msg << "but flow domain is defined over type '";
		msg << info.solver.config.flowDomain->GetNodeType().name << "'.";
		throw SolvingError(msg.str());
	}
	if (!IsHeapHomogeneous()) {
		throw SolvingError("Cannot handle non-homogeneous heaps. Sorry.");
		// TODO: explain 'non-homogeneous'
	}
}

void DerefPostComputer::Throw(std::string reason, std::string explanation) {
	std::stringstream msg;
	msg << reason + " '" + AssignmentString(lhs, rhs) + "'";
	if (explanation != "") msg << ": " << explanation;
	msg << ".";
	throw SolvingError(msg.str());
}

bool DerefPostComputer::IsHeapHomogeneous() {
	for (auto field : OutgoingEdges()) {
		if (nodeType.field(field) != &nodeType) {
			return false;
		}
	}
	return true;
}

std::size_t DerefPostComputer::GetFootprintDepth() {
	return info.solver.config.flowDomain->GetFootprintDepth(info.preNow, lhs, rhs);
}

std::vector<std::string> DerefPostComputer::OutgoingEdges() {
	// TODO: reuse code from encoder?
	std::vector<std::string> result;
	for (auto [fieldName, fieldType] : nodeType.fields) {
		if (fieldType.get().sort != Sort::PTR) continue;
		result.push_back(fieldName);
	}
	return result;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<Annotation> DerefPostComputer::MakePostTime() {
	auto result = std::make_unique<Annotation>();
	result->time = info.CopyPreTime();
	return result;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void DerefPostComputer::AddBaseRulesToSolver() {
	// ownership
	if (rhs.sort() == Sort::PTR) {
		solver.add(z3::forall(qvNode, encoder.MakeImplication(
			qvNode != rhsNow, /* ==> */ encoder.EncodeTransitionMaintainsOwnership(qvNode)
		)));
		solver.add(encoder.EncodeNextOwnership(rhsNext, false));
	} else {
		solver.add(z3::forall(qvNode, encoder.EncodeTransitionMaintainsOwnership(qvNode)));
	}

	// heap
	solver.add(z3::forall(qvNode, encoder.MakeImplication(
		qvNode != lhsNodeNow, /* ==> */ encoder.EncodeTransitionMaintainsHeap(qvNode, nodeType)
	)));
	solver.add(encoder.EncodeTransitionMaintainsHeap(lhsNodeNow, nodeType, { lhs.fieldname }));
	solver.add(lhsNext == rhsNow);

	// stack
	for (auto var : info.solver.GetVariablesInScope()) {
		solver.add(encoder.EncodeVariable(*var) == encoder.EncodeNextVariable(*var));
	}

	// flow
	// forall node,selector,successor,key:  node->{selector}=successor /\ key_flows_out(node, key) ==> key in flow(successor)
	// for (auto selector : OutgoingEdges()) {
	// 	for (auto tag : { StepTag::NOW }) {
	// 		auto encodedSelector = std::make_pair(&nodeType, selector);
	// 		auto successor = encoder.EncodeVariable(GetDummyDecl<Sort::PTR, 1>());
	// 		// TODO: have a variable for the successor
	// 		solver.add(z3::forall(qvNode, successor, qvKey, encoder.MakeImplication(
	// 			(successor != encoder.MakeNullPtr()) &&
	// 			encoder.EncodeHeap(successor, encodedSelector, successor, tag) &&
	// 			encoder.EncodePredicate(info.solver.config.flowDomain->GetOutFlowContains(selector), qvNode, qvKey, tag),
	// 			/* ==> */
	// 			encoder.EncodeFlow(successor, qvKey, true, tag)
	// 		)));
	// 	}
	// }

	// for (auto [fieldName, fieldType] : GetNodeTypePointerFields()) {
	// 	auto key = EncodeVariable(Sort::DATA, "qv-flow-" + fieldName + "-key", tag);
	// 	auto node = EncodeVariable(nodeType.sort, "qv-flow-" + fieldName + "-node", tag);
	// 	auto successor = EncodeVariable(fieldType->sort, "qv-flow-" + fieldName + "-successor", tag);
	// 	auto selector = std::make_pair(&nodeType, fieldName);

	// 	solver.add(z3::forall(node, key, successor, MakeImplication(
	// 		EncodeHeap(node, selector, successor, tag) &&
	// 		EncodeHasFlow(node, tag) &&
	// 		EncodePredicate(postConfig.flowDomain->GetOutFlowContains(fieldName), node, key, tag),
	// 		/* ==> */
	// 		EncodeFlow(successor, key, tag)
	// 	)));
	// }

	// rules
	solver.add(encoder.Encode(info.solver.GetInstantiatedRules()));
	solver.add(encoder.Encode(info.solver.GetInstantiatedInvariant()));
	solver.add(encoder.EncodeNext(info.solver.GetInstantiatedRules()));
	solver.add(encoder.EncodeNext(info.solver.GetInstantiatedInvariant()));

	// add pre
	solver.add(encoder.Encode(info.preNow));
}

void DerefPostComputer::AddNonFootprintFlowRulesToSolver() {
	// the flow outside the footprint remains unchanged
	solver.add(z3::forall(qvNode, qvKey, encoder.MakeImplication(
		!MakeAlreadyCoveredCheck(Node(footprintSize, qvNode)),
		/* ==> */
		encoder.EncodeTransitionMaintainsFlow(qvNode, qvKey)
	)));
}

void DerefPostComputer::AddSpecificationRulesToSolver() {
	if (!performSpecificationCheck) return;

	switch (purityStatus) {
		case PurityStatus::PURE:
			for (auto kind : SpecificationAxiom::KindIteratable) {
				solver.add(z3::forall(qvKey, encoder.MakeImplication(
					encoder.EncodeObligation(kind, qvKey), /* ==> */ encoder.EncodeNextObligation(kind, qvKey)
				)));
				for (bool value : { true, false }) {
					solver.add(z3::forall(qvKey, encoder.MakeImplication(
						encoder.EncodeFulfillment(kind, qvKey, value), /* ==> */ encoder.EncodeNextFulfillment(kind, qvKey, value)
					)));
				}
			}
			break;

		case PurityStatus::INSERTION:
			solver.add(encoder.MakeImplication(
				encoder.EncodeObligation(SpecificationAxiom::Kind::INSERT, impureKey),
				/* ==> */
				encoder.EncodeNextFulfillment(SpecificationAxiom::Kind::INSERT, impureKey, true)
			));
			break;

		case PurityStatus::DELETION:
			solver.add(encoder.MakeImplication(
				encoder.EncodeObligation(SpecificationAxiom::Kind::DELETE, impureKey),
				/* ==> */
				encoder.EncodeNextFulfillment(SpecificationAxiom::Kind::DELETE, impureKey, true)
			));
			break;
	}
}

z3::expr DerefPostComputer::GetFootprintNode(std::size_t index) {
	static Type dummyType("dummy-post-deref-node-type", Sort::PTR);
	static std::vector<std::unique_ptr<VariableDeclaration>> dummyPointers;
	while (index >= dummyPointers.size()) {
		auto newDecl = std::make_unique<VariableDeclaration>("post-deref#footprint" + std::to_string(dummyPointers.size()), dummyType, false);
		dummyPointers.push_back(std::move(newDecl));
	}

	assert(index < dummyPointers.size());
	return encoder.EncodeNextVariable(*dummyPointers.at(index));
}

Node DerefPostComputer::MakeNewFootprintNode() {
	auto index = footprintSize++;
	return Node(index, GetFootprintNode(index));
}

void DerefPostComputer::CheckInvariant(Node node) {
	if (!performInvariantCheck) return;

	// invariant must handle NULL case itself
	auto check = encoder.EncodeNextInvariant(*info.solver.config.invariant, node);
	if (!checker.Implies(check)) {
		log() << std::endl << std::endl << "Invariant violated in footprint: " << node << std::endl;
		log() << std::endl << "PRE: " << info.preNow << std::endl;
		Throw("Could not establish invariant for heap update", "invariant violated within footprint");
	}
}

bool DerefPostComputer::IsNullOrOutflowUnchanged(Node node, std::string fieldname) {
	const auto& outflow = info.solver.config.flowDomain->GetOutFlowContains(fieldname);
	auto root = GetFootprintNode(0);
	auto isNull = node == encoder.MakeNullPtr();
	// auto isUnchanged = z3::forall(qvKey, encoder.MakeEquivalence(
	// 	encoder.EncodePredicate(outflow, node, qvKey), /* <==> */ encoder.EncodeNextPredicate(outflow, node, qvKey)
	// ));
	auto isUnchanged = z3::forall(qvKey, encoder.MakeImplication(
		encoder.EncodeFlow(root, qvKey), // TODO: now or next? should not matter
		/* ==> */
		encoder.EncodePredicate(outflow, node, qvKey) == encoder.EncodeNextPredicate(outflow, node, qvKey)
	));
	return checker.Implies(isNull || isUnchanged);
	// TODO: enforce footprint not contain NULL?
	// TODO important: kann es sein, dass er nicht weiß wie der alte Flow ist? ==> alten Flow reincode oder nochmal universelle Flow-Regel probieren
}

z3::expr DerefPostComputer::MakeAlreadyCoveredCheck(Node node) {
	// TODO: do this or just return false?
	auto vec = encoder.MakeVector();
	for (std::size_t index = 0; index < node.id; ++index) {
		vec.push_back(GetFootprintNode(index) == node);
	}
	return encoder.MakeOr(vec);
}

bool DerefPostComputer::IsAlreadyCovered(Node node) {
	// TODO: do this or just return false?
	auto check = MakeAlreadyCoveredCheck(node);
	auto result = checker.Implies(check);
	if (result) solver.add(check);
	return result;
}

void DerefPostComputer::CheckAssumptionIntegrity(Node node) {
	// for decreasing flows: nothing to do
	if (info.solver.config.flowDomain->IsFlowDecreasing()) return;

	// for non-decreasing flows: ensure loop-freedom of the footprint ==> ensure that 'node' is not yet covered
	// TODO: relax requirement for non-decreasing flows ==> it is sufficient that the flow on loops is not increased
	if (checker.Implies(!MakeAlreadyCoveredCheck(node))) return;
	
	// fail if assumptions violated
	throw SolvingError("Cannot handle non-decreasing flows in the presence of (potential) loops within the flow footprint.");
}

void DerefPostComputer::CheckSpecification(Node node) {
	if (!performSpecificationCheck) return;

	// helpers
	auto NodeContains = [&,this](z3::expr key, StepTag tag) {
		return encoder.EncodePredicate(*info.solver.config.logicallyContainsKey, node, key, tag) && encoder.EncodeKeysetContains(node, key, tag);
	};
	auto IsInserted = [&](z3::expr key) { return !NodeContains(key, StepTag::NOW) && NodeContains(key, StepTag::NEXT); };
	auto IsDeleted = [&](z3::expr key) { return NodeContains(key, StepTag::NOW) && !NodeContains(key, StepTag::NEXT); };
	auto IsUnchanged = [&](z3::expr key) { return encoder.MakeEquivalence(NodeContains(key, StepTag::NOW), NodeContains(key, StepTag::NEXT)); };

	// check purity
	auto pureCheck = z3::forall(qvKey, IsUnchanged(qvKey));
	if (checker.Implies(pureCheck)) return;
	if (purityStatus != PurityStatus::PURE)
		Throw("Bad impure heap update", "not the first impure heap update");

	// check impure, part 1: nothing but 'impureKey' is changed
	solver.add(!IsUnchanged(impureKey));
	auto othersUnchangedCheck = z3::forall(qvKey, encoder.MakeImplication(
		qvKey != impureKey, /* ==> */ IsUnchanged(qvKey)
	));
	if (!checker.Implies(othersUnchangedCheck))
		Throw("Specification violated by impure heap update", "multiple keys inserted/deleted at once");

	// check impure, part 2: check insertion/deletion for 'impureKey'
	assert(!(checker.Implies(IsInserted(impureKey)) && checker.Implies(IsDeleted(impureKey))));
	if (checker.Implies(IsInserted(impureKey))) purityStatus = PurityStatus::INSERTION;
	else if (checker.Implies(IsDeleted(impureKey))) purityStatus = PurityStatus::DELETION;
	else Throw("Failed to find specification for impure heap update", "unable to figure out if it is an insertion or a deletion");
}

void DerefPostComputer::AddPointerAliasesToSolver(Node node) {
	for (auto variable : info.solver.GetVariablesInScope()) {
		auto alias = encoder.EncodeVariable(*variable);
		if (checker.Implies(alias == node)) {
			solver.add(alias == node);
			// TODO: also add NEXT version of alias?
			log() << "    aka: " << node << " = " << alias << std::endl;
		}
	}
}

void DerefPostComputer::AddFlowRulesToSolver(Node node, std::string selector, Node successor, StepTag desiredTag) {
	// TODO important: do we need to do the check 'node.id == 0' semantically??
	std::vector<StepTag> tags;
	if (node.id == 0) tags = { desiredTag };
	else tags = { StepTag::NOW, StepTag::NEXT };

	// add flow rule (if 'node' not null): 'successor' receives what 'node' sends via 'selector'
	for (auto tag : tags) {
		auto flowRule = z3::forall(qvKey, encoder.MakeImplication(
			encoder.EncodePredicate(info.solver.config.flowDomain->GetOutFlowContains(selector), node, qvKey, tag),
			/* ==> */
			encoder.EncodeFlow(successor, qvKey, true, tag)
		));
		solver.add(encoder.MakeImplication(
			node != encoder.MakeNullPtr(), /* ==> */ flowRule
		));
	}
}

Node DerefPostComputer::ExtendFootprintWithSuccessor(Node node, std::string selector, StepTag tag) {
	// make successor
	auto successor = MakeNewFootprintNode();

	// establish link between node and successor (or both null)
	auto encodedSelector = std::make_pair(&nodeType, selector);
	solver.add(encoder.EncodeHeap(node, encodedSelector, successor, tag));
	AddFlowRulesToSolver(node, selector, successor, tag);

	// ensure non-decreasing flow / loop-free footprint
	CheckAssumptionIntegrity(successor);

	// done
	return successor;
}

void DerefPostComputer::ExploreAndCheckFootprintFromSuccessor(Node node, std::string selector, std::size_t remainingDepth, StepTag tag) {
	if (remainingDepth == 0) Throw("Could not establish invariant for heap update", "footprint too small");
	
	auto successor = ExtendFootprintWithSuccessor(node, selector, tag);
	log() << "    descending from " << node << " along " << selector << " to " << successor << std::endl;
	ExploreAndCheckFootprintFromNode(successor, remainingDepth - 1); // tag is only relevant for roots; ignore in exploration
}

void DerefPostComputer::ExploreAndCheckFootprintFromNode(Node node, std::size_t remainingDepth) {
	log() << "  working at " << node << " for " << remainingDepth << std::endl;
	AddPointerAliasesToSolver(node);

	// skip if NULL or already visited
	if (checker.Implies(node == encoder.MakeNullPtr())) return;
	if (IsAlreadyCovered(node)) return;
	log() << "    not NULL and not yet covered" << std::endl;

	// check invariant / specification
	CheckInvariant(node);
	log() << "    established invariant" << std::endl;
	CheckSpecification(node);
	log() << "    established specification" << std::endl;

	// check if outflow is unchanged; if not, explore successor
	for (auto selector : OutgoingEdges()) {
		log() << "    looking at selector: " << selector << std::endl;
		if (IsNullOrOutflowUnchanged(node, selector))
			continue;
		log() << "      outflow potentially changed" << std::endl;

		ExploreAndCheckFootprintFromSuccessor(node, selector, remainingDepth, StepTag::NEXT);
	}
}

void DerefPostComputer::ExploreAndCheckFootprint() {
	//
	// TODO:
	// - first: explore the entire footprint up to specified depth, adding solver rules (including null??)
	// - second: check invariants and specification
	// - third: check outflow unchanged at boundary (only on edges *leaving* footprint)
	//

	auto footprintDepth = info.solver.config.flowDomain->GetFootprintDepth(info.preNow, lhs, rhs);

	// explore from primary footprint root 'lhs.expr'
	// flow of primary root remains unchanged ==> see DerefPostComputer::CheckAssumptionIntegrity
	auto root = MakeNewFootprintNode();
	assert(root.id == 0);
	solver.add(root == lhsNodeNext);
	solver.add(z3::forall(qvKey, encoder.EncodeTransitionMaintainsFlow(root, qvKey)));
	log() << "  <exploring root>" << std::endl;
	ExploreAndCheckFootprintFromNode(root, footprintDepth);

	// secondary roots are 'lhs' before and after the assignment
	if (lhs.sort() == Sort::PTR) {
		// TODO: skip this if there is no outflow from 'lhs.expr' via 'lhs.fieldname'
		//       ==> if (!checker.Implies(!encoder.EncodeHasFlow(lhsNodeNow))) return;  not sufficient! --> add isDecreasing

		// new successor may gain flow
		log() << "  <exploring secondary root: new successor>" << std::endl;
		ExploreAndCheckFootprintFromSuccessor(root, lhs.fieldname, footprintDepth, StepTag::NEXT);
		
		// old successor may loose flow
		log() << "  <exploring secondary root: old successor>" << std::endl;
		ExploreAndCheckFootprintFromSuccessor(root, lhs.fieldname, footprintDepth, StepTag::NOW);
	}
}

std::unique_ptr<ConjunctionFormula> DerefPostComputer::MakePostNow() {	
	AddBaseRulesToSolver();
	ExploreAndCheckFootprint();
	AddNonFootprintFlowRulesToSolver();
	AddSpecificationRulesToSolver();

	// TODO important: obey info.implicationCheck and if set skip checks
	// TODO important: keyset disjointness check within footprint
	std::cerr << "WARNING: missing disjointness check!!" << std::endl;

	return info.ComputeImpliedCandidates(checker);

	// std::size_t counter = 0;
	// for (const auto& candidate : info.solver.GetCandidates()) {
	// 	bool inPre = info.solver.Implies(info.preNow, candidate.GetCheck());
	// 	bool inPost = checker.Implies(candidate.GetCheck());
	// 	if (!inPre || inPost) continue;
	// 	log() << "candidate: " << candidate.GetCheck() << "    ";
	// 	log() << "pre=" << inPre << "  ";
	// 	log() << "post=" << inPost << std::endl;
	// 	++counter;
	// }
	// log() << " ==> " << counter << " differ" << std::endl;
}
