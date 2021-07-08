#include "testing.hpp"

using namespace cola;
using namespace heal;
using namespace solver;


const std::string CODE = R"(
#name "Vechev&Yahav CAS Set"


struct Node {
	data_t val;
	bool marked;
	Node* next;
}

Node* Head;
Node* Tail;


void __init__() {
	Tail = malloc;
	Tail->next = NULL;
	Tail->marked = false;
	Tail->val = MAX_VAL;
	Head = malloc;
	Head->next = Tail;
	Head->marked = false;
	Head->val = MIN_VAL;
}


inline <Node*, Node*, data_t> locate(data_t key) {
	Node* curr, pred;
	data_t k;

	curr = Head;
	do {
		pred = curr;
		curr = pred->next;
        k = curr->val;
	} while (k < key);
    return <pred, curr, k>;
}


bool contains(data_t key) {
	Node* curr, pred;
	data_t k;

	(pred, curr, k) = locate(key);
	return k == key;
}

bool add(data_t key) {
	Node* curr, pred, entry;
	data_t k;

	entry = malloc;
	entry->val = key;
	entry->marked = false;

	while (true) {
		(pred, curr, k) = locate(key);

		if (k == key) {
            return false;

		} else {
			entry->next = curr;
            if (CAS((pred->marked, pred->next), (false, curr), (false, entry))) {
				return true;
			}
		}
	}
}


bool remove(data_t key) {
	Node* curr, pred, next;
	data_t k;

	while (true) {
		(pred, curr, k) = locate(key);

		if (k > key) {
			return false;

		} else {
            // TODO: support any curr->marked, not just unmarked
            next = curr->next;
			if (CAS((curr->marked, curr->next), (false, next), (true, next))) {
                if (CAS((pred->marked, pred->next), (false, curr), (false, next))) {
                    return true;
                }
			}
		}
	}
}
)";

const std::string NEXT = "next";
const std::string DATA = "val";
const std::string MARK = "marked";
const std::string HEAD = "Head";
const std::string TAIL = "Tail";
const std::string NODE = "Node";

template<SymbolicAxiom::Operator OP, typename I, typename... Args>
inline std::unique_ptr<SymbolicAxiom> MakeImmediateAxiom(const SymbolicVariableDeclaration& decl, Args&&... args) {
    return std::make_unique<SymbolicAxiom>(std::make_unique<SymbolicVariable>(decl), OP, std::make_unique<I>(std::forward<Args>(args)...));
}


struct MyBenchmark : public Benchmark {
    explicit MyBenchmark() : Benchmark(CODE, NODE, HEAD, TAIL) {}

    [[nodiscard]] std::unique_ptr<heal::Formula> MakeSharedNodeInvariant(const heal::PointsToAxiom& memory, const cola::VariableDeclaration* head, const cola::VariableDeclaration* tail) const override {
        assert(head);
        assert(tail);
        auto result = std::make_unique<SeparatingConjunction>();
        auto nonNull = MakeImmediateAxiom<SymbolicAxiom::NEQ, SymbolicNull>(memory.fieldToValue.at(NEXT)->Decl());

        // memory.node == head ==> memory.next != NULL && memory.data == MIN && memory.marked == false && memory.flow != \empty && [MIN, MAX] \subset memory.flow
        auto isHead = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(*head), std::make_unique<SymbolicVariable>(memory.node->Decl()));
        auto minData = MakeImmediateAxiom<SymbolicAxiom::EQ, SymbolicMin>(memory.fieldToValue.at(DATA)->Decl());
        auto isUnmarked = MakeImmediateAxiom<SymbolicAxiom::EQ, SymbolicBool>(memory.fieldToValue.at(MARK)->Decl(), false);
        auto allFlow = std::make_unique<InflowContainsRangeAxiom>(memory.flow, std::make_unique<SymbolicMin>(), std::make_unique<SymbolicMax>());
        auto hasFlow = std::make_unique<InflowEmptinessAxiom>(memory.flow, false);
        result->conjuncts.push_back(std::make_unique<SeparatingImplication>(heal::Copy(*isHead), heal::Copy(*nonNull)));
        result->conjuncts.push_back(std::make_unique<SeparatingImplication>(heal::Copy(*isHead), heal::Copy(*minData)));
        result->conjuncts.push_back(std::make_unique<SeparatingImplication>(heal::Copy(*isHead), heal::Copy(*isUnmarked)));
        result->conjuncts.push_back((std::make_unique<SeparatingImplication>(heal::Copy(*isHead), heal::Copy(*hasFlow))));
        result->conjuncts.push_back(std::make_unique<SeparatingImplication>(heal::Copy(*isHead), heal::Copy(*allFlow)));

