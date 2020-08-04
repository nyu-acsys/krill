#include "plankton/solverimpl/linsolver.hpp"

#include "plankton/logger.hpp" // TODO: delete
#include "plankton/error.hpp"
#include "plankton/util.hpp"
#include "plankton/solverimpl/post/info.hpp"

using namespace cola;
using namespace heal;
using namespace plankton;


enum struct PurityStatus { PURE, INSERTION, DELETION };


struct Node {
	std::size_t id;
	Symbol symbol;

	Node(std::size_t id, Symbol symbol) : id(id), symbol(symbol) {}
	operator Symbol() const { return symbol; }
	operator Term() const { return symbol; }

	inline bool IMP(bool a, bool b) const { return !a || b; }
	bool operator<(const Node& other) const {
		assert(IMP(id == other.id, symbol == other.symbol));
		return id < other.id;
	}
	bool operator==(const Node& other) const {
		assert(IMP(id == other.id, symbol == other.symbol));
		return id == other.id;
	}
};

template<>
Logger& Logger::operator<<(const Node& node) {
	std::cout << "[" << node.id << ": " << node.symbol << "]";
	return *this;
}

using InnerEdge = std::tuple<Node, Selector, Node>; // <node, selector, successor> encodes: {node}->{selector}=={successor} inside footprint
using OuterEdge = std::tuple<Node, Selector>; // <node, selector> encodes: {node}->{selector} leads outside footprint


struct DerefPostComputer {
	PostInfo info;
	std::shared_ptr<Encoder> encoderPtr;
	Encoder& encoder;
	const Dereference& lhs;
	const Expression& rhs;

	const Type& nodeType;
	std::size_t footprintSize = 0;

	std::shared_ptr<ImplicationChecker> checkerPtr;
	ImplicationChecker& checker;

	Term lhsNodeNow, lhsNodeNext;
	Term lhsNow, lhsNext;
	Term rhsNow, rhsNext;
	Symbol qvNode, qvOtherNode, qvKey;
	Symbol impureKey;
	Symbol interfaceKey;

	std::set<Node> footprint;
	std::set<InnerEdge> footprintIntraface;
	std::set<OuterEdge> footprintBoundary;
	std::vector<Selector> pointerSelectors;
	PurityStatus purityStatus = PurityStatus::PURE;

	DerefPostComputer(PostInfo info, const Dereference& lhs, const Expression& rhs);
	DerefPostComputer(PostInfo info, const Dereference& lhs, const Expression& rhs, std::array<Symbol, 5> qvVars);
	std::unique_ptr<Annotation> MakePost();

	void Throw(std::string reason, std::string explanation="");
	std::size_t GetMaxFootprintSize();
	Term MakeFootprintContainsCheck(Symbol node);
	// Term MakeIsIntrafaceKeyCheck(Symbol key);
	Term MakeBoundaryKeyRestriction(Symbol key, Node node, Selector selector);
	Term MakeBoundaryKeyRestriction(Symbol key);

	void AddBaseRulesToSolver();
	void AddNonFootprintFlowRulesToSolver();
	void AddSpecificationRulesToSolver();
	void AddPointerAliasesToSolver(Node node);
	void AddFootprintFlowRulesToSolver();
	// void AddFlowRulesToSolver(Node node, std::string selector, Node successor, EncodingTag tag);

	void CheckAssumptionIntegrity(Node node);
	void AddEdgeToBoundary(Node node, Selector selector);
	void AddEdgeToIntraface(Node node, Selector selector, Node successor);
	bool AddToFootprint(Node node);
	Symbol GetNode(std::size_t index);
	Node MakeNewNode();
	Node MakeFootprintRoot();
	bool ExtendFootprint(Node node, Selector selector, EncodingTag tag);
	bool ExtendFootprint(Node node, Selector selector);
	void InitializeFootprint();

