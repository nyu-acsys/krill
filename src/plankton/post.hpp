#pragma once
#ifndef PLANKTON_POST
#define PLANKTON_POST


#include <memory>
#include "cola/ast.hpp"
#include "plankton/logic.hpp"


namespace plankton {

	std::unique_ptr<Annotation> post(std::unique_ptr<Annotation> pre, const cola::Assume& cmd);
	std::unique_ptr<Annotation> post(std::unique_ptr<Annotation> pre, const cola::Malloc& cmd);
	std::unique_ptr<Annotation> post(std::unique_ptr<Annotation> pre, const cola::Assignment& cmd);

	inline std::unique_ptr<Annotation> post(const Annotation& pre, const cola::Assume& cmd) { return post(copy(pre), cmd); }
	inline std::unique_ptr<Annotation> post(const Annotation& pre, const cola::Malloc& cmd) { return post(copy(pre), cmd); }
	inline std::unique_ptr<Annotation> post(const Annotation& pre, const cola::Assignment& cmd) { return post(copy(pre), cmd); }

} // namespace plankton

#endif