        // memory.node == tail ==> memory.next == NULL && memory.data == MAX && memory.marked == false && memory.flow != \empty
        auto isTail = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(*tail), std::make_unique<SymbolicVariable>(memory.node->Decl()));
        auto isNull = MakeImmediateAxiom<SymbolicAxiom::EQ, SymbolicNull>(memory.fieldToValue.at(NEXT)->Decl());
        auto maxData = MakeImmediateAxiom<SymbolicAxiom::EQ, SymbolicMax>(memory.fieldToValue.at(DATA)->Decl());
        result->conjuncts.push_back(std::make_unique<SeparatingImplication>(heal::Copy(*isTail), heal::Copy(*isNull)));
        result->conjuncts.push_back((std::make_unique<SeparatingImplication>(heal::Copy(*isTail), heal::Copy(*maxData))));
        result->conjuncts.push_back((std::make_unique<SeparatingImplication>(heal::Copy(*isTail), heal::Copy(*isUnmarked))));
        result->conjuncts.push_back((std::make_unique<SeparatingImplication>(heal::Copy(*isTail), heal::Copy(*hasFlow))));

        // memory.data == MIN ==> memory.node == head
        result->conjuncts.push_back(std::make_unique<SeparatingImplication>(heal::Copy(*minData), heal::Copy(*isHead)));

        // memory.data == MAX ==> memory.node == tail
        result->conjuncts.push_back((std::make_unique<SeparatingImplication>(heal::Copy(*maxData), heal::Copy(*isTail))));

        // memory.marked == false ==> [memory.data, MAX] \subset memory.flow
        auto flow = std::make_unique<InflowContainsRangeAxiom>(memory.flow, heal::Copy(*memory.fieldToValue.at(DATA)), std::make_unique<SymbolicMax>());
        result->conjuncts.push_back(std::make_unique<SeparatingImplication>(heal::Copy(*isUnmarked), heal::Copy(*flow)));

        // memory.flow != \empty ==> memory.marked == false
//        result->conjuncts.push_back(std::make_unique<SeparatingImplication>(heal::Copy(*hasFlow), heal::Copy(*isUnmarked)));

        // memory.flow == \empty ==> memory.marked == true
        auto noFlow = std::make_unique<InflowEmptinessAxiom>(memory.flow, true);
        auto isMarked = MakeImmediateAxiom<SymbolicAxiom::EQ, SymbolicBool>(memory.fieldToValue.at(MARK)->Decl(), true);
        result->conjuncts.push_back(std::make_unique<SeparatingImplication>(heal::Copy(*noFlow), heal::Copy(*isMarked)));

        // memory.flow != \empty ==> [memory.data, MAX] \subset memory.flow
        result->conjuncts.push_back(std::make_unique<SeparatingImplication>(heal::Copy(*hasFlow), heal::Copy(*flow)));

        // memory.next == NULL ==> memory.node == tail
        result->conjuncts.push_back(std::make_unique<SeparatingImplication>(heal::Copy(*isNull), heal::Copy(*isTail)));

        return result;
    }

    [[nodiscard]] std::unique_ptr<heal::Formula> MakeLocalNodeInvariant(const heal::PointsToAxiom& memory) const override {
        return std::make_unique<InflowEmptinessAxiom>(memory.flow, true);
    }

    [[nodiscard]] std::unique_ptr<heal::Formula> MakeVariableInvariant(const heal::EqualsToAxiom& variable) const override {
        auto result = std::make_unique<SeparatingConjunction>();
        if (!variable.variable->decl.is_shared) return result;
        result->conjuncts.push_back(MakeImmediateAxiom<SymbolicAxiom::NEQ, SymbolicNull>(variable.value->Decl()));
        return result;
    }

    [[nodiscard]] std::unique_ptr<heal::Formula> MakeOutflow(const heal::PointsToAxiom& memory, const std::string& fieldName, const heal::SymbolicVariableDeclaration& value) const override {
        assert(fieldName == NEXT);
//        auto isMarked = MakeImmediateAxiom<SymbolicAxiom::EQ, SymbolicBool>(memory.fieldToValue.at(MARK)->Decl(), true);
//        auto goesOut = MakeImmediateAxiom<SymbolicAxiom::LT, SymbolicVariable>(memory.fieldToValue.at(DATA)->Decl(), value);
//        auto result = std::make_unique<StackDisjunction>();
//        result->axioms.push_back(std::move(isMarked));
//        result->axioms.push_back(std::move(goesOut));
//        return result;
        return MakeImmediateAxiom<SymbolicAxiom::LT, SymbolicVariable>(memory.fieldToValue.at(DATA)->Decl(), value);
    }

    [[nodiscard]] std::unique_ptr<heal::Formula> MakeContains(const heal::PointsToAxiom& memory, const heal::SymbolicVariableDeclaration& value) const override {
//        auto isUnmarked = MakeImmediateAxiom<SymbolicAxiom::EQ, SymbolicBool>(memory.fieldToValue.at(MARK)->Decl(), false);
//        auto isData = MakeImmediateAxiom<SymbolicAxiom::EQ, SymbolicVariable>(memory.fieldToValue.at(DATA)->Decl(), value);
//        auto result = std::make_unique<SeparatingConjunction>();
//        result->conjuncts.push_back(std::move(isUnmarked));
//        result->conjuncts.push_back(std::move(isData));
//        return result;
        return MakeImmediateAxiom<SymbolicAxiom::EQ, SymbolicVariable>(memory.fieldToValue.at(DATA)->Decl(), value);
    }

};

int main(int /*argc*/, char** /*argv*/) {
    MyBenchmark benchmark;
    return benchmark();
//    benchmark(5);
//    return 0;
}
