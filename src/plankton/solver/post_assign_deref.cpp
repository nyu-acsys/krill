#include "plankton/solver/post.hpp"

#include "plankton/logger.hpp" // TODO: delete
// #include "cola/util.hpp"
// #include "plankton/util.hpp"

using namespace cola;
using namespace plankton;


enum struct PurityStatus { PURE, INSERTION, DELETION, FAILED };

PurityStatus Combine(PurityStatus status, PurityStatus other) {
	if (status == other) return status;
	switch (status) {
		case PurityStatus::PURE: return other;
		case PurityStatus::FAILED: return status;
		default: break;
	}
	switch (other) {
		case PurityStatus::PURE: return status;
		case PurityStatus::FAILED: return other;
		default: break;
	}
	return PurityStatus::FAILED;
}

struct DerefPostComputer {
	PostInfo info;
	Encoder& encoder;
	const Dereference& lhs;
	const Expression& rhs;

	const Type& nodeType;
	std::size_t footprintSize = 0;

	z3::solver solver;
	ImplicationCheckerImpl checker;
	
	PurityStatus purityStatus = PurityStatus::PURE;
	std::optional<z3::expr> purityKey;

	z3::expr lhsNodeNow, lhsNodeNext;
	z3::expr lhsNow, lhsNext;
	z3::expr rhsNow, rhsNext;
	z3::expr qvNode, qvKey, qvOtherKey;

	DerefPostComputer(PostInfo info, const Dereference& lhs, const Expression& rhs);
	std::unique_ptr<Annotation> MakePost();

	bool IsHeapHomogeneous();
	std::size_t GetFootprintDepth();
	std::vector<std::string> OutgoingEdges();
	
	bool ImpliesInvariant(z3::expr node);
	bool IsNullOrOutflowUnchange(z3::expr node, std::string fieldname);
	z3::expr MakeAlreadyCoveredCheck(z3::expr node, std::size_t nodeId);
	bool IsAlreadyCovered(z3::expr node, std::size_t nodeId);
	void CheckAssumptionIntegrity(z3::expr node, std::size_t nodeId);
	void UpdatePurity(z3::expr node);

	void AddBaseRulesToSolver();
	void AddNonFootprintFlowRulesToSolver();
	z3::expr GetFootprintNode(std::size_t index);
	std::pair<z3::expr, std::size_t> MakeNewFootprintNode();
	std::deque<std::tuple<z3::expr, std::size_t, std::size_t>> InitalizeFootprintWithRoots();
	std::pair<z3::expr, std::size_t> ExtendFootprintWithSuccessor(z3::expr node, std::string selector);
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
	  nodeType(lhs.expr->type()),
	  solver(encoder.MakeSolver(Encoder::StepTag::NEXT)),
	  checker(encoder, solver, GetTrueAxiom(), Encoder::StepTag::NEXT),
	  lhsNodeNow(encoder.Encode(*lhs.expr)), lhsNodeNext(encoder.EncodeNext(*lhs.expr)),
	  lhsNow(encoder.Encode(lhs)), lhsNext(encoder.EncodeNext(lhs)),
	  rhsNow(encoder.Encode(rhs)), rhsNext(encoder.EncodeNext(rhs)),
	  qvNode(encoder.EncodeVariable(GetDummyDecl<Sort::PTR>())),
	  qvKey(encoder.EncodeVariable(GetDummyDecl<Sort::DATA, 0>())),
	  qvOtherKey(encoder.EncodeVariable(GetDummyDecl<Sort::DATA, 1>()))
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


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

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

	// rules
	solver.add(encoder.Encode(info.solver.GetInstantiatedRules()));
	solver.add(encoder.Encode(info.solver.GetInstantiatedInvariant()));
	solver.add(encoder.EncodeNext(info.solver.GetInstantiatedRules()));
	solver.add(encoder.EncodeNext(info.solver.GetInstantiatedInvariant()));

