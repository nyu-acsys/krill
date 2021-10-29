#include "programs/util.hpp"

using namespace plankton;


inline bool IsEqual(const VariableExpression& object, const VariableExpression& other) {
    return object.Decl() == other.Decl();
}

inline bool IsEqual(const TrueValue& /*object*/, const TrueValue& /*other*/) {
    return true;
}

inline bool IsEqual(const FalseValue& /*object*/, const FalseValue& /*other*/) {
    return true;
}

inline bool IsEqual(const MinValue& /*object*/, const MinValue& /*other*/) {
    return true;
}

inline bool IsEqual(const MaxValue& /*object*/, const MaxValue& /*other*/) {
    return true;
}

inline bool IsEqual(const NullValue& /*object*/, const NullValue& /*other*/) {
    return true;
}

inline bool IsEqual(const Dereference& object, const Dereference& other) {
    return object.variable->Decl() == other.variable->Decl() && object.fieldName == other.fieldName;
}

#include "util/log.hpp"
inline bool IsEqual(const BinaryExpression& object, const BinaryExpression& other) {
    DEBUG(" <chk IsEqual for BinaryExpression> " << object << "   " << other << std::endl)
    return object.op == other.op
           && plankton::SyntacticalEqual(*object.lhs, *other.lhs)
           && plankton::SyntacticalEqual(*object.rhs, *other.rhs);
}

struct ProgramComparator : public BaseProgramVisitor {
    bool result = false;
    const Expression& compare;

    explicit ProgramComparator(const Expression& compare) : compare(compare) {}

    template<typename T>
    inline void Compare(const T& object) {
        if (auto other = dynamic_cast<const T*>(&compare)) {
            result = IsEqual(object, *other);
        }
    }

    void Visit(const VariableExpression& object) override { Compare(object); }
    void Visit(const TrueValue& object) override { Compare(object); }
    void Visit(const FalseValue& object) override { Compare(object); }
    void Visit(const MinValue& object) override { Compare(object); }
    void Visit(const MaxValue& object) override { Compare(object); }
    void Visit(const NullValue& object) override { Compare(object); }
    void Visit(const Dereference& object) override { Compare(object); }
    void Visit(const BinaryExpression& object) override { Compare(object); }
};

bool plankton::SyntacticalEqual(const Expression& object, const Expression& other) {
    ProgramComparator comparator(other);
    object.Accept(comparator);
    return comparator.result;
}
