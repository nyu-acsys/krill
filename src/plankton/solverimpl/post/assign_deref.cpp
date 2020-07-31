#include "plankton/solverimpl/linsolver.hpp"

#include "plankton/logger.hpp" // TODO: delete
#include "plankton/solverimpl/post/info.hpp"

using namespace cola;
using namespace heal;
using namespace plankton;


enum struct PurityStatus { PURE, INSERTION, DELETION };


struct Node {
	std::size_t id;
	Symbol Symbol;

	Node(std::size_t id, Symbol Symbol) : id(id), Symbol(Symbol) {}
	operator Symbol() const { return Symbol; }
	operator Term() const { return Symbol; }

	inline bool IMP(bool a, bool b) { return !a || b; }
	bool operator<(const Node& other) const {
		assert(IMP(id == other.id, expr == other.expr));
		return id < other.id;
	}
	bool operator==(const Node& other) const {
		assert(IMP(id == other.id, expr == other.expr));
		return id == other.id;
	}
};

template<>
Logger& Logger::operator<<(const Node& node) {
	std::cout << "[" << node.id << ": " << node.expr << "]";
	return *this;
}

using InnerEdge = std::tuple<Node, Selector, Node>; // <node, selector, successor> encodes: {node}->{selector}=={successor} inside footprint
using OuterEdge = std::tuple<Node, Selector>; // <node, selector> encodes: {node}->{selector} leads outside footprint


struct DerefPostComputer {
	PostInfo info;
	Encoder& encoder;
	const Dereference& lhs;
	const Expression& rhs;

	const Type& nodeType;
	std::size_t footprintSize = 0;

	std::shared_ptr<ImplicationChecker> checkerPtr;
	ImplicationChecker& checker;

	Symbol lhsNodeNow, lhsNodeNext;
	Symbol lhsNow, lhsNext;
	Symbol rhsNow, rhsNext;
	Symbol qvNode, qvKey;
	Symbol impureKey;

	std::set<Node> footprint;
	std::set<InnerEdge> footprintIntraface;
	std::set<OuterEdge> footprintBoundary;
	std::vector<Selector> pointerSelectors;
	PurityStatus purityStatus = PurityStatus::PURE;

	DerefPostComputer(PostInfo info, const Dereference& lhs, const Expression& rhs);
	DerefPostComputer(PostInfo info, const Dereference& lhs, const Expression& rhs, std::array<Symbol, 3> qvVars);
	std::unique_ptr<Annotation> MakePost();

	void Throw(std::string reason, std::string explanation="");
	bool IsHeapHomogeneous();
	std::size_t GetMaxFootprintSize();
	std::vector<std::string> OutgoingEdges();
	
	// bool Implies(z3::expr expr);
	// bool ImpliesIsNull(Node node);
	// bool ImpliesIsNull(Node node, std::string selector, EncodingTag tag);
	// bool ImpliesIsNonNull(Node node);
	Term MakeFootprintContainsCheck(Symbol node);
	void AddBaseRulesToSolver();
	void AddNonFootprintFlowRulesToSolver();
	void AddSpecificationRulesToSolver();
	void AddPointerAliasesToSolver(Node node);
	void AddFootprintFlowRulesToSolver();
	// void AddFlowRulesToSolver(Node node, std::string selector, Node successor, EncodingTag tag);

