#include "heal/symbolic.hpp"

#include "heal/util.hpp"

using namespace cola;
using namespace heal;


bool SymbolicVariableSetComparator::operator() (const std::reference_wrapper<const cola::VariableDeclaration>& var, const std::reference_wrapper<const cola::VariableDeclaration>& other) const {
    return &var.get() < &other.get();
}

struct SymbolicVariableCollector : public DefaultListener, public BaseVisitor {
    using DefaultListener::visit;

    SymbolicVariableSet result;

    void visit(const VariableDeclaration& node) override { result.insert(std::ref(node)); }
    void visit(const BooleanValue& /*node*/) override { /* do nothing */ }
    void visit(const NullValue& /*node*/) override { /* do nothing */ }
    void visit(const EmptyValue& /*node*/) override { /* do nothing */ }
    void visit(const MaxValue& /*node*/) override { /* do nothing */ }
    void visit(const MinValue& /*node*/) override { /* do nothing */ }
    void visit(const NDetValue& /*node*/) override { /* do nothing */ }
    void visit(const VariableExpression& node) override { node.decl.accept(*this); }
    void visit(const NegatedExpression& node) override { node.expr->accept(*this); }
    void visit(const BinaryExpression& node) override { node.lhs->accept(*this); node.rhs->accept(*this); }
    void visit(const Dereference& node) override { node.expr->accept(*this); }

    void enter(const ExpressionAxiom& formula) override { formula.expr->accept(*this); }
    void enter(const OwnershipAxiom& formula) override { formula.expr->accept(*this); }
    void enter(const DataStructureLogicallyContainsAxiom& formula) override { formula.value->accept(*this); }
    void enter(const NodeLogicallyContainsAxiom& formula) override { formula.node->accept(*this); formula.value->accept(*this); }
    void enter(const KeysetContainsAxiom& formula) override { formula.node->accept(*this); formula.value->accept(*this); }
    void enter(const HasFlowAxiom& formula) override { formula.expr->accept(*this); }
    void enter(const FlowContainsAxiom& formula) override { formula.node->accept(*this); formula.value_low->accept(*this); formula.value_high->accept(*this); }
    void enter(const UniqueInflowAxiom& formula) override { formula.node->accept(*this); }
    void enter(const ObligationAxiom& formula) override { formula.key->accept(*this); }
    void enter(const FulfillmentAxiom& formula) override { formula.key->accept(*this); }
};

SymbolicVariableSet heal::CollectSymbolicVariables(const LogicObject& object) {
    SymbolicVariableCollector collector;
    object.accept(collector);
    return std::move(collector.result);
}

class SymbolicVariablePool {
    private:
        SymbolicVariablePool() = default;
        std::deque<std::unique_ptr<VariableDeclaration>> variables;

        std::string MakeNewName(const Type& type) {
            switch (type.sort) {
                case Sort::VOID: return "@v" + std::to_string(variables.size());
                case Sort::BOOL: return "@b" + std::to_string(variables.size());
                case Sort::DATA: return "@d" + std::to_string(variables.size());
                case Sort::PTR:  return "@a" + std::to_string(variables.size());
            }
        }

    public:
        static SymbolicVariablePool& Get() {
            static SymbolicVariablePool pool;
            return pool;
        }

        const VariableDeclaration& NewVariable(const Type& type) {
            variables.push_back(std::make_unique<VariableDeclaration>(MakeNewName(type), type, false));
            return *variables.back();
        }

        [[nodiscard]] const decltype(variables)& GetVariables() const {
            return variables;
        }
};

bool heal::IsSymbolicVariable(const VariableDeclaration& decl) {
    auto& symbolicVariables = SymbolicVariablePool::Get().GetVariables();
    return symbolicVariables.end() != std::find_if(symbolicVariables.begin(), symbolicVariables.end(), [&decl](const auto& other) { return other.get() == &decl; });
}

const VariableDeclaration& heal::MakeFreshSymbolicVariable(const Type& type) {
    return SymbolicVariablePool::Get().NewVariable(type);
}

const VariableDeclaration& heal::GetUnusedSymbolicVariable(const Type& type, const SymbolicVariableSet& variablesInUse) {
    auto isUsed = [&variablesInUse](const auto* decl) -> bool {
        return variablesInUse.end() != std::find_if(variablesInUse.begin(), variablesInUse.end(), [&decl](const auto& other) { return &other.get() == decl; });
    };
    for (const auto& decl : SymbolicVariablePool::Get().GetVariables()) {
        if (decl->type == type && !isUsed(decl.get())) return *decl;
    }
    return MakeFreshSymbolicVariable(type);
}

const VariableDeclaration& heal::GetUnusedSymbolicVariable(const Type& type, const LogicObject& object) {
    return heal::GetUnusedSymbolicVariable(type, heal::CollectSymbolicVariables(object));
}

template<typename T>
std::unique_ptr<T> ApplyRenaming(std::unique_ptr<T> object, const LogicObject& avoidSymbolicVariablesFrom) {
    auto variablesInUse = heal::CollectSymbolicVariables(avoidSymbolicVariablesFrom);
    auto variablesToRename = heal::CollectSymbolicVariables(*object);
    std::map<const VariableDeclaration*, const VariableDeclaration*> replacementMap;
    for (const VariableDeclaration& decl : variablesToRename) {
        auto& replacement = GetUnusedSymbolicVariable(decl.type, variablesInUse);
        replacementMap[&decl] = &replacement;
        variablesInUse.insert(std::ref(replacement));
    }
    auto transformer = [&replacementMap](const cola::Expression& expr){
        std::pair<bool,std::unique_ptr<cola::Expression>> result;
        result.first = false;
        auto [isDecl, decl] = heal::IsOfType<VariableExpression>(expr);
        if (isDecl) {
            auto find = replacementMap.find(&decl->decl);
            if (find != replacementMap.end()) {
                result.first = true;
                result.second = std::make_unique<VariableExpression>(*find->second);
            }
        }
        return result;
    };
    return heal::ReplaceExpression(std::move(object), transformer);
}

std::unique_ptr<Formula> heal::RenameSymbolicVariables(std::unique_ptr<Formula> formula, const LogicObject& avoidSymbolicVariablesFrom) {
    return ApplyRenaming(std::move(formula), avoidSymbolicVariablesFrom);
}

std::unique_ptr<TimePredicate> heal::RenameSymbolicVariables(std::unique_ptr<TimePredicate> predicate, const LogicObject& avoidSymbolicVariablesFrom) {
    return ApplyRenaming(std::move(predicate), avoidSymbolicVariablesFrom);
}

std::unique_ptr<Annotation> heal::RenameSymbolicVariables(std::unique_ptr<Annotation> annotation, const LogicObject& avoidSymbolicVariablesFrom) {
    return ApplyRenaming(std::move(annotation), avoidSymbolicVariablesFrom);
}
