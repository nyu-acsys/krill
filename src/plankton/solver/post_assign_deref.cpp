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
};

std::unique_ptr<Annotation> plankton::MakeDerefAssignPost(PostInfo info, const cola::Dereference& lhs, const cola::Expression& rhs) {
	return DerefPostComputer(std::move(info), lhs, rhs).MakePost();
}


std::unique_ptr<Annotation> DerefPostComputer::MakePost() {
	throw std::logic_error("not yet implemented: DerefPostComputer::MakePost()");
}