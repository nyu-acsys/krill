#include "parser/builder.hpp"
#include "PlanktonBaseVisitor.h"
#include "util/shortcuts.hpp"

using namespace plankton;

FlowConstraintsParsingResult AstBuilder::MakeFlowgraphs(PlanktonParser::FlowgraphsContext& context) {
    FlowConstraintsParsingResult result;
    result.config = MakeConfig(context);
    for (auto* elem : context.graphs) result.constraints.push_back(MakeFlowgraph(*elem, *result.config));
    plankton::MoveInto(std::move(_types), result.types);
    for (auto* ctx : context.option()) {
        auto ident = ctx->ident->getText();
        if (ident != "name") continue;
        result.name = ctx->str->getText();
        result.name = result.name.substr(1,result.name.length()-2);
    }
    return result;
}

struct SymbolicValueBuilder : public PlanktonBaseVisitor {
    std::unique_ptr<SymbolicExpression> result = nullptr;

    antlrcpp::Any visitValueTrue(PlanktonParser::ValueTrueContext*) override { result = std::make_unique<SymbolicBool>(true); return nullptr; }
    antlrcpp::Any visitValueFalse(PlanktonParser::ValueFalseContext*) override { result = std::make_unique<SymbolicBool>(false); return nullptr; }
    antlrcpp::Any visitValueMax(PlanktonParser::ValueMaxContext*) override { result = std::make_unique<SymbolicMax>(); return nullptr; }
    antlrcpp::Any visitValueMin(PlanktonParser::ValueMinContext*) override { result = std::make_unique<SymbolicMin>(); return nullptr; }
    antlrcpp::Any visitValueNull(PlanktonParser::ValueNullContext*) override { result = std::make_unique<SymbolicNull>(); return nullptr; }
};

std::unique_ptr<SymbolicExpression> MakeSymbolicValue(PlanktonParser::ValueContext& context) {
    SymbolicValueBuilder builder;
    context.accept(&builder);
    if (builder.result) return std::move(builder.result);
    throw std::logic_error("Unknown value.");
}

std::unique_ptr<FlowConstraint> AstBuilder::MakeFlowgraph(PlanktonParser::GraphContext& context, const SolverConfig& config) {
    auto& flowType = config.GetFlowValueType();
    SymbolFactory factory;
    std::map<std::string, const SymbolDeclaration*> name2map;
    auto getSymbolOrNull = [&](const std::string& name)-> const SymbolDeclaration* {
        auto find = name2map.find(name);
        if (find != name2map.end()) return find->second;
        return nullptr;
    };
    auto translateSymbol = [&](const std::string& name, const Type& type)-> const SymbolDeclaration& {
        if (auto result = getSymbolOrNull(name)) return *result;
        auto& symbol = factory.GetFreshFO(type);
        name2map[name] = &symbol;
        return symbol;
    };
    auto getType = [&](std::string name) -> const Type& {
        auto* type = TypeByNameOrNull(name);
        if (type) return *type;
        name.pop_back();
        return TypeByName(name);
    };

    // create nodes
    std::deque <Node> nodes;
    for (auto* nodeContext : context.nodes) {
        auto& type = getType(nodeContext->type()->getText());
        auto& address = translateSymbol(nodeContext->name->getText(), type);
        nodes.emplace_back(address, factory, flowType);
        auto& node = nodes.back();
        if (nodeContext->preUnreach) node.unreachablePre = true;
        if (nodeContext->preNoFlow) node.emptyInflow = true;
        for (auto selectorContext : nodeContext->fields) {
            auto& selType = getType(selectorContext->type()->getText());
            auto selName = selectorContext->name->getText();
            auto& selPre = translateSymbol(selectorContext->pre->getText(), selType);
            auto& selPost = translateSymbol(selectorContext->post->getText(), selType);
            if (selType.sort == Sort::PTR) node.pointerSelectors.emplace_back(selName, selType, selPre, selPost, factory, flowType);
            else node.dataSelectors.emplace_back(selName, selType, selPre, selPost);
        }
    }

    // create constraints
    auto constraints = std::make_unique<SeparatingConjunction>();
    auto makeExpr = [&](PlanktonParser::GraphConstraintExprContext& ctx) -> std::unique_ptr<SymbolicExpression> {
        if (ctx.immi) return MakeSymbolicValue(*ctx.immi);
        else if (ctx.id) {
            auto symbol = getSymbolOrNull(ctx.id->getText());
            if (!symbol) return nullptr;
            return std::make_unique<SymbolicVariable>(*symbol);
        }
        throw std::logic_error("Unknown expression.");
    };
    auto makeOp = [](PlanktonParser::GraphConstraintOpContext& ctx) -> BinaryOperator {
        if (ctx.Eq()) return BinaryOperator::EQ;
        else if (ctx.Neq()) return BinaryOperator::NEQ;
        else if (ctx.Lt()) return BinaryOperator::LT;
        else if (ctx.Lte()) return BinaryOperator::LEQ;
        else if (ctx.Gte()) return BinaryOperator::GEQ;
        else if (ctx.Gt()) return BinaryOperator::GT;
        else throw std::logic_error("Unknown binary operator.");
    };
    for (auto* constraintContext : context.constraints) {
        auto lhs = makeExpr(*constraintContext->lhs);
        auto rhs = makeExpr(*constraintContext->rhs);
        if (!lhs || !rhs) continue;
        BinaryOperator op = makeOp(*constraintContext->op);
        constraints->Conjoin(std::make_unique<StackAxiom>(op, std::move(lhs), std::move(rhs)));
    }

    auto result = std::make_unique<FlowConstraint>(config, std::move(constraints));
    result->nodes = std::move(nodes);

    // options
    for (auto* optionContext : context.option()) {
        auto ident = optionContext->ident->getText();
        if (ident != "name") continue;
        result->name = optionContext->str->getText();
        result->name = result->name.substr(1,result->name.length()-2);
    }

    return result;
}

void Traverse(std::ostream& stream, const antlr4::tree::ParseTree& ctx) {
    for (auto* child : ctx.children) {
        if (child->children.empty()) stream << child->getText() << " ";
        Traverse(stream, *child);
    }
}

#include <regex>
std::string PrintContext(const antlr4::tree::ParseTree& context) {
    std::stringstream stream;
    Traverse(stream, context);
    auto res = stream.str();
    std::regex_replace(res, std::regex(" \\*"), "*");
    res = std::regex_replace(res, std::regex(" ;"), ";");
    res = std::regex_replace(res, std::regex(" ,"), ",");
    res = std::regex_replace(res, std::regex(" \\[ "), "[");
    res = std::regex_replace(res, std::regex(" \\] "), "]");
    res = std::regex_replace(res, std::regex("\\( "), "(");
    res = std::regex_replace(res, std::regex(" \\)"), ")");
    return res;
}

std::string AstBuilder::MakeFootprintConfig(const Program& program, PlanktonParser::ProgramContext& context) {
    std::stringstream result;
    result << "#name " << program.name << std::endl << std::endl;
    for (auto* ctx : context.structs) result << PrintContext(*ctx) << std::endl;
    for (auto* ctx : context.outf) result << PrintContext(*ctx) << std::endl;
    return result.str();
}