	void CheckInvariant(Node node);
	void CheckSpecification(Node node);
	void CheckFootprint(Node node);
	bool IsOutflowUnchanged(Node node, Selector fieldname);

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
	log() << "**PRE**  " << info.preNow << std::endl << std::endl;

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

inline bool IsHeapHomogeneous(const std::vector<Selector>& pointerSelectors) {
	for (auto selector : pointerSelectors) {
		if (selector.type.field(selector.fieldname) != &selector.type) {
			return false;
		}
	}
	return true;
}

inline std::vector<Selector> GetPointerSelectors(const Type& type) {
	// TODO: reuse code from encoder?
	std::vector<Selector> result;
	result.reserve(type.fields.size());
	for (auto [fieldName, fieldType] : type.fields) {
		if (fieldType.get().sort != Sort::PTR) continue;
		result.emplace_back(type, fieldName);
	}
	return result;
}

DerefPostComputer::DerefPostComputer(PostInfo info_, const Dereference& lhs, const Expression& rhs, std::array<Symbol, 5> qvVars)
	: info(std::move(info_)),
	  encoderPtr(info.solver.GetEncoder()),
	  encoder(*encoderPtr),
	  lhs(lhs),
	  rhs(rhs),
	  nodeType(lhs.expr->type()),
	  checkerPtr(info.solver.MakeImplicationChecker(EncodingTag::NEXT)),
	  checker(*checkerPtr),
	  lhsNodeNow(encoder.EncodeNow(*lhs.expr)), lhsNodeNext(encoder.EncodeNext(*lhs.expr)),
	  lhsNow(encoder.EncodeNow(lhs)), lhsNext(encoder.EncodeNext(lhs)),
	  rhsNow(encoder.EncodeNow(rhs)), rhsNext(encoder.EncodeNext(rhs)),
	  qvNode(std::move(qvVars[0])),
	  qvOtherNode(std::move(qvVars[1])),
	  qvKey(std::move(qvVars[2])), // TODO: flow domain should tell us the key type
	  impureKey(std::move(qvVars[3])),
	  interfaceKey(std::move(qvVars[4])),
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

inline const Type& GetDataType() { // TODO: we should get this from the flow domain
	static Type dummyType("DerefPostComputer#dummy-data-type", Sort::DATA);
	return dummyType;
}

inline std::array<Symbol, 5> MakeQVars(std::shared_ptr<Encoder> encoder, const Type& nodeType) {
	return plankton::MakeQuantifiedVariables(encoder, nodeType, nodeType, GetDataType(), GetDataType(), GetDataType());
}

DerefPostComputer::DerefPostComputer(PostInfo info, const Dereference& lhs, const Expression& rhs)
	: DerefPostComputer(std::move(info), lhs, rhs, MakeQVars(info.solver.GetEncoder(), lhs.expr->type()))
{}


void DerefPostComputer::Throw(std::string reason, std::string explanation) {
	std::stringstream msg;
	msg << reason + " '" + AssignmentString(lhs, rhs) + "'";
	if (explanation != "") msg << ": " << explanation;
	msg << ".";
	throw SolvingError(msg.str());
}

std::size_t DerefPostComputer::GetMaxFootprintSize() {
	std::cerr << "WARNING: using dummy footprint size!!" << std::endl;
	return 3;
	// return info.solver.config.flowDomain->GetFootprintDepth(info.preNow, lhs, rhs);
}

Term DerefPostComputer::MakeFootprintContainsCheck(Symbol node) {
	std::vector<Term> vec;
	for (Term elem : footprint) {
		vec.push_back(elem.Equal(node));
	}
	return encoder.MakeOr(vec);
}

// Term DerefPostComputer::MakeIsIntrafaceKeyCheck(Symbol key) {
// 	std::vector<Term> vec;
// 	for (auto [node, selector, successor] : footprintIntraface) {
// 		auto& outflow = info.solver.config.flowDomain->GetOutFlowContains(selector.fieldname);
// 		vec.push_back(encoder.EncodeNowPredicate(outflow, node, key)); // TODO important: encode for NOW or NEXT??
// 	}
// 	return encoder.MakeOr(vec);
// }

// Term DerefPostComputer::MakeBoundaryKeyRestriction(Node node, Selector selector, Symbol key) {
// 	auto isInterfaceKey = MakeIsIntrafaceKeyCheck(key);
// 	auto& outflow = info.solver.config.flowDomain->GetOutFlowContains(selector.fieldname);
// 	auto isInOutflow = encoder.EncodeNowPredicate(outflow, node, key); // TODO important: encode for NOW or NEXT??
// 	return encoder.MakeOr(isInterfaceKey, isInOutflow);
// }

inline void HandleEdge (Node node, Selector selector, Symbol key, Encoder& encoder, const PostInfo& info, std::vector<Term>& dst) {
	auto& outflow = info.solver.config.flowDomain->GetOutFlowContains(selector.fieldname);
	// TODO important: encode for NOW or NEXT??
	dst.push_back(encoder.EncodeNowPredicate(outflow, node, key));
	dst.push_back(encoder.EncodeNextPredicate(outflow, node, key));
};

Term DerefPostComputer::MakeBoundaryKeyRestriction(Symbol key, Node node, Selector selector) {
	std::vector<Term> vec;
	for (auto [node, selector, successor] : footprintIntraface) {
		HandleEdge(node, selector, key, encoder, info, vec);
	}
	HandleEdge(node, selector, key, encoder, info, vec);
	return encoder.MakeOr(vec);	
}

Term DerefPostComputer::MakeBoundaryKeyRestriction(Symbol key) {
	std::vector<Term> vec;
	for (auto [node, selector, successor] : footprintIntraface) {
		HandleEdge(node, selector, key, encoder, info, vec);
	}
	for (auto [node, selector] : footprintBoundary) {
		HandleEdge(node, selector, key, encoder, info, vec);
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
			rhsNow.Distinct(qvNode), /* ==> */ encoder.EncodeTransitionMaintainsOwnership(qvNode)
		)));
		checker.AddPremise(encoder.EncodeNextIsOwned(rhsNext).Negate());
	} else {
		checker.AddPremise(encoder.MakeForall(qvNode, encoder.EncodeTransitionMaintainsOwnership(qvNode)));
	}

	// heap
	checker.AddPremise(encoder.MakeForall(qvNode, encoder.MakeImplies(
		lhsNodeNow.Distinct(qvNode), /* ==> */ encoder.EncodeTransitionMaintainsHeap(qvNode, nodeType)
	)));
	checker.AddPremise(encoder.EncodeTransitionMaintainsHeap(lhsNodeNow, nodeType, { lhs.fieldname }));
	checker.AddPremise(lhsNext.Equal(rhsNow));

	// stack
	for (auto var : info.solver.GetVariablesInScope()) {
		checker.AddPremise(encoder.EncodeNow(*var).Equal(encoder.EncodeNext(*var)));
	}

	// flow
	for (auto tag : { EncodingTag::NOW, EncodingTag::NEXT }) {
		for (auto selector : pointerSelectors) {
			checker.AddPremise(encoder.MakeForall(qvNode, qvOtherNode, encoder.MakeImplies(
				encoder.MakeAnd({
					encoder.MakeDistinct(qvNode, encoder.MakeNullPtr()),
					encoder.EncodeHeapIs(qvNode, selector, qvOtherNode, tag),
					encoder.EncodePredicate(info.solver.config.flowDomain->GetOutFlowContains(selector.fieldname), qvNode, interfaceKey, tag)
				}),
				/* ==> */
				encoder.EncodeFlow(qvOtherNode, interfaceKey, tag)
			)));
		}
	}

	// rules
	checker.AddPremise(encoder.EncodeNow(info.solver.GetInstantiatedInvariant()));
	checker.AddPremise(encoder.EncodeNow(info.solver.GetInstantiatedRules()));
	checker.AddPremise(encoder.EncodeNext(info.solver.GetInstantiatedRules()));

	// add pre
	checker.AddPremise(encoder.EncodeNow(info.preNow));
}

