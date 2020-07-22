#include "plankton/solver/post.hpp"

#include "plankton/logger.hpp" // TODO: delete
// #include "cola/util.hpp"
// #include "plankton/util.hpp"

using namespace cola;
using namespace plankton;


struct DerefPostComputer {
	PostInfo info;
	Encoder& encoder;
	const Dereference& lhs;
	const Expression& rhs;

	const Type& nodeType;
	z3::expr lhsNodeNow, lhsNodeNext;
	z3::expr lhsNow, lhsNext;
	z3::expr rhsNow, rhsNext;
	z3::expr qvNode, qvKey;

	DerefPostComputer(PostInfo info, const Dereference& lhs, const Expression& rhs);
	std::unique_ptr<Annotation> MakePost();

	bool IsHeapHomogeneous();
	std::size_t GetFootprintDepth();
	std::vector<std::string> OutgoingEdges();
	bool ImpliesInvariant(z3::expr node);
	bool IsNullOrOutflowUnchange(z3::expr node, std::string fieldname);
	z3::solver MakeBaseSolver();
	z3::expr MakeFootprintNode(std::size_t index);
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

template<Sort S>
const VariableDeclaration& GetDummyDecl() {
	static Type dummyType("dummy-post-deref-" + SortToString(S) + "-type", Sort::PTR);
	static VariableDeclaration dummyDecl("dummy-post-deref-" + SortToString(S) + "-var", dummyType, false);
	return dummyDecl;
}

DerefPostComputer::DerefPostComputer(PostInfo info_, const Dereference& lhs, const Expression& rhs)
	: info(std::move(info_)), encoder(info.solver.GetEncoder()), lhs(lhs), rhs(rhs), nodeType(lhs.expr->type()),
	  lhsNodeNow(encoder.Encode(*lhs.expr)), lhsNodeNext(encoder.EncodeNext(*lhs.expr)),
	  lhsNow(encoder.Encode(lhs)), lhsNext(encoder.EncodeNext(lhs)),
	  rhsNow(encoder.Encode(rhs)), rhsNext(encoder.EncodeNext(rhs)),
	  qvNode(encoder.EncodeVariable(GetDummyDecl<Sort::PTR>())),
	  qvKey(encoder.EncodeVariable(GetDummyDecl<Sort::DATA>()))
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
	throw std::logic_error("not yet implemented: DerefPostComputer::IsHeapHomogeneous");
}

std::size_t DerefPostComputer::GetFootprintDepth() {
	throw std::logic_error("not yet implemented: DerefPostComputer::GetFootprintDepth");
	// info.solver.config.flowDomain->GetFootprintSize(???, lhs, rhs);
}

z3::solver DerefPostComputer::MakeBaseSolver() {
	auto solver = encoder.MakeSolver(Encoder::StepTag::NEXT); // TODO: which StepTag?

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

	return solver;
}

z3::expr DerefPostComputer::MakeFootprintNode(std::size_t index) {
	static Type dummyType("dummy-post-deref-node-type", Sort::PTR);
	static std::vector<std::unique_ptr<VariableDeclaration>> dummyPointers;
	while (index >= dummyPointers.size()) {
		auto newDecl = std::make_unique<VariableDeclaration>("dummy-post-deref-ptr" + std::to_string(dummyPointers.size()), dummyType, false);
		dummyPointers.push_back(std::move(newDecl));
	}

	assert(index < dummyPointers.size());
	return encoder.EncodeVariable(*dummyPointers.at(index), Encoder::StepTag::NEXT);
}

std::vector<std::string> DerefPostComputer::OutgoingEdges() {
	// TODO: reuse code from encoder?
	throw std::logic_error("not yet implemented: DerefPostComputer::OutgoingEdges");
}

bool DerefPostComputer::ImpliesInvariant(z3::expr /*node*/) {
	// return checker.Implies(encoder.EncodeNextInvariant(node));
	throw std::logic_error("not yet implemented: DerefPostComputer::ImpliesInvariant");
}

bool DerefPostComputer::IsNullOrOutflowUnchange(z3::expr /*node*/, std::string /*fieldname*/) {
	throw std::logic_error("not yet implemented: DerefPostComputer::IsNullOrOutflowUnchange");
}

std::unique_ptr<ConjunctionFormula> DerefPostComputer::MakePostNow() {
	log() << "**PRE**" << std::endl << info.preNow << std::endl;

	auto solver = MakeBaseSolver();
	auto footprintDepth = GetFootprintDepth();

	std::size_t footprintSize = 0;
	auto NewFootprintNode = [this,&footprintSize](){ return MakeFootprintNode(footprintSize++); };	
	std::deque<std::pair<z3::expr, std::size_t>> worklist;

	// init worklist
	{
		auto root = NewFootprintNode();
		solver.add(root == lhsNodeNext);
		worklist.emplace_back(root, footprintDepth);

		// TODO important: add more roots
		if (lhs.sort() == Sort::PTR) {
			throw std::logic_error("not yet implemented: DerefPostComputer::MakePostNow()");
		}
	}

	// work worklist
	while (!worklist.empty()) {
		auto [node, remainingDepth] = worklist.front();
		worklist.pop_front();

		// ensure that invariant holds for node (invariant must handle NULL case itself)
		if (!ImpliesInvariant(node)) {
			throw SolvingError("Could not establish invariant for heap update '" + AssignmentString(lhs, rhs) + "': invariant violated.");
		}

		// check if outflow is unchanged; if not, add successor to worklist
		for (auto selectorName : OutgoingEdges()) {
			if (IsNullOrOutflowUnchange(node, selectorName))
				continue;
			else if (remainingDepth == 0)
				throw SolvingError("Could not establish invariant for heap update '" + AssignmentString(lhs, rhs) + "': footprint to small.");

			auto successor = NewFootprintNode();
			auto selector = std::make_pair(&nodeType, selectorName);
			auto bothNull = node == encoder.MakeNullPtr() && successor == encoder.MakeNullPtr();
			auto bothConnected = encoder.EncodeNextHeap(node, selector, successor);
			solver.add(bothNull || bothConnected);
			worklist.emplace_back(successor, remainingDepth -1);
		}
	}



	// TODO: purity checks
	// TODO: obl/ful transformation


	static ExpressionAxiom trueAxiom(std::make_unique<BooleanValue>(true));
	ImplicationCheckerImpl checker(encoder, solver, trueAxiom, Encoder::StepTag::NEXT);
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
