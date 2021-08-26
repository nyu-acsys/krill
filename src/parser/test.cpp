#include "parser/parse.hpp"

#include <iostream>
#include <sstream>

using namespace plankton;


constexpr std::string_view CODE = R"(
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
	Node* pred, curr;
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
	Node* pred, curr;
	data_t k;

	<pred, curr, k> = locate(key);
    return k == key;
}

bool add(data_t key) {
	Node* entry, pred, curr;
	data_t k;

	entry = malloc;
	entry->val = key;
	entry->marked = false;

	while (true) {
		<pred, curr, k> = locate(key);

		if (k == key) {
            return false;

		} else {
			entry->next = curr;
            if (CAS(<pred->marked, pred->next>, <false, curr>, <false, entry>)) {
				return true;
			}
		}
	}
}

bool remove(data_t key) {
	Node* pred, curr, next;
	data_t k;

	while (true) {
		<pred, curr, k> = locate(key);

		if (k > key) {
			return false;

		} else {
            // TODO: support any curr->marked, not just unmarked
            next = curr->next;
			if (CAS(<curr->marked, curr->next>, <false, next>, <true, next>)) {
                if (CAS(<pred->marked, pred->next>, <false, curr>, <false, next>)) {
                    return true;
                }
			}
		}
	}
}
)";

int main(int /*argc*/, char** /*argv*/) {
    std::stringstream stream;
    stream << CODE;
    
    std::cout << std::endl << "Parsing file... " << std::flush;
    auto program = plankton::ParseProgram(stream);
    std::cout << "done" << std::endl;
    std::cout << *program << std::endl;
    
    return 0;
}
