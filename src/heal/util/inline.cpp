#include "heal/util.hpp"

using namespace cola;
using namespace heal;


void heal::InlineAndSimplify(LogicObject& object) {
    // TODO: implement
//    throw std::logic_error("not yet implemented: InlineAndSimplify");
    heal::Simplify(object);
    std::cerr << "WARNING: heal::InlineAndSimplify not yet implemented!" << std::endl;
}
