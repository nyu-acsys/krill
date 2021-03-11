// TODO: delete file

#include "heal/util.hpp"

#include <map>

using namespace cola;
using namespace heal;


//static const VariableDeclaration& GetDeclaration(std::size_t index, const Type& type) {
//    static std::map<const Type*, std::deque<VariableDeclaration>> typeList;
//    auto& deque = typeList[&type]; // default constructs entry if not present
//
//    while (index >= deque.size()) {
//        std::string name = "qv^" + type.name + "^" + std::to_string(deque.size());
//        deque.emplace_back(name, type, false);
//    }
//
//    assert(index < deque.size());
//    return deque.at(index);
//}
//
//heal::QuantifiedVariableGenerator::QuantifiedVariableGenerator(std::shared_ptr<Encoder> encoder_) : encoder(std::move(encoder_)) {}
//
//Symbol heal::QuantifiedVariableGenerator::GetNext(const cola::Type& type) {
//    auto& decl = GetDeclaration(counter++, type);
//    return encoder->Encode(decl, EncodingTag::NOW);
//}
//
//
//std::vector<Symbol> heal::MakeQuantifiedVariables(std::shared_ptr<Encoder> encoder, std::vector<std::reference_wrapper<const cola::Type>> types) {
//    QuantifiedVariableGenerator generator(std::move(encoder));
//    std::vector<Symbol> result;
//    result.reserve(types.size());
//    for (auto type : types) {
//        result.push_back(generator.GetNext(type));
//    }
//    return result;
//}