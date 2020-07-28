#include "plankton/solver/post.hpp"

#include "plankton/logger.hpp" // TODO: delete
// #include "cola/util.hpp"
// #include "plankton/util.hpp"

using namespace cola;
using namespace plankton;
using StepTag = Encoder::StepTag;


enum struct PurityStatus { PURE, INSERTION, DELETION };

inline bool IMP(bool a, bool b) { return !a || b; }

struct Node {
	std::size_t id;
	z3::expr expr;
	Node(std::size_t id, z3::expr expr) : id(id), expr(expr) {}
	operator z3::expr() const { return expr; }
	bool operator<(const Node& other) const {
		assert(IMP(this->id == other.id, this->expr == other.expr));
		return this->id < other.id;
	}
	bool operator==(const Node& other) const {
		assert(IMP(this->id == other.id, this->expr == other.expr));
		return this->id == other.id;
	}
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
	std::vector<std::unique_ptr<cola::Expression>> flowValues;

	z3::expr lhsNodeNow, lhsNodeNext;
	z3::expr lhsNow, lhsNext;
	z3::expr rhsNow, rhsNext;
	z3::expr qvNode, qvKey;

	std::set<Node> footprint;
	std::set<std::tuple<Node, std::string, Node>> footprintIntraface;
	std::set<std::tuple<Node, std::string>> footprintBoundary;
	PurityStatus purityStatus = PurityStatus::PURE;
	z3::expr impureKey;

	DerefPostComputer(PostInfo info, const Dereference& lhs, const Expression& rhs);
	std::unique_ptr<Annotation> MakePost();

	void Throw(std::string reason, std::string explanation="");
	bool IsHeapHomogeneous();
	std::size_t GetFootprintDepth();
	std::vector<std::string> OutgoingEdges();
	
	bool Implies(z3::expr expr);
	bool ImpliesIsNull(Node node);
	bool ImpliesIsNull(Node node, std::string selector, StepTag tag);
	bool ImpliesIsNonNull(Node node);void AddBaseRulesToSolver();
	z3::expr MakeFootprintContainsCheck(z3::expr node);
	
	void AddNonFootprintFlowRulesToSolver();
	void AddSpecificationRulesToSolver();
	void AddPointerAliasesToSolver(Node node);
	void AddFlowRulesToSolver(Node node, std::string selector, Node successor, StepTag tag);

	void CheckAssumptionIntegrity(Node node);
	void AddEdgeToBoundary(Node node, std::string selector);
	void AddEdgeToIntraface(Node node, std::string selector, Node successor);
	bool AddToFootprint(Node node);
	z3::expr GetNode(std::size_t index);
	Node MakeNewNode();
	Node MakeFootprintRoot();
	bool ExtendFootprint(Node node, std::string selector, StepTag tag);
	bool ExtendFootprint(Node node, std::string selector);
	void InitializeFootprint();