	void CheckAssumptionIntegrity(Node node);
	void AddEdgeToBoundary(Node node, std::string selector);
	void AddEdgeToIntraface(Node node, std::string selector, Node successor);
	bool AddToFootprint(Node node);
	Symbol GetNode(std::size_t index);
	Node MakeNewNode();
	Node MakeFootprintRoot();
	bool ExtendFootprint(Node node, std::string selector, EncodingTag tag);
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

bool IsHeapHomogeneous(const std::vector<Selector>& pointerSelectors) {
	for (auto selector : pointerSelectors) {
		if (selector.type.field(selector.fieldname) != &selector.type) {
			return false;
		}
	}
	return true;
}

std::vector<Selector> GetPointerSelectors(const Type& type) {
	// TODO: reuse code from encoder?
	std::vector<Selector> result;
	result.reserve(type.fields.size());
	for (auto [fieldName, fieldType] : type.fields) {
		if (fieldType.get().sort != Sort::PTR) continue;
		result.emplace_back(type, fieldName);
	}
	return result;
}

DerefPostComputer(PostInfo info, const Dereference& lhs, const Expression& rhs, std::array<Symbol, 3> qvVars)
	: info(std::move(info_)),
	  encoderPtr(info.solver.GetEncoder()),
	  encoder(*encoderPtr),
	  lhs(lhs),
	  rhs(rhs),
	  nodeType(lhs.expr->type()),
	  checker(info.solver.MakeImplicationChecker(EncodingTag::NEXT)),
	  lhsNodeNow(encoder.EncodeNow(*lhs.expr)), lhsNodeNext(encoder.EncodeNext(*lhs.expr)),
	  lhsNow(encoder.EncodeNow(lhs)), lhsNext(encoder.EncodeNext(lhs)),
	  rhsNow(encoder.EncodeNow(rhs)), rhsNext(encoder.EncodeNext(rhs)),
	  qvNode(std::move(qvVars[0])),
	  qvKey(std::move(qvVars[1])), // TODO: flow domain should tell us the key type
	  impureKey(std::move(qvVars[2])),
	  pointerSelectors(GetPointerSelectors(nodeType))
{
	// check flow domain compatibility
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

	// check heap structure compatibility
	if (!IsHeapHomogeneous(pointerSelectors)) {
		throw SolvingError("Cannot handle non-homogeneous heaps. Sorry.");
		// TODO: explain 'non-homogeneous'
	}
}

const Type& GetDataType() { // TODO: we should get this from the flow domain
	static Type dummyType("DerefPostComputer#dummy-data-type", Sort::DATA);
	return dummyType;
}

DerefPostComputer::DerefPostComputer(PostInfo info, const Dereference& lhs, const Expression& rhs)
	: DerefPostComputer(std::move(info), lhs, rhs, plankton::MakeQuantifiedVariables(lhs.expr->type(), GetDataType(), GetDataType()))
{}


void DerefPostComputer::Throw(std::string reason, std::string explanation) {
	std::stringstream msg;
	msg << reason + " '" + AssignmentString(lhs, rhs) + "'";
	if (explanation != "") msg << ": " << explanation;
	msg << ".";
	throw SolvingError(msg.str());
}

std::size_t DerefPostComputer::GetMaxFootprintSize() {
	throw std::logic_error("not yet implemented: DerefPostComputer::GetMaxFootprintSize()");
	// return info.solver.config.flowDomain->GetFootprintDepth(info.preNow, lhs, rhs);
}

Term DerefPostComputer::MakeFootprintContainsCheck(Symbol node) {
	std::vector<Term> vec;
	for (Term elem : footprint) {
		vec.push_back(elem.Equals(node));
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
		checker.AddPremise(encoder.MakeForall(qvNode, encoder.MakeImplies(
			qvNode.Distinct(rhsNow), /* ==> */ encoder.EncodeTransitionMaintainsOwnership(qvNode)
		)));
		checker.AddPremise(encoder.EncodeNextOwnership(rhsNext).Negate());
	} else {
		checker.AddPremise(encoder.MakeForall(qvNode, encoder.EncodeTransitionMaintainsOwnership(qvNode)));
	}

	// heap
	checker.AddPremise(encoder.MakeForall(qvNode, encoder.MakeImplication(
		qvNode.Distinct(lhsNodeNow), /* ==> */ encoder.EncodeTransitionMaintainsHeap(qvNode, nodeType)
	)));
	checker.AddPremise(encoder.EncodeTransitionMaintainsHeap(lhsNodeNow, nodeType, { lhs.fieldname }));
	checker.AddPremise(lhsNext.Equals(rhsNow));

	// stack
	for (auto var : info.solver.GetVariablesInScope()) {
		checker.AddPremise(encoder.EncodeNow(*var).Equals(encoder.EncodeNext(*var)));
	}

	// rules
	// TODO important: do not merge invariants and rules; here we want to use the rules in NOW and NEXT; and we do not want to show the rules!!!
	throw std::logic_error("not yet implemented: DerefPostComputer::AddBaseRulesToSolver()");
	checker.AddPremise(encoder.EncodeNow(info.solver.GetInstantiatedInvariant()));
	// TODO: checker.AddPremise(encoder.EncodeNow(info.solver.GetInstantiatedRules()));
	// TODO: checker.AddPremise(encoder.EncodeNext(info.solver.GetInstantiatedRules()));

	// add pre
	checker.AddPremise(encoder.EncodeNow(info.preNow));
}

void AddFootprintFlowRulesToSolver() {
	// TODO: add flow rules; (1) how flow propagates, (2) only flow coming from root may change
	throw std::logic_error("not yet implemented: DerefPostComputer::AddBaseRulesToSolver");
}

void DerefPostComputer::AddNonFootprintFlowRulesToSolver() {
	// the flow outside the footprint remains unchanged
	checker.AddPremise(encoder.MakeForall(qvNode, qvKey, encoder.MakeImplication(
		!MakeFootprintContainsCheck(qvNode), /* ==> */ encoder.EncodeTransitionMaintainsFlow(qvNode, qvKey)
	)));
}

void DerefPostComputer::AddSpecificationRulesToSolver() {
	if (!info.performSpecificationCheck) return;

	switch (purityStatus) {
		case PurityStatus::PURE:
			for (auto kind : SpecificationAxiom::KindIteratable) {
				checker.AddPremise(encoder.MakeForall(qvKey, encoder.MakeImplication(
					encoder.EncodeNowObligation(kind, qvKey), /* ==> */ encoder.EncodeNextObligation(kind, qvKey)
				)));
				for (bool value : { true, false }) {
					checker.AddPremise(encoder.MakeForall(qvKey, encoder.MakeImplication(
						encoder.EncodeNowFulfillment(kind, qvKey, value), /* ==> */ encoder.EncodeNextFulfillment(kind, qvKey, value)
					)));
				}
			}
			break;

		case PurityStatus::INSERTION:
			checker.AddPremise(encoder.MakeImplication(
				encoder.EncodeNowObligation(SpecificationAxiom::Kind::INSERT, impureKey),
				/* ==> */
				encoder.EncodeNextFulfillment(SpecificationAxiom::Kind::INSERT, impureKey, true)
			));
			break;

		case PurityStatus::DELETION:
			checker.AddPremise(encoder.MakeImplication(
				encoder.EncodeNowObligation(SpecificationAxiom::Kind::DELETE, impureKey),
				/* ==> */
				encoder.EncodeNextFulfillment(SpecificationAxiom::Kind::DELETE, impureKey, true)
			));
			break;
	}
}

void DerefPostComputer::AddPointerAliasesToSolver(Node node) {
	// TODO: remove this?

	for (auto variable : info.solver.GetVariablesInScope()) {
		auto alias = encoder.EncodeNow(*variable);
		if (checker.Implies(alias.Equal(node))) {
			// TODO: also add NEXT version of alias?
			log() << "    alias: " << node << " = " << alias << std::endl;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void DerefPostComputer::CheckAssumptionIntegrity(Node node) {
	// for decreasing flows: nothing to do
	if (info.solver.config.flowDomain->IsFlowDecreasing()) return;

	// for non-decreasing flows: ensure loop-freedom of the footprint ==> ensure that 'node' is not yet covered
	// TODO: relax requirement for non-decreasing flows ==> it is sufficient that the flow on loops is not increased
	if (checker.Implies(MakeFootprintContainsCheck(node).Negate())) return;
	
	// fail if assumptions violated
	throw SolvingError("Cannot handle non-decreasing flows in the presence of (potential) loops within the flow footprint.");
}


void DerefPostComputer::AddEdgeToBoundary(Node node, std::string selector) {
	// TODO important: do not add to boundary if already in interface?
	throw std::logic_error("not yet implemented: DerefPostComputer::AddEdgeToBoundary");

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
	AddFootprintFlowRulesToSolver();
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
