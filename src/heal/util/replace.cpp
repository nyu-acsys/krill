#include "heal/util.hpp"

#include "cola/util.hpp"

using namespace cola;
using namespace heal;


struct VariableReplacer : public DefaultLogicNonConstListener {
	const transformer_t& transformer;
	bool performedReplacement = false;

	explicit VariableReplacer(const transformer_t& transformer_) : transformer(transformer_) {}

	void enter(LogicVariable& variable) override {
	    auto replacement = transformer(variable.Decl());
	    if (replacement) {
	        variable.decl_storage = *replacement;
	        performedReplacement = true;
	    }
	}
};


template<typename T>
std::unique_ptr<T> make_replacement(std::unique_ptr<T> obj, const transformer_t& transformer, bool* changed) {
	VariableReplacer replacer(transformer);
	obj->accept(replacer);
	if (changed) *changed = replacer.performedReplacement;
	return std::move(obj);
}

std::unique_ptr<LogicObject> heal::Replace(std::unique_ptr<LogicObject> object, const transformer_t& transformer, bool* changed) {
    return make_replacement(std::move(object), transformer, changed);
}
std::unique_ptr<SeparatingConjunction> heal::Replace(std::unique_ptr<SeparatingConjunction> formula, const transformer_t& transformer, bool* changed) {
    return make_replacement(std::move(formula), transformer, changed);
}
std::unique_ptr<TimePredicate> heal::Replace(std::unique_ptr<TimePredicate> predicate, const transformer_t& transformer, bool* changed) {
    return make_replacement(std::move(predicate), transformer, changed);
}
std::unique_ptr<Annotation> heal::Replace(std::unique_ptr<Annotation> annotation, const transformer_t& transformer, bool* changed) {
    return make_replacement(std::move(annotation), transformer, changed);
}
