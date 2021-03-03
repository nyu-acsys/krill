#include "prover/verify.hpp"

using namespace cola;
using namespace heal;
using namespace prover;


inline bool contains_name(std::string name, RenamingInfo& info) {
	for (const auto& decl : info.renamed_variables) {
		if (decl->name == name) {
			return true;
		}
	}
	return false;
}

inline std::string find_renaming(std::string name, RenamingInfo& info) {
	std::size_t counter = 0;
	std::string result;
	do {
		counter++;
		result = "interference#" + std::to_string(counter) + "#" + name;
	} while (contains_name(result, info));
	return result;
}

const VariableDeclaration& RenamingInfo::rename(const VariableDeclaration& decl) {
	// renames non-shared variables
	if (decl.is_shared) return decl;
	const VariableDeclaration* replacement;

	auto find = variable2renamed.find(&decl);
	if (find != variable2renamed.end()) {
		// use existing replacement
		replacement = find->second;

	} else {
		// create new replacement
		auto newName = find_renaming(decl.name, *this);
		auto newDecl = std::make_unique<VariableDeclaration>(newName, decl.type, decl.is_shared);
		replacement = newDecl.get();
		variable2renamed[&decl] = replacement;
		variable2renamed[replacement] = replacement; // prevent renamed variable from being renamed // TODO: why?
		renamed_variables.push_back(std::move(newDecl));
	}

	return *replacement;
}

transformer_t RenamingInfo::as_transformer() {
	return [this](const auto& expr){
		std::unique_ptr<Expression> replacement;
		auto check = is_of_type<VariableExpression>(expr);
		if (check.first) {
			replacement = std::make_unique<VariableExpression>(this->rename(check.second->decl));
		}
		return std::make_pair(replacement ? true : false, std::move(replacement));
	};
}
