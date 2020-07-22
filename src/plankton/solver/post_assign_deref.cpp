#include "plankton/solver/post.hpp"

#include "plankton/logger.hpp" // TODO: delete
// #include "cola/util.hpp"
// #include "plankton/util.hpp"

using namespace cola;
using namespace plankton;


struct DerefPostComputer {
	PostInfo info;
	const Dereference& lhs;
	const Expression& rhs;

	template<typename... Args>
	DerefPostComputer(PostInfo info_, const Dereference& lhs, const Expression& rhs) : info(std::move(info_)), lhs(lhs), rhs(rhs) {}
	std::unique_ptr<Annotation> MakePost();

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
	return DerefPostComputer(std::move(info), lhs, rhs).MakePost();
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<ConjunctionFormula> DerefPostComputer::MakePostNow() {
	log() << "**PRE**" << std::endl << info.preNow << std::endl;

	Encoder& encoder = info.solver.GetEncoder();
	auto solver = encoder.MakeSolver(Encoder::StepTag::NEXT); // TODO: which StepTag?


	// variables
	auto lhsNodeNow = encoder.Encode(*lhs.expr);
	auto lhsNext = encoder.EncodeNext(lhs);
	auto rhsNow = encoder.Encode(rhs);
	auto rhsNext = encoder.EncodeNext(rhs);

	const Type& nodeType = lhs.expr->type(); 
	VariableDeclaration dummyNode("qv-post-deref-ptr", nodeType, false); // TODO: avoid spoinling encoding with copies
	VariableDeclaration dummyKey("qv-post-deref-key", Type::data_type(), false); // TODO: avoid spoinling encoding with copies
	auto qvNode = encoder.EncodeVariable(dummyNode);
	auto qvKey = encoder.EncodeVariable(dummyKey);

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