	// add pre
	solver.add(encoder.Encode(info.preNow));
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

std::pair<z3::expr, std::size_t> DerefPostComputer::MakeNewFootprintNode() {
	auto index = footprintSize++;
	return { GetFootprintNode(index), index };
}

bool DerefPostComputer::ImpliesInvariant(z3::expr node) {
	return checker.Implies(encoder.EncodeNextInvariant(*info.solver.config.invariant, node));
}

bool DerefPostComputer::IsNullOrOutflowUnchange(z3::expr node, std::string fieldname) {
	const auto& outflow = info.solver.config.flowDomain->GetOutFlowContains(fieldname);
	auto isNull = node == encoder.MakeNullPtr();
	auto isUnchanged = z3::forall(qvKey,
		encoder.EncodePredicate(outflow, node, qvKey) == encoder.EncodeNextPredicate(outflow, node, qvKey)
	);
	return checker.Implies(isNull || isUnchanged);
}

z3::expr DerefPostComputer::MakeAlreadyCoveredCheck(z3::expr node, std::size_t nodeId) {
	// TODO: do this or just return false?
	auto vec = encoder.MakeVector();
	for (std::size_t index = 0; index < nodeId; ++index) {
		vec.push_back(GetFootprintNode(index) == node);
	}
	return encoder.MakeOr(vec);
}

bool DerefPostComputer::IsAlreadyCovered(z3::expr node, std::size_t nodeId) {
	// TODO: do this or just return false?
	return checker.Implies(MakeAlreadyCoveredCheck(node, nodeId));
}

void DerefPostComputer::CheckAssumptionIntegrity(z3::expr node, std::size_t nodeId) {
	// for decreasing flows: nothing to do
	if (info.solver.config.flowDomain->IsFlowDecreasing()) return;

	// for non-decreasing flows: ensure loop-freedom of the footprint ==> ensure that 'node' is not yet covered
	// TODO: relax requirement fo non-decreasing flows ==> it is sufficient that the flow on loops is not increased
	if (checker.Implies(!MakeAlreadyCoveredCheck(node, nodeId))) return;
	
	// fail if assumptions violated
	throw SolvingError("Cannot handle non-decreasing flows in the presence of (potential) loops within the flow footprint.");
}

void DerefPostComputer::UpdatePurity(z3::expr node) {
	throw std::logic_error("not yet implemented: DerefPostComputer::UpdatePurity");
}

void DerefPostComputer::AddNonFootprintFlowRulesToSolver() {
	// the flow outside the footprint remains unchanged
	solver.add(z3::forall(qvNode, qvKey, encoder.MakeImplication(
		!MakeAlreadyCoveredCheck(qvNode, footprintSize),
		/* ==> */
		encoder.EncodeTransitionMaintainsFlow(qvNode, qvKey)
	)));
}

std::deque<std::tuple<z3::expr, std::size_t, std::size_t>> DerefPostComputer::InitalizeFootprintWithRoots() {
	auto footprintDepth = info.solver.config.flowDomain->GetFootprintDepth(info.preNow, lhs, rhs);
	std::deque<std::tuple<z3::expr, std::size_t, std::size_t>> result; // elements: node x nodeId x remainingDepth
	assert(footprintSize == 0);

	// primary root is 'lhs.expr'; flow of primary root remains unchanged (see DerefPostComputer::CheckAssumptionIntegrity)
	auto [root, rootId] = MakeNewFootprintNode();
	solver.add(root == lhsNodeNext);
	solver.add(z3::forall(qvKey, encoder.EncodeTransitionMaintainsFlow(root, qvKey)));
	result.emplace_back(root, rootId, footprintDepth);

	// secondary root is 'lhs' in 'info.preNow' (aka before the assignment); only needed for pointer assignments
	if (lhs.sort() == Sort::PTR) {
		if (footprintDepth == 0) {
				throw SolvingError("Could not handle heap update '" + AssignmentString(lhs, rhs)
					+ "': footprint too small; updates of pointer fields require a footprint depth of at least 1.");
		}

		auto [old, oldId] = MakeNewFootprintNode();
		solver.add(old == lhsNow);
		// TODO: do we know something more about 'old' that we could/should add?
		result.emplace_back(old, oldId, footprintDepth-1);
	}

	// done
	return result;
}

std::pair<z3::expr, std::size_t> DerefPostComputer::ExtendFootprintWithSuccessor(z3::expr node, std::string selector) {
	// make successor
	auto [successor, successorId] = MakeNewFootprintNode();

	// establish link between node and successor (or both null)
	auto encodedSelector = std::make_pair(&nodeType, selector);
	auto bothNull = node == encoder.MakeNullPtr() && successor == encoder.MakeNullPtr();
	auto bothConnected = encoder.EncodeNextHeap(node, encodedSelector, successor);
	solver.add(bothNull || bothConnected);

	// add flow rule (if 'node' not null): 'successor' receives what 'node' sends via 'selector'
	auto flowRule = z3::forall(qvKey, encoder.MakeImplication(
		encoder.EncodeNextPredicate(info.solver.config.flowDomain->GetOutFlowContains(selector), node, qvKey),
		/* ==> */
		encoder.EncodeNextFlow(successor, qvKey)
	));
	solver.add(encoder.MakeImplication(
		node != encoder.MakeNullPtr(), /* ==> */ flowRule
	));

	// ensure non-decreasing flow / loop-free footprint
	CheckAssumptionIntegrity(successor, successorId);

	// done
	return { successor, successorId };
}

void DerefPostComputer::ExploreAndCheckFootprint() {
	// init footprint / worklist
	std::deque<std::tuple<z3::expr, std::size_t, std::size_t>> worklist = InitalizeFootprintWithRoots();

	// work worklist
	while (!worklist.empty()) {
		auto [node, nodeId, remainingDepth] = worklist.front();
		worklist.pop_front();
		log() << "  working at " << remainingDepth << " for " << node << std::endl;

		// skip if already visited
		if (IsAlreadyCovered(node, nodeId))
			continue;
		log() << "    not yet covered" << std::endl;

		// ensure that invariant holds for node (invariant must handle NULL case itself)
		if (!ImpliesInvariant(node))
			throw SolvingError("Could not establish invariant for heap update '" + AssignmentString(lhs, rhs) + "': invariant violated.");
		log() << "    established invariant" << std::endl;

		// check specification
		UpdatePurity(node);

		// check if outflow is unchanged; if not, add successor to worklist
		for (auto selector : OutgoingEdges()) {
			log() << "    looking at selector: " << selector << std::endl;
			if (IsNullOrOutflowUnchange(node, selector))
				continue;
			log() << "       outflow potentially changed" << std::endl;

			if (remainingDepth == 0)
				throw SolvingError("Could not establish invariant for heap update '" + AssignmentString(lhs, rhs) + "': footprint too small.");

			log() << "      descending along " << selector << std::endl;
			auto [successor, successorId] = ExtendFootprintWithSuccessor(node, selector);
			worklist.emplace_back(successor, successorId, remainingDepth -1);
		}
	}
}

std::unique_ptr<ConjunctionFormula> DerefPostComputer::MakePostNow() {
	// log() << "**PRE**" << std::endl << info.preNow << std::endl;

	AddBaseRulesToSolver();
	ExploreAndCheckFootprint();
	AddNonFootprintFlowRulesToSolver();


	// TODO: obey info.implicationCheck and if set skip checks
	// TODO: keyset disjointness check
	// TODO: purity checks
	// TODO: obl/ful transformation


	std::size_t counter = 0;
	for (const auto& candidate : info.solver.GetCandidates()) {
		bool inPre = info.solver.Implies(info.preNow, candidate.GetCheck());
		bool inPost = checker.Implies(candidate.GetCheck());
		if (!inPre || inPost) continue;
		log() << "candidate: " << candidate.GetCheck() << "    ";
		log() << "pre=" << inPre << "  ";
		log() << "post=" << inPost << std::endl;
		++counter;
	}
	log() << " ==> " << counter << " differ" << std::endl;
	auto foobar = info.ComputeImpliedCandidates(checker);
	
	log() << "**POST**" << std::endl << *foobar << std::endl;

	throw std::logic_error("pointu breaku");
	throw std::logic_error("not yet implemented: DerefPostComputer::MakePostNow()");
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
std::unique_ptr<Annotation> DerefPostComputer::MakePostTime() {
	auto result = std::make_unique<Annotation>();
	result->time = info.CopyPreTime();
	return result;
}