void DerefPostComputer::AddFootprintFlowRulesToSolver() {
	// NOTE: these rules are invalidated by footprint changes / extensions
	// TODO important: check soundness

	auto MakeIntraFaceFlowRuleForNode = [&,this](Node node, EncodingTag tag) {
		std::vector<Term> vec;
		for (auto [pred, sel, succ] : footprintIntraface) {
			if (!(succ == node)) continue;
			auto& outflow = info.solver.config.flowDomain->GetOutFlowContains(sel.fieldname);
			vec.push_back(encoder.EncodePredicate(outflow, pred, interfaceKey, tag));
		}
		auto notFromWithin = encoder.MakeOr(vec).Negate();
		auto hasFlow = encoder.EncodeFlow(node, interfaceKey, tag);
		return encoder.MakeImplies(
			encoder.MakeAnd(hasFlow, notFromWithin),
			/* ==> */
			encoder.EncodeTransitionMaintainsFlow(node, interfaceKey)
		);
	};

	for (auto tag : { EncodingTag::NOW, EncodingTag::NEXT }) {
		for (auto node : footprint) {
			checker.AddPremise(MakeIntraFaceFlowRuleForNode(node, tag));
		}
	}
}

void DerefPostComputer::AddNonFootprintFlowRulesToSolver() {
	// the flow outside the footprint remains unchanged
	checker.AddPremise(encoder.MakeForall(qvNode, qvKey, encoder.MakeImplies(
		MakeFootprintContainsCheck(qvNode).Negate(), /* ==> */ encoder.EncodeTransitionMaintainsFlow(qvNode, qvKey)
	)));
}