	void CheckInvariant(Node node);
	void CheckSpecification(Node node);
	void CheckFootprint(Node node);
	bool IsOutflowUnchanged(Node node, std::string fieldname);

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

static std::string SortToString(Sort sort) {
	switch (sort) {
		case Sort::VOID: return "VOID";
		case Sort::BOOL: return "BOOL";
		case Sort::DATA: return "DATA";
		case Sort::PTR: return "PTR";
	}
}

template<Sort S, std::size_t I = 0>
static const VariableDeclaration& GetDummyDecl() {
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
	  flowValues(info.solver.MakeCandidateExpressions(Sort::DATA)), // TODO: flow domain should tell us the type of flow values?
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

bool DerefPostComputer::Implies(z3::expr expr) {
	auto result = checker.Implies(expr);
	if (result) solver.add(expr);
	return result;
}

bool DerefPostComputer::ImpliesIsNull(Node node) {
	return Implies(node == encoder.MakeNullPtr());
}

bool DerefPostComputer::ImpliesIsNull(Node node, std::string selector, StepTag tag) {
	auto encodedSelector = std::make_pair(&nodeType, selector);
	auto next = encoder.EncodeHeap(node, encodedSelector, tag);
	return Implies(next == encoder.MakeNullPtr());
}

bool DerefPostComputer::ImpliesIsNonNull(Node node) {
	return Implies(node != encoder.MakeNullPtr());
}

z3::expr DerefPostComputer::MakeFootprintContainsCheck(z3::expr node) {
	auto vec = encoder.MakeVector();
	for (auto other : footprint) {
		vec.push_back(node == other);
	}
	return encoder.MakeOr(vec);
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

	// rules
	solver.add(encoder.Encode(info.solver.GetInstantiatedRules()));
	solver.add(encoder.EncodeNext(info.solver.GetInstantiatedRules()));
	solver.add(encoder.Encode(info.solver.GetInstantiatedInvariant()));
	// solver.add(encoder.EncodeNext(info.solver.GetInstantiatedInvariant()));

	// add pre
	solver.add(encoder.Encode(info.preNow));


	// flow: only flow that comes from the root may change // TODO important: correct???
	// throw std::logic_error("try without first"); // <<<<<<<<<<<<<<<=====================================================================|||||||
	// solver.add(z3::forall(qvNode, qvKey, encoder.MakeImplication(
	// 	encoder.EncodeFlow(GetNode(0), qvKey, false), // TODO: now or next? should not matter
	// 	/* ==> */
	// 	encoder.EncodeTransitionMaintainsFlow(qvNode, qvKey)
	// )));
}

void DerefPostComputer::AddNonFootprintFlowRulesToSolver() {
	// the flow outside the footprint remains unchanged
	solver.add(z3::forall(qvNode, qvKey, encoder.MakeImplication(
		!MakeFootprintContainsCheck(qvNode),
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

void DerefPostComputer::AddPointerAliasesToSolver(Node node) {
	for (auto variable : info.solver.GetVariablesInScope()) {
		auto alias = encoder.EncodeVariable(*variable);
		if (Implies(alias == node)) {
			// TODO: also add NEXT version of alias?
			log() << "    alias: " << node << " = " << alias << std::endl;
		}
	}
}

void DerefPostComputer::AddFlowRulesToSolver(Node node, std::string selector, Node successor, StepTag desiredTag) {
	// log() << "    !not adding flow rules!" << std::endl;
	// return;

	std::vector<StepTag> tags = { desiredTag };
	if (selector != lhs.fieldname || Implies(node != GetNode(0))) {
		tags = { StepTag::NOW, StepTag::NEXT };
	}

	// add flow rule (if 'node' not null): 'successor' receives what 'node' sends via 'selector'
	auto& edgeFunction = info.solver.config.flowDomain->GetOutFlowContains(selector);
	for (auto tag : tags) {
		log() << "    flow rule: " << node << " -> " << selector << " ~~> " << successor << "   " << (tag==StepTag::NOW ? "(now)" : "(next)") << std::endl;
		auto flowRule = z3::forall(qvKey, encoder.MakeImplication(
			encoder.EncodePredicate(edgeFunction, node, qvKey, tag),
			/* ==> */
			encoder.EncodeFlow(successor, qvKey, true, tag)
		));
		solver.add(encoder.MakeImplication(
			node != encoder.MakeNullPtr(), /* ==> */ flowRule
		));
	}

	// for (auto tag : tags) {
	// 	log() << "    flow rule: " << node << " -> " << selector << " ~~> " << successor << "   " << (tag==StepTag::NOW ? "(now)" : "(next)") << std::endl;
	// 	auto flowRule = z3::forall(qvKey, encoder.MakeImplication(
	// 		encoder.EncodeFlow(GetNode(0), qvKey, false) &&
	// 		encoder.EncodePredicate(edgeFunction, node, qvKey, tag),
	// 		/* ==> */
	// 		encoder.EncodeFlow(successor, qvKey, true, tag)
	// 	));
	// 	solver.add(encoder.MakeImplication(
	// 		node != encoder.MakeNullPtr(), /* ==> */ flowRule
	// 	));
	// }

	// for (auto tag : tags) {
	// 	for (const auto& value : flowValues) {
	// 		for (auto otherTag : { StepTag::NOW, StepTag::NEXT }) {
	// 			log() << "    flow rule: " << node << " -> " << selector << " ~~> " << successor << "   " << *value << "  " << (tag==StepTag::NOW ? "(now)" : "(next)") << std::endl;
	// 			auto encodedValue = encoder.Encode(*value, otherTag);
	// 			auto flowRule = encoder.MakeImplication(
	// 				encoder.EncodePredicate(edgeFunction, node, encodedValue, tag),
	// 				/* ==> */
	// 				encoder.EncodeFlow(successor, encodedValue, true, tag)
	// 			);
	// 			solver.add(encoder.MakeImplication(
	// 				node != encoder.MakeNullPtr(), /* ==> */ flowRule
	// 			));
	// 		}
	// 	}
	// }
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void DerefPostComputer::CheckAssumptionIntegrity(Node node) {
	// for decreasing flows: nothing to do
	if (info.solver.config.flowDomain->IsFlowDecreasing()) return;

	// for non-decreasing flows: ensure loop-freedom of the footprint ==> ensure that 'node' is not yet covered
	// TODO: relax requirement for non-decreasing flows ==> it is sufficient that the flow on loops is not increased
	if (Implies(!MakeFootprintContainsCheck(node))) return;
	
	// fail if assumptions violated
	throw SolvingError("Cannot handle non-decreasing flows in the presence of (potential) loops within the flow footprint.");
}


void DerefPostComputer::AddEdgeToBoundary(Node node, std::string selector) {
	// TODO important: do not add to boundary if already in interface?
	footprintBoundary.emplace(node, selector);
}

void DerefPostComputer::AddEdgeToIntraface(Node node, std::string selector, Node successor) {
	auto insertion = footprintIntraface.emplace(node, selector, successor);
	if (insertion.second) AddFlowRulesToSolver(node, selector, successor, StepTag::NEXT);
}

bool DerefPostComputer::AddToFootprint(Node node) {
	// NULL cannot be added to the footprint
	if (!ImpliesIsNonNull(node)) {
		return false;
	}

	// TODO: correct/needed?
	CheckAssumptionIntegrity(node);

	// check if we already have an alias for node
	for (auto existingNode : footprint) {
		z3::expr equal = existingNode.expr == node.expr;
		bool areEqual = Implies(equal);
		bool areUnequal = !areEqual && Implies(!equal);
		if (!areEqual && !areUnequal)
			Throw("Cannot create footprint for heap update", "abstraction too imprecise");
		assert(!areEqual);
	}
	footprint.insert(node);

	// node satisfied the invariant before the update
	solver.add(encoder.EncodeInvariant(*info.solver.config.invariant, node));

	// add outgoing edges to boundary
	for (auto selector : OutgoingEdges()) {
		AddEdgeToBoundary(node, selector);
	}

	// find new intraface edges
	std::vector<std::pair<Node, std::string>> toBeDeleted;
	for (auto [other, selector] : footprintBoundary) {
		auto encodedSelector = std::make_pair(&nodeType, selector);
		auto pointsToNode = encoder.EncodeHeap(other, encodedSelector, node, StepTag::NEXT);
		if (Implies(pointsToNode)) {
			AddEdgeToIntraface(other, selector, node);
			toBeDeleted.emplace_back(other, selector);
		}
	}
	for (auto pair : toBeDeleted) {
		footprintBoundary.erase(pair);
	}

	// done
	return true;
}

z3::expr DerefPostComputer::GetNode(std::size_t index) {
	static Type dummyType("dummy-post-deref-node-type", Sort::PTR);
	static std::vector<std::unique_ptr<VariableDeclaration>> dummyPointers;
	while (index >= dummyPointers.size()) {
		auto newDecl = std::make_unique<VariableDeclaration>("post-deref#footprint" + std::to_string(dummyPointers.size()), dummyType, false);
		dummyPointers.push_back(std::move(newDecl));
	}

	assert(index < dummyPointers.size());
	return encoder.EncodeNextVariable(*dummyPointers.at(index));
}

Node DerefPostComputer::MakeNewNode() {
	auto index = footprintSize++;
	return Node(index, GetNode(index));
}


Node DerefPostComputer::MakeFootprintRoot() {
	log() << "    adding root" << std::endl;
	// primary root is 'lhs.expr'; flow is unchanged ==> see DerefPostComputer::CheckAssumptionIntegrity
	auto root = MakeNewNode();
	assert(root.id == 0);
	solver.add(root == lhsNodeNext);
	solver.add(z3::forall(qvKey, encoder.EncodeTransitionMaintainsFlow(root, qvKey)));
	// for (const auto& value : flowValues) {
	// 	solver.add(encoder.EncodeTransitionMaintainsFlow(root, encoder.Encode(*value, StepTag::NOW)));
	// 	solver.add(encoder.EncodeTransitionMaintainsFlow(root, encoder.Encode(*value, StepTag::NEXT)));
	// }
	AddPointerAliasesToSolver(root);
	AddToFootprint(root);

	if (!ImpliesIsNonNull(root)) {
		Throw("Cannot compute footprint for heap update", "potential NULL dereference.");
	}

	return root;
}

bool DerefPostComputer::ExtendFootprint(Node node, std::string selector, StepTag tag) {
	log() << "    extending footprint: " << node << " -> " << selector << std::endl;
	for (auto [otherNode, otherSelector, successor] : footprintIntraface) {
		if (otherNode == node && otherSelector == selector) {
			assert(false);
			return true;
		}
	}

	// make successor
	auto successor = MakeNewNode();
	auto encodedSelector = std::make_pair(&nodeType, selector);
	solver.add(encoder.EncodeHeap(node, encodedSelector, successor, tag));
	AddPointerAliasesToSolver(successor);

	auto added = AddToFootprint(successor);
	log() << "      " << (added ? "" : "not ") << "adding successor: " << successor << std::endl;
	if (added && tag == StepTag::NOW) AddFlowRulesToSolver(node, selector, successor, tag);
	else if (!added) AddEdgeToBoundary(node, selector);

	return added;
}

bool DerefPostComputer::ExtendFootprint(Node node, std::string selector) {
	return ExtendFootprint(node, selector, StepTag::NEXT);
}

void DerefPostComputer::InitializeFootprint() {
	auto root = MakeFootprintRoot();

	if (lhs.sort() == Sort::PTR) {
		// TODO: skip if no flow??
		ExtendFootprint(root, lhs.fieldname, StepTag::NOW);
		ExtendFootprint(root, lhs.fieldname, StepTag::NEXT);
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void DerefPostComputer::CheckInvariant(Node node) {
	if (!performInvariantCheck) return;

	// invariant must handle NULL case itself
	auto check = encoder.EncodeNextInvariant(*info.solver.config.invariant, node);
	if (!Implies(check)) {
		log() << std::endl << std::endl << "Invariant violated in footprint: " << node << std::endl;
		for (const auto& conjunct : info.solver.config.invariant->blueprint->conjuncts) {
			auto encoded = encoder.MakeImplication(
				node.expr == encoder.EncodeNextVariable(*info.solver.config.invariant->vars.at(0)),
				encoder.EncodeNext(*conjunct)
			);
			log() << "  - inv " << *conjunct << ": " << Implies(encoded) << std::endl;
		}
		log() << std::endl << "PRE: " << info.preNow << std::endl;
		Throw("Could not establish invariant for heap update", "invariant violated within footprint");
	}
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
	if (Implies(pureCheck)) return;
	if (purityStatus != PurityStatus::PURE)
		Throw("Bad impure heap update", "not the first impure heap update");

	// check impure, part 1: nothing but 'impureKey' is changed
	solver.add(!IsUnchanged(impureKey));
	auto othersUnchangedCheck = z3::forall(qvKey, encoder.MakeImplication(
		qvKey != impureKey, /* ==> */ IsUnchanged(qvKey)
	));
	if (!Implies(othersUnchangedCheck))
		Throw("Specification violated by impure heap update", "multiple keys inserted/deleted at once");

	// begin debug output
	for (auto var : info.solver.GetVariablesInScope()) {
		if (checker.Implies(encoder.EncodeVariable(*var) == impureKey)) {
			log() << "      impure key == " << var->name << std::endl;
		}
	}
	// end debug output

	// check impure, part 2: check insertion/deletion for 'impureKey'
	assert(!(Implies(IsInserted(impureKey)) && Implies(IsDeleted(impureKey)))); // TODO: assertion with side effect
	if (Implies(IsInserted(impureKey))) purityStatus = PurityStatus::INSERTION;
	else if (Implies(IsDeleted(impureKey))) purityStatus = PurityStatus::DELETION;
	else Throw("Failed to find specification for impure heap update", "unable to figure out if it is an insertion or a deletion");
}

void DerefPostComputer::CheckFootprint(Node node) {
	log() << "    checking footprint: " << node << std::endl;
	log() << "      checking invariant..." << std::endl;
	CheckInvariant(node);
	log() << "      checking specification..." << std::endl;
	CheckSpecification(node);
	log() << "      purity: " << (purityStatus == PurityStatus::PURE ? "pure" : (purityStatus == PurityStatus::INSERTION ? "insert" : "delete")) << std::endl;
}

bool DerefPostComputer::IsOutflowUnchanged(Node node, std::string fieldname) {
	log() << "    checking outflow: " << node << " -> " << fieldname << std::endl;
	const auto& outflow = info.solver.config.flowDomain->GetOutFlowContains(fieldname);
	auto root = GetNode(0);
	assert(ImpliesIsNonNull(node)); // TODO: assert with side effect

	auto isUnchanged = z3::forall(qvKey, encoder.MakeImplication(
		encoder.EncodeFlow(root, qvKey), // TODO: now or next? should not matter
		/* ==> */
		encoder.EncodePredicate(outflow, node, qvKey) == encoder.EncodeNextPredicate(outflow, node, qvKey)
	));

	auto result = Implies(isUnchanged);
	log() << "        ==> unchanged = " << result << std::endl;
	return result;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////


void DerefPostComputer::ExploreAndCheckFootprint() {
	// TODO important: we are ignoring NULL during the exploration, can we ignore its flow / flow changes?? Are we ignoring NULL??

	InitializeFootprint();
	solver.push();
	// flow: only flow that comes from the root may change // TODO important: correct???
	// throw std::logic_error("try without first"); // <<<<<<<<<<<<<<<=====================================================================|||||||
	
	
	// auto qvOtherNode = encoder.EncodeVariable(GetDummyDecl<Sort::PTR, 1>());
	// for (auto tag : { StepTag::NOW, StepTag::NEXT }) {
	// 	for (auto selector : OutgoingEdges()) {
	// 		auto flowRule = z3::forall(qvNode, qvOtherNode, qvKey, encoder.MakeImplication(
	// 			qvNode != encoder.MakeNullPtr() &&
	// 			encoder.EncodePredicate(info.solver.config.flowDomain->GetOutFlowContains(selector), qvNode, qvKey, tag) &&
	// 			encoder.EncodeHeap(qvNode, std::make_pair(&nodeType, selector), qvOtherNode, tag),
	// 			/* ==> */
	// 			encoder.EncodeFlow(qvOtherNode, qvKey, true, tag)
	// 		));
	// 		solver.add(flowRule);
	// 	}
	// }

	solver.add(z3::forall(qvNode, qvKey, encoder.MakeImplication(
		encoder.EncodeFlow(GetNode(0), qvKey, false), // TODO: now or next? should not matter
		/* ==> */
		encoder.EncodeTransitionMaintainsFlow(qvNode, qvKey)
	)));
	if (lhs.sort() == Sort::PTR) {
		solver.add(z3::forall(qvNode, qvKey, encoder.MakeImplication( // TODO: only correct if the outflow of root did not change!!!!
			!encoder.EncodePredicate(info.solver.config.flowDomain->GetOutFlowContains(lhs.fieldname), GetNode(0), qvKey),
			/* ==> */
			encoder.EncodeTransitionMaintainsFlow(qvNode, qvKey)
		)));
	}
	// for (const auto& value : flowValues) {
	// 	for (auto tag : { StepTag::NOW, StepTag::NEXT }) {
	// 		auto encodedValue = encoder.Encode(*value, tag);
	// 		solver.add(z3::forall(qvNode, encoder.MakeImplication(
	// 			encoder.EncodeFlow(GetNode(0), encodedValue, false), // TODO: now or next? should not matter
	// 			/* ==> */
	// 			encoder.EncodeTransitionMaintainsFlow(qvNode, encodedValue)
	// 		)));
	// 	}
	// }
	log() << "  initialized footprint with " << footprint.size() << " nodes" << std::endl;

	// decltype(footprint) checkedFootprint;
	decltype(footprintBoundary) checkedBoundary;
	bool done;

	do {
		log() << "  iterating" << std::endl;

		// check interface
		done = true;
		for (auto pair : footprintBoundary) {
			auto [node, selector] = pair;
			if (checkedBoundary.count(pair)) continue;
			if (!IsOutflowUnchanged(node, selector)) {
				bool success = ExtendFootprint(node, selector);
				if (!success) Throw("Cannot verify heap update", "failed to extend footprint");
				done = false;
				break;
			}
			checkedBoundary.insert(pair);
		}

		log() << "  ~> footprint size = " << footprint.size() << std::endl;
		log() << "  ~> done = " << done << std::endl;
		if (footprint.size() > 4) // TODO important: configured by flow domain
			Throw("Cannot verify heap update", "footprint too small");

	} while (!done);
		
	// check invariant / specification
	AddNonFootprintFlowRulesToSolver();
	for (auto node : footprint) {
		// if (checkedFootprint.count(node)) continue;
		CheckFootprint(node);
		// checkedFootprint.insert(node);
	}

	solver.pop();
	solver.add(encoder.EncodeNext(info.solver.GetInstantiatedInvariant()));
	log() << "  update looks good!" << std::endl;
}


std::unique_ptr<ConjunctionFormula> DerefPostComputer::MakePostNow() {
	AddBaseRulesToSolver();
	ExploreAndCheckFootprint();
	AddNonFootprintFlowRulesToSolver();
	AddSpecificationRulesToSolver();

	// TODO important: obey info.implicationCheck and if set skip checks
	// TODO important: keyset disjointness check within footprint
	std::cerr << "WARNING: missing disjointness check!!" << std::endl;

	auto result = info.ComputeImpliedCandidates(checker);

	// debug output
	log() << "**PRE**     " << info.preNow << std::endl;
	log() << "**POST**    " << *result << std::endl;
	for (const auto& candidate : info.solver.GetCandidates()) {
		bool inPre = plankton::syntactically_contains_conjunct(info.preNow, candidate.GetCheck());
		bool inPost = plankton::syntactically_contains_conjunct(*result, candidate.GetCheck());
		if (inPre == inPost) continue;
		log() << " diff " << (inPre ? "lost" : "got") << " " << candidate.GetCheck() << std::endl;
	}

	return result;
}
