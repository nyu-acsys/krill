#include "testing.hpp"

using namespace cola;
using namespace heal;
using namespace solver;


const std::string CODE = R"(
#name "Vechev&Yahav DCAS Set"


struct Node {
	data_t val;
	Node* next;
}

Node* Head;
Node* Tail;


void __init__() {
	Tail = malloc;
	Tail->next = NULL;
	Tail->val = MAX_VAL;
	Head = malloc;
	Head->next = Tail;
	Head->val = MIN_VAL;
}


inline <Node*, Node*, data_t> locate(data_t key) {
	Node* curr, pred;
	data_t k;

	curr = Head;
	do {
		pred = curr;
		curr = pred->next;

		if (curr == pred->next && curr != NULL) {
			k = curr->val;
		} else {
			curr = Head;
			k = MIN_VAL;
		}
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
	Node* entry;

	entry = malloc;
	entry->val = key;

	while (true) {
		Node* curr, pred;
		data_t k;

		(pred, curr, k) = locate(key);

		if (k == key) {
			return false;

		} else {
			entry->next = curr;
			if (CAS(pred->next, curr, entry)) {
				return true;
			}
		}
	}
}


bool remove(data_t key) {
	while (true) {
		Node* curr, pred;
		data_t k;

		(pred, curr, k) = locate(key);

		if (k > key) {
			return false;

		} else {
			Node* next;

			next = curr->next;
			if (CAS((pred->next, curr->next), (curr, next), (next, NULL))) {
				return true;
			}
		}
	}
}
)";

const std::string NEXT = "next";
const std::string DATA = "val";
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

        // memory.node == head ==> memory.next != NULL && memory.data == MIN && memory.flow != \empty && [MIN, MAX] \subset memory.flow
        auto isHead = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(*head), std::make_unique<SymbolicVariable>(memory.node->Decl()));
        auto minData = MakeImmediateAxiom<SymbolicAxiom::EQ, SymbolicMin>(memory.fieldToValue.at(DATA)->Decl());
        auto allFlow = std::make_unique<InflowContainsRangeAxiom>(memory.flow, std::make_unique<SymbolicMin>(), std::make_unique<SymbolicMax>());
        auto hasFlow = std::make_unique<InflowEmptinessAxiom>(memory.flow, false);
        result->conjuncts.push_back(std::make_unique<SeparatingImplication>(heal::Copy(*isHead), heal::Copy(*nonNull)));
        result->conjuncts.push_back(std::make_unique<SeparatingImplication>(heal::Copy(*isHead), heal::Copy(*minData)));
        result->conjuncts.push_back((std::make_unique<SeparatingImplication>(heal::Copy(*isHead), heal::Copy(*hasFlow))));
        result->conjuncts.push_back(std::make_unique<SeparatingImplication>(heal::Copy(*isHead), heal::Copy(*allFlow)));

        // memory.node == tail ==> memory.next == NULL && memory.data == MAX && memory.flow != \empty
        auto isTail = std::make_unique<EqualsToAxiom>(std::make_unique<VariableExpression>(*tail), std::make_unique<SymbolicVariable>(memory.node->Decl()));
        auto isNull = MakeImmediateAxiom<SymbolicAxiom::EQ, SymbolicNull>(memory.fieldToValue.at(NEXT)->Decl());
        auto maxData = MakeImmediateAxiom<SymbolicAxiom::EQ, SymbolicMax>(memory.fieldToValue.at(DATA)->Decl());
        result->conjuncts.push_back(std::make_unique<SeparatingImplication>(heal::Copy(*isTail), heal::Copy(*isNull)));
        result->conjuncts.push_back((std::make_unique<SeparatingImplication>(heal::Copy(*isTail), heal::Copy(*maxData))));
        result->conjuncts.push_back((std::make_unique<SeparatingImplication>(heal::Copy(*isTail), heal::Copy(*hasFlow))));

        // memory.data == MIN ==> memory.node == head
        result->conjuncts.push_back(std::make_unique<SeparatingImplication>(heal::Copy(*minData), heal::Copy(*isHead)));

        // memory.data == MAX ==> memory.node == tail
        result->conjuncts.push_back((std::make_unique<SeparatingImplication>(heal::Copy(*maxData), heal::Copy(*isTail))));

        // memory.next != NULL ==> memory.flow != \empty && [memory.data, MAX] \subset memory.flow
        auto flow = std::make_unique<InflowContainsRangeAxiom>(memory.flow, heal::Copy(*memory.fieldToValue.at(DATA)), std::make_unique<SymbolicMax>());
//    result->conjuncts.push_back(std::make_unique<SeparatingImplication>(heal::Copy(*nonNull), heal::Copy(*hasFlow))); // redundant
        result->conjuncts.push_back(std::make_unique<SeparatingImplication>(heal::Copy(*nonNull), heal::Copy(*flow)));

        // memory.flow != \empty ==> [memory.data, MAX] \subset memory.flow
        result->conjuncts.push_back(std::make_unique<SeparatingImplication>(heal::Copy(*hasFlow), heal::Copy(*flow)));

        // memory.flow != \empty && memory.next != NULL ==> memory.data != MAX
        auto notMaxData = MakeImmediateAxiom<SymbolicAxiom::NEQ, SymbolicMax>(memory.fieldToValue.at(DATA)->Decl());
        auto premise = std::make_unique<SeparatingConjunction>();
        premise->conjuncts.push_back(heal::Copy(*hasFlow));
        premise->conjuncts.push_back(heal::Copy(*nonNull));
        result->conjuncts.push_back(std::make_unique<SeparatingImplication>(std::move(premise), heal::Copy(*notMaxData)));

        // memory.flow != \empty && memory.data != MAX ==> memory.next != NULL
        premise = std::make_unique<SeparatingConjunction>();
        premise->conjuncts.push_back(heal::Copy(*hasFlow));
        premise->conjuncts.push_back(heal::Copy(*notMaxData));
        result->conjuncts.push_back(std::make_unique<SeparatingImplication>(std::move(premise), heal::Copy(*nonNull)));

        // memory.flow != \empty ==> memory.next != NULL || memory.data == MAX
        // auto isMaxData = MakeImmediateAxiom<SymbolicAxiom::EQ, SymbolicMax>(memory.fieldToValue.at(DATA_FIELD)->Decl());
        // auto conclusion = std::make_unique<StackDisjunction>();
        // conclusion->axioms.push_back(std::move(isMaxData));
        // conclusion->axioms.push_back(std::move(nonNull));
        // result->conjuncts.push_back(std::make_unique<SeparatingImplication>(std::move(hasFlow), std::move(conclusion)));

        // memory.next != NULL ==> memory.data != MAX
        result->conjuncts.push_back(std::make_unique<SeparatingImplication>(heal::Copy(*nonNull), heal::Copy(*notMaxData)));

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
        return MakeImmediateAxiom<SymbolicAxiom::LT, SymbolicVariable>(memory.fieldToValue.at(DATA)->Decl(), value);
    }

    [[nodiscard]] std::unique_ptr<heal::Formula> MakeContains(const heal::PointsToAxiom& memory, const heal::SymbolicVariableDeclaration& value) const override {
        return MakeImmediateAxiom<SymbolicAxiom::EQ, SymbolicVariable>(memory.fieldToValue.at(DATA)->Decl(), value);
    }

};

int main(int /*argc*/, char** /*argv*/) {
    MyBenchmark benchmark;
    return benchmark();
}