void DerefPostComputer::AddSpecificationRulesToSolver() {
	if (!info.performSpecificationCheck) return;

	switch (purityStatus) {
		case PurityStatus::PURE:
			for (auto kind : SpecificationAxiom::KindIteratable) {
				checker.AddPremise(encoder.MakeForall(qvKey, encoder.MakeImplies(
					encoder.EncodeNowObligation(kind, qvKey), /* ==> */ encoder.EncodeNextObligation(kind, qvKey)
				)));
				for (bool value : { true, false }) {
					checker.AddPremise(encoder.MakeForall(qvKey, encoder.MakeImplies(
						encoder.EncodeNowFulfillment(kind, qvKey, value), /* ==> */ encoder.EncodeNextFulfillment(kind, qvKey, value)
					)));
				}
			}
			break;

		case PurityStatus::INSERTION:
			checker.AddPremise(encoder.MakeImplies(
				encoder.EncodeNowObligation(SpecificationAxiom::Kind::INSERT, impureKey),
				/* ==> */
				encoder.EncodeNextFulfillment(SpecificationAxiom::Kind::INSERT, impureKey, true)
			));
			break;

		case PurityStatus::DELETION:
			checker.AddPremise(encoder.MakeImplies(
				encoder.EncodeNowObligation(SpecificationAxiom::Kind::DELETE, impureKey),
				/* ==> */
				encoder.EncodeNextFulfillment(SpecificationAxiom::Kind::DELETE, impureKey, true)
			));
			break;
	}
}

