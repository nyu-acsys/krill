#include "engine/util.hpp"

using namespace plankton;


struct ExpressionConverter : public BaseProgramVisitor {
    const Formula& formula;
    std::unique_ptr<SymbolicExpression> result = nullptr;

    explicit ExpressionConverter(const Formula& formula) : formula(formula) {}

    template<typename T>
    std::unique_ptr<SymbolicVariable> ConvertByEvaluation(const T& object) {
        auto& symbol = plankton::Evaluate(object, formula);
        return std::make_unique<SymbolicVariable>(symbol);
    }
    void Visit(const VariableExpression& object) override { result = ConvertByEvaluation(object); }
    void Visit(const Dereference& object) override { result = ConvertByEvaluation(object); }
    void Visit(const TrueValue& /*object*/) override { result = std::make_unique<SymbolicBool>(true); }
    void Visit(const FalseValue& /*object*/) override { result = std::make_unique<SymbolicBool>(false); }
    void Visit(const MinValue& /*object*/) override { result = std::make_unique<SymbolicMin>(); }
    void Visit(const MaxValue& /*object*/) override { result = std::make_unique<SymbolicMax>(); }
    void Visit(const NullValue& /*object*/) override { result = std::make_unique<SymbolicNull>(); }
};

void foo(){}

std::unique_ptr<SymbolicExpression> plankton::MakeSymbolic(const ValueExpression& expression, const Formula& context) {
    // TODO: ensure/assert that the necessary resources are present in context
    ExpressionConverter converter(context);
    expression.Accept(converter);
    assert(converter.result);
    return std::move(converter.result);
}
