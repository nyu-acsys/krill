#include "plankton/post.hpp"

using namespace cola;
using namespace plankton;


std::unique_ptr<Annotation> plankton::post(std::unique_ptr<Annotation> /*pre*/, const Assume& /*cmd*/) {
	throw std::logic_error("not yet implemented: plankton::post(std::unique_ptr<Annotation>, const Assume&)");
}

std::unique_ptr<Annotation> plankton::post(std::unique_ptr<Annotation> /*pre*/, const Malloc& /*cmd*/) {
	// TODO: extend_current_annotation(... knowledge about all members of new object, flow...)
	// TODO: destroy knowledge about current lhs (and everything that is not guaranteed to survive the assignment)
	throw std::logic_error("not yet implemented: plankton::post(std::unique_ptr<Annotation>, const Malloc&)");
}

std::unique_ptr<Annotation> plankton::post(std::unique_ptr<Annotation> /*pre*/, const Assignment& /*cmd*/) {
	// TODO: destroy knowledge about current lhs (and everything that is not guaranteed to survive the assignment)
	throw std::logic_error("not yet implemented: plankton::post(std::unique_ptr<Annotation>, const Assignment&)");
}