void DerefPostComputer::AddPointerAliasesToSolver(Node node) {
	// TODO: remove this?

	// get terms to solve
	std::vector<Term> aliases;
	for (auto variable : info.solver.GetVariablesInScope()) {
		auto encodedVariable = encoder.EncodeNow(*variable);
		aliases.push_back(encoder.MakeEqual(node, encodedVariable));
		aliases.push_back(encoder.MakeDistinct(node, encodedVariable));
	}

	// compute implied terms
	auto implied = checker.ComputeImplied(aliases);
	assert(aliases.size() == implied.size());

	// handle result
	for (std::size_t index = 0; index < implied.size(); ++index) {
		if (!implied[index]) continue;
		checker.AddPremise(aliases.at(index));
		log() << "    alias: " << aliases.at(index) << std::endl;
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


void DerefPostComputer::AddEdgeToBoundary(Node node, Selector selector) {
	// not adding to boundary if already in intraface
	for (auto [otherNode, otherSelector, successor] : footprintIntraface) {
		if (node == otherNode && selector == otherSelector) return;
	}

	footprintBoundary.emplace(node, selector);
}

void DerefPostComputer::AddEdgeToIntraface(Node node, Selector selector, Node successor) {
	footprintIntraface.emplace(node, selector, successor);
	
	// auto insertion = footprintIntraface.emplace(node, selector, successor);
	// if (insertion.second) AddFlowRulesToSolver(node, selector, successor, EncodingTag::NEXT);
}

bool DerefPostComputer::AddToFootprint(Node node) {
	// NULL cannot be added to the footprint
	if (!checker.ImpliesIsNonNull(node)) {
		return false;
	}

	// TODO: correct/needed?
	CheckAssumptionIntegrity(node);

	// check if we already have an alias for node
	for (auto existingNode : footprint) {
		Term equal = encoder.MakeEqual(existingNode, node);
		bool areEqual = checker.Implies(equal);
		bool areUnequal = !areEqual && checker.Implies(equal.Negate());
		if (!areEqual && !areUnequal)
			Throw("Cannot create footprint for heap update", "abstraction too imprecise");
		assert(!areEqual);
	}
	footprint.insert(node);

	// node satisfied the invariant before the update
	checker.AddPremise(encoder.EncodeNowInvariant(*info.solver.config.invariant, node));

	// add outgoing edges to boundary
	for (auto selector : pointerSelectors) {
		AddEdgeToBoundary(node, selector);
	}

	// find new intraface edges
	std::vector<OuterEdge> toBeDeleted;
	for (auto [other, selector] : footprintBoundary) {
		auto pointsToNode = encoder.EncodeNowHeapIs(other, selector, node);
		if (checker.Implies(pointsToNode)) {
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

Symbol DerefPostComputer::GetNode(std::size_t index) {
	static Type dummyType("dummy-post-deref-node-type", Sort::PTR);
	static std::vector<std::unique_ptr<VariableDeclaration>> dummyPointers;
	while (index >= dummyPointers.size()) {
		auto newDecl = std::make_unique<VariableDeclaration>("post-deref#footprint" + std::to_string(dummyPointers.size()), dummyType, false);
		dummyPointers.push_back(std::move(newDecl));
	}

	assert(index < dummyPointers.size());
	return encoder.EncodeNext(*dummyPointers.at(index));
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
	checker.AddPremise(encoder.MakeEqual(root, lhsNodeNext));
	checker.AddPremise(encoder.MakeForall(qvKey, encoder.EncodeTransitionMaintainsFlow(root, qvKey)));
	checker.AddPremise(encoder.EncodeTransitionMaintainsFlow(root, interfaceKey));
	AddPointerAliasesToSolver(root);
	AddToFootprint(root);

	if (!checker.ImpliesIsNonNull(root)) {
		Throw("Cannot compute footprint for heap update", "potential NULL dereference.");
	}

	return root;
}

bool DerefPostComputer::ExtendFootprint(Node node, Selector selector, EncodingTag tag) {
	log() << "    extending footprint: " << node << " -> " << selector << std::endl;
	for (auto [otherNode, otherSelector, successor] : footprintIntraface) {
		if (otherNode == node && otherSelector == selector) {
			// assert(false);
			return true;
		}
	}

	// make successor
	auto successor = MakeNewNode();
	checker.AddPremise(encoder.EncodeHeapIs(node, selector, successor, tag));
	AddPointerAliasesToSolver(successor);

	auto added = AddToFootprint(successor);
	if (!added) AddEdgeToBoundary(node, selector);
	log() << "      " << (added ? "" : "not ") << "adding successor: " << successor << std::endl;
	// if (added && tag == EncodingTag::NOW) AddFlowRulesToSolver(node, selector, successor, tag);
	// else if (!added) AddEdgeToBoundary(node, selector);

	return added;
}

bool DerefPostComputer::ExtendFootprint(Node node, Selector selector) {
	return ExtendFootprint(node, selector, EncodingTag::NEXT);
}

void DerefPostComputer::InitializeFootprint() {
	auto root = MakeFootprintRoot();

	if (lhs.sort() == Sort::PTR) {
		// TODO: skip if no flow??
		// if (!checker.Implies(encoder.EncodeNowHasFlow(root).Negate())) {
			ExtendFootprint(root, Selector(nodeType, lhs.fieldname), EncodingTag::NOW);
			ExtendFootprint(root, Selector(nodeType, lhs.fieldname), EncodingTag::NEXT);
		// }
	}
}

/*
	Das Problem
	===========

	Es kann passieren, dass man in PRE weiß, dass man Flow an einem Knoten gehabt hat, aber
	man weiß nicht *woher* dieser kommt. Falls er von außerhalb des Footprints kommt, so sollte
	er auf Grund von Distributivität von Edge-Functions ignoriert werden können.
	Nur falls der Flow von innerhalb des Footprints stammt, muss man gucken dieser Flow im
	PRE und POST Schritt gleich ist.
*/


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void DerefPostComputer::CheckInvariant(Node node) {
	if (!info.performInvariantCheck) return;

	// invariant must handle NULL case itself
	auto check = encoder.EncodeNextInvariant(*info.solver.config.invariant, node);
	if (!checker.Implies(check)) {
		log() << std::endl << std::endl << "Invariant violated in footprint: " << node << std::endl;
		for (const auto& conjunct : info.solver.config.invariant->blueprint->conjuncts) {
			auto encoded = encoder.MakeImplies(
				encoder.MakeEqual(node, encoder.EncodeNext(*info.solver.config.invariant->vars.at(0))),
				encoder.EncodeNext(*conjunct)
			);
			log() << "  - inv " << *conjunct << ": " << checker.Implies(encoded) << std::endl;
		}
		log() << std::endl << "PRE: " << info.preNow << std::endl;
		Throw("Could not establish invariant for heap update", "invariant violated within footprint");
	}
}

void DerefPostComputer::CheckSpecification(Node node) {
	if (!info.performSpecificationCheck) return;

	// helpers
	auto NodeContains = [&,this](Symbol key, EncodingTag tag) {
		return encoder.MakeAnd(
			encoder.EncodePredicate(*info.solver.config.logicallyContainsKey, node, key, tag),
			encoder.EncodeKeysetContains(node, key, tag)
		);
	};
	auto IsInserted = [&](Symbol key) { return encoder.MakeAnd( NodeContains(key, EncodingTag::NOW).Negate(), NodeContains(key, EncodingTag::NEXT)); };
	auto IsDeleted = [&](Symbol key) { return encoder.MakeAnd(NodeContains(key, EncodingTag::NOW), NodeContains(key, EncodingTag::NEXT).Negate()); };
	auto IsUnchanged = [&](Symbol key) { return encoder.MakeIff(NodeContains(key, EncodingTag::NOW), NodeContains(key, EncodingTag::NEXT)); };

	// check purity
	// auto pureCheck = encoder.MakeForall(qvKey, IsUnchanged(qvKey));
	// auto pureCheck = IsUnchanged(interfaceKey);
	// auto pureCheck = encoder.MakeImplies(encoder.EncodeNowFlow(GetNode(0), interfaceKey), IsUnchanged(interfaceKey)); // <<<<=========== premis does it all!!
	auto pureCheck = encoder.MakeImplies(MakeBoundaryKeyRestriction(interfaceKey), IsUnchanged(interfaceKey));
	if (checker.Implies(pureCheck)) return;
	if (purityStatus != PurityStatus::PURE)
		Throw("Bad impure heap update", "not the first impure heap update");

	// check impure, part 1: nothing but 'impureKey' is changed
	checker.AddPremise(IsUnchanged(impureKey).Negate());
	auto othersUnchangedCheck = encoder.MakeForall(qvKey, encoder.MakeImplies(
		encoder.MakeDistinct(qvKey, impureKey), /* ==> */ IsUnchanged(qvKey)
	));
	if (!checker.Implies(othersUnchangedCheck))
		Throw("Specification violated by impure heap update", "multiple keys inserted/deleted at once");

	// begin debug output
	for (auto var : info.solver.GetVariablesInScope()) {
		if (checker.Implies(encoder.EncodeNow(*var).Equal(impureKey))) {
			log() << "      impure key == " << var->name << std::endl;
		}
	}
	// end debug output

	// check impure, part 2: check insertion/deletion for 'impureKey'
	bool isInsertion = checker.Implies(IsInserted(impureKey));
	bool isDeletion = checker.Implies(IsDeleted(impureKey)); // TODO important: add fast path, '!isInsertion &&'
	assert(!(isInsertion && isDeletion));
	if (isInsertion) purityStatus = PurityStatus::INSERTION;
	else if (isDeletion) purityStatus = PurityStatus::DELETION;
	else Throw("Failed to find specification for impure heap update", "unable to figure out if it is an insertion or a deletion");
}

void DerefPostComputer::CheckFootprint(Node node) {
	log() << "  checking footprint: " << node << std::endl;
	log() << "    checking invariant..." << std::endl;
	CheckInvariant(node);
	log() << "    checking specification..." << std::endl;
	CheckSpecification(node);
	log() << "    purity: " << (purityStatus == PurityStatus::PURE ? "pure" : (purityStatus == PurityStatus::INSERTION ? "insert" : "delete")) << std::endl;
}

bool DerefPostComputer::IsOutflowUnchanged(Node node, Selector selector) {
	log() << "    checking outflow: " << node << " -> " << selector << std::endl;
	const auto& outflow = info.solver.config.flowDomain->GetOutFlowContains(selector.fieldname);
	auto root = GetNode(0);
	assert(checker.ImpliesIsNonNull(node)); // TODO: assert with side effect

	// auto isUnchanged = encoder.MakeImplies(
	// 	MakeBoundaryKeyRestriction(interfaceKey, node, selector),
	// 	// encoder.EncodeNowFlow(root, interfaceKey), // TODO important: is this really correct???? only for decreasing flow?
	// 	/* ==> */
	// 	encoder.MakeIff(encoder.EncodeNowPredicate(outflow, node, interfaceKey), encoder.EncodeNextPredicate(outflow, node, interfaceKey))
	// );

	auto isUnchanged = encoder.MakeIff(
		encoder.EncodeNowPredicate(outflow, node, interfaceKey),
		encoder.EncodeNextPredicate(outflow, node, interfaceKey)
	);



	// auto isUnchanged = encoder.MakeForall(qvKey, encoder.MakeImplies(
	// 	encoder.EncodeNowFlow(root, qvKey), // TODO: now or next? should not matter
	// 	/* ==> */
	// 	encoder.MakeIff(encoder.EncodeNowPredicate(outflow, node, qvKey), encoder.EncodeNextPredicate(outflow, node, qvKey))
	// ));

	// // compute intraflow ==> everything produced within footprint // TODO important: avoid recomputing this
	// std::vector<Term> vector;
	// for (auto [intraNode, intraSelector, introSuccessor] : footprintIntraface) {
	// 	auto& outflowPred = info.solver.config.flowDomain->GetOutFlowContains(intraSelector.fieldname);
	// 	auto inOutflow = encoder.EncodeNowPredicate(outflowPred, intraNode, interfaceKey);
	// 	vector.push_back(inOutflow.Negate());
	// }
	// auto intrafaceKeyNotProducedInFootprint = encoder.MakeAnd(vector);

	// auto isUnchanged = encoder.MakeImplies(
	// 	intrafaceKeyNotProducedInFootprint,
	// 	/* ==> */
	// 	encoder.MakeIff(encoder.EncodeNowPredicate(outflow, node, interfaceKey), encoder.EncodeNextPredicate(outflow, node, interfaceKey))
	// );

	auto result = checker.Implies(isUnchanged);
	log() << "        ==> unchanged = " << result << std::endl;
	return result;
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////


void DerefPostComputer::ExploreAndCheckFootprint() {
	// TODO important: we are ignoring NULL during the exploration, can we ignore its flow / flow changes?? Are we ignoring NULL??

	InitializeFootprint();
	log() << "  initialized footprint with " << footprint.size() << " nodes" << std::endl;

	// decltype(footprint) checkedFootprint;
	decltype(footprintBoundary) checkedBoundary;
	auto maxFootprintSize = GetMaxFootprintSize();
	bool done;
	checker.Push();

	do {
		log() << "  iterating" << std::endl;
		done = true;

		// add flow rules for footprint ==> remove old rules as they are invalidated by footprint updates
		checker.Pop();
		checker.Push();
		AddFootprintFlowRulesToSolver();

		// check interface
		auto Expanded = [this](Node node, Selector selector) {
			if (IsOutflowUnchanged(node, selector)) return false;
			bool success = ExtendFootprint(node, selector);
			if (!success) Throw("Cannot verify heap update", "failed to extend footprint");
			return true;
		};
		for (auto edge : footprintBoundary) {
			if (checkedBoundary.count(edge)) continue;
			if (std::apply(Expanded, edge)) {
				done = false;
				break;
			}
			checkedBoundary.insert(edge);
		}

		// fail if footprint becomes to large
		log() << "  ~> footprint size = " << footprint.size() << std::endl;
		log() << "  ~> done = " << done << std::endl;
		if (footprint.size() > maxFootprintSize)
			Throw("Cannot verify heap update", "footprint too large");

	} while (!done);
		
	// check invariant / specification
	AddNonFootprintFlowRulesToSolver();
	for (auto node : footprint) {
		CheckFootprint(node);
	}

	// explicitly add invariant
	checker.Pop(); // TODO: should we do this??
	checker.AddPremise(encoder.EncodeNext(info.solver.GetInstantiatedInvariant()));
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
		bool inPre = heal::syntactically_contains_conjunct(info.preNow, *candidate);
		bool inPost = heal::syntactically_contains_conjunct(*result, *candidate);
		if (inPre == inPost) continue;
		log() << " diff " << (inPre ? "lost" : "got") << " " << *candidate << std::endl;
	}

	return result;
}
