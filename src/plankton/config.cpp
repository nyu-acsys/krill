#include "plankton/config.hpp"

#include <iostream> // TODO: delete

using namespace plankton;


std::unique_ptr<PlanktonConfig> plankton::config = std::make_unique<BaseConfig>();


PlanktonConfig::OutFlowInfo::OutFlowInfo(const cola::Type& node, const std::string field, const Predicate& prop)
	: node_type(node), fieldname(field), contains_key(prop)
{
	std::cout << "HELLO" << std::endl;
	std::cout << node_type.name << std::endl;
	std::cout << contains_key.vars.at(0)->type.name << std::endl;
	std::cout << &node_type << std::endl;
	std::cout << &contains_key.vars.at(0)->type << std::endl;

	assert(&node_type == &contains_key.vars.at(0)->type);
	assert(node_type.field(fieldname));
}