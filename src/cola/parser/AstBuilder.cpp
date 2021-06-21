#include "AstBuilder.hpp"

#include "TypeBuilder.hpp"
#include "cola/util.hpp"
#include "cola/parse.hpp"
#include <sstream>

using namespace cola;

// TODO: extract line number and pass to ParseError

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct VarExtractor : public Visitor {
	const VariableDeclaration* result = nullptr;
	void visit(const VariableExpression& var) override { result = &var.decl; }
	void visit(const VariableDeclaration& /*node*/) override { /* do nothing */ }
	void visit(const Expression& /*node*/) override { /* do nothing */ }
	void visit(const BooleanValue& /*node*/) override { /* do nothing */ }
	void visit(const NullValue& /*node*/) override { /* do nothing */ }
	void visit(const EmptyValue& /*node*/) override { /* do nothing */ }
	void visit(const MaxValue& /*node*/) override { /* do nothing */ }
	void visit(const MinValue& /*node*/) override { /* do nothing */ }
	void visit(const NDetValue& /*node*/) override { /* do nothing */ }
	void visit(const NegatedExpression& /*node*/) override { /* do nothing */ }
	void visit(const BinaryExpression& /*node*/) override { /* do nothing */ }
	void visit(const Dereference& /*node*/) override { /* do nothing */ }
	void visit(const Sequence& /*node*/) override { /* do nothing */ }
	void visit(const Scope& /*node*/) override { /* do nothing */ }
	void visit(const Atomic& /*node*/) override { /* do nothing */ }
	void visit(const Choice& /*node*/) override { /* do nothing */ }
	void visit(const IfThenElse& /*node*/) override { /* do nothing */ }
	void visit(const Loop& /*node*/) override { /* do nothing */ }
	void visit(const While& /*node*/) override { /* do nothing */ }
	void visit(const DoWhile& /*node*/) override { /* do nothing */ }
	void visit(const Skip& /*node*/) override { /* do nothing */ }
	void visit(const Break& /*node*/) override { /* do nothing */ }
	void visit(const Continue& /*node*/) override { /* do nothing */ }
	void visit(const Assume& /*node*/) override { /* do nothing */ }
	void visit(const Assert& /*node*/) override { /* do nothing */ }
	void visit(const Return& /*node*/) override { /* do nothing */ }
	void visit(const Malloc& /*node*/) override { /* do nothing */ }
    void visit(const Assignment& /*node*/) override { /* do nothing */ }
    void visit(const ParallelAssignment& /*node*/) override { /* do nothing */ }
	void visit(const Macro& /*node*/) override { /* do nothing */ }
	void visit(const CompareAndSwap& /*node*/) override { /* do nothing */ }
	void visit(const Function& /*node*/) override { /* do nothing */ }
	void visit(const Program& /*node*/) override { /* do nothing */ }
};

static const VariableDeclaration& expression2Variable(const Expression& expr) {
	VarExtractor extractor;
	expr.accept(extractor);
	if (extractor.result) {
		return *extractor.result;
	} else {
		throw ParseError("Expected a variable, got an expression.");
	}
}


struct AssignableVisitor : public Visitor {
	bool result = true;
	void visit(const VariableExpression& /*node*/) override { /* do nothing */ }
	void visit(const Dereference& expr) override { expr.expr->accept(*this); }
	void visit(const BooleanValue& /*node*/) override { result = false; }
	void visit(const NullValue& /*node*/) override { result = false; }
	void visit(const EmptyValue& /*node*/) override { result = false; }
	void visit(const MaxValue& /*node*/) override { result = false; }
	void visit(const MinValue& /*node*/) override { result = false; }
	void visit(const NDetValue& /*node*/) override { result = false; }
	void visit(const NegatedExpression& /*node*/) override { result = false; }
	void visit(const BinaryExpression& /*node*/) override { result = false; }
	
	void visit(const Expression& /*node*/) override { throw std::logic_error("Unexpected invocation (AssignableVisitor::visit(const Expression&)"); }
	void visit(const VariableDeclaration& /*node*/) override { throw std::logic_error("Unexpected invocation (AssignableVisitor::visit(const VariableDeclaration&)"); }
	void visit(const Sequence& /*node*/) override { throw std::logic_error("Unexpected invocation (AssignableVisitor::visit(const Sequence&)"); }
	void visit(const Scope& /*node*/) override { throw std::logic_error("Unexpected invocation (AssignableVisitor::visit(const Scope&)"); }
	void visit(const Atomic& /*node*/) override { throw std::logic_error("Unexpected invocation (AssignableVisitor::visit(const Atomic&)"); }
	void visit(const Choice& /*node*/) override { throw std::logic_error("Unexpected invocation (AssignableVisitor::visit(const Choice&)"); }
	void visit(const IfThenElse& /*node*/) override { throw std::logic_error("Unexpected invocation (AssignableVisitor::visit(const IfThenElse&)"); }
	void visit(const Loop& /*node*/) override { throw std::logic_error("Unexpected invocation (AssignableVisitor::visit(const Loop&)"); }
	void visit(const While& /*node*/) override { throw std::logic_error("Unexpected invocation (AssignableVisitor::visit(const While&)"); }
	void visit(const DoWhile& /*node*/) override { throw std::logic_error("Unexpected invocation (AssignableVisitor::visit(const DoWhile&)"); }
	void visit(const Skip& /*node*/) override { throw std::logic_error("Unexpected invocation (AssignableVisitor::visit(const Skip&)"); }
	void visit(const Break& /*node*/) override { throw std::logic_error("Unexpected invocation (AssignableVisitor::visit(const Break&)"); }
	void visit(const Continue& /*node*/) override { throw std::logic_error("Unexpected invocation (AssignableVisitor::visit(const Continue&)"); }
	void visit(const Assume& /*node*/) override { throw std::logic_error("Unexpected invocation (AssignableVisitor::visit(const Assume&)"); }
	void visit(const Assert& /*node*/) override { throw std::logic_error("Unexpected invocation (AssignableVisitor::visit(const Assert&)"); }
	void visit(const Return& /*node*/) override { throw std::logic_error("Unexpected invocation (AssignableVisitor::visit(const Return&)"); }
	void visit(const Malloc& /*node*/) override { throw std::logic_error("Unexpected invocation (AssignableVisitor::visit(const Malloc&)"); }
    void visit(const Assignment& /*node*/) override { throw std::logic_error("Unexpected invocation (AssignableVisitor::visit(const Assignment&)"); }
    void visit(const ParallelAssignment& /*node*/) override { throw std::logic_error("Unexpected invocation (AssignableVisitor::visit(const ParallelAssignment&)"); }
	void visit(const Macro& /*node*/) override { throw std::logic_error("Unexpected invocation (AssignableVisitor::visit(const Macro&)"); }
	void visit(const CompareAndSwap& /*node*/) override { throw std::logic_error("Unexpected invocation (AssignableVisitor::visit(const CompareAndSwap&)"); }
	void visit(const Function& /*node*/) override { throw std::logic_error("Unexpected invocation (AssignableVisitor::visit(const Function&)"); }
	void visit(const Program& /*node*/) override { throw std::logic_error("Unexpected invocation (AssignableVisitor::visit(const Program&)"); }
};

static bool isExprAssignable(const Expression& expr) {
	AssignableVisitor visitor;
	expr.accept(visitor);
	return visitor.result;
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void AstBuilder::pushScope() {
	_scope.emplace_back();
}

std::vector<std::unique_ptr<VariableDeclaration>> AstBuilder::popScope() {
	std::vector<std::unique_ptr<VariableDeclaration>> decls, tmp;
	decls.reserve(_scope.back().size());
	tmp.reserve(_scope.back().size());
	for (auto& entry : _scope.back()) { // reverses variable order?
		tmp.push_back(std::move(entry.second));
	}
	while (!tmp.empty()) {
		decls.push_back(std::move(tmp.back()));
		tmp.pop_back();
	}
	_scope.pop_back();
	return decls;
}

bool AstBuilder::isVariableDeclared(std::string variableName) {
	for (auto it = _scope.crbegin(); it != _scope.rend(); it++) {
		if (it->count(variableName)) {
			return true;
		}
	}

	return false;
}

const VariableDeclaration& AstBuilder::lookupVariable(std::string variableName) {
	std::size_t counter = 0;
	for (auto it = _scope.crbegin(); it != _scope.rend(); it++) {
		if (it->count(variableName)) {
			return *it->at(variableName);
		}
		counter++;
	}

	throw ParseError("Variable '" + variableName + "' not declared.");
}

void AstBuilder::addVariable(std::unique_ptr<VariableDeclaration> variable) {
	if (isVariableDeclared(variable->name)) {
		throw ParseError("variable '" + variable->name + "' already declared.");
	}

	_scope.back()[variable->name] = std::move(variable);
}

bool AstBuilder::isTypeDeclared(std::string typeName) {
	return _types.count(typeName) != 0;
}

const Type& AstBuilder::lookupType(std::string typeName) {
	auto it = _types.find(typeName);
	if (it == _types.end()) {
		std::string help = _types.count(typeName + "*") ? " Did you mean '" + typeName + "*'?" : "";
		throw ParseError("Type '" + typeName + "' not declared." + help);
	} else {
		return it->second;
	}
}

void AstBuilder::addFunction(Function& function) {
	if (function.kind == Function::Kind::INIT) {
		if (_found_init) {
			throw ParseError("Duplicate initializer declaration: '" + INIT_NAME + "' already defined.");
		}
		_found_init = true;
		// do not make initializer available for calls

	} else {
		if (function.name == INIT_NAME) {
			throw ParseError("Function name not allowed: '" + function.name + "' is reserved for the initializer, but function does not meet its signature.");
		} else if (_functions.count(function.name)) {
			throw ParseError("Duplicate function declaration: function with name '" + function.name + "' already defined.");
		}
		_functions.insert({{ function.name, function }}); 
	}
}

std::unique_ptr<Statement> AstBuilder::mk_stmt_from_list(std::vector<cola::CoLaParser::StatementContext*> stmts) {
	std::vector<Statement*> list;
	stmts.reserve(stmts.size());
	for (auto& stmt : stmts) {
		list.push_back(stmt->accept(this).as<Statement*>());
	}
	return mk_stmt_from_list(list);
}

std::unique_ptr<Statement> AstBuilder::mk_stmt_from_list(std::vector<Statement*> stmts) {
	if (stmts.size() == 0) {
		return std::make_unique<Skip>();
	} else {
		std::unique_ptr<Statement> current(stmts.at(0));
		for (std::size_t i = 1; i < stmts.size(); i++) {
			std::unique_ptr<Statement> next(stmts.at(i));
			auto seq = std::make_unique<Sequence>(std::move(current), std::move(next));
			current = std::move(seq);
		}
		return current;
	}
}

std::string AstBuilder::iTh(std::size_t number) {
	switch (number) {
		case 0: return "0th";
		case 1: return "1st";
		case 2: return "2nd";
		default: return std::to_string(number) + "th";
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<Program> AstBuilder::buildFrom(cola::CoLaParser::ProgramContext* parseTree) {
	AstBuilder builder;
	parseTree->accept(&builder);
	return builder._program;
}

antlrcpp::Any AstBuilder::visitProgram(cola::CoLaParser::ProgramContext* context) {
	_program = std::make_unique<Program>();
	_program->name = "unknown";

	// get options
	for (auto optionContext : context->opts()) {
		optionContext->accept(this);
	}

	// get types
	auto typeinfo = TypeBuilder::build(context);
	_types = std::move(typeinfo->lookup);
	_program->types = std::move(typeinfo->types);

	// add shared variables
	pushScope();
	for (auto variableContext : context->var_decl()) {
		variableContext->accept(this);
	}
	for (auto& kvpair : _scope.back()) {
		kvpair.second->is_shared = true;
	}

	// functions
	_program->functions.reserve(context->function().size());
	for (auto functionContext : context->function()) {
		functionContext->accept(this);
	}

	if (!_program->initializer) {
		throw ParseError("Program has no 'init' function.");
	}

	// finish
	_program->variables = popScope();

	return nullptr;
}

antlrcpp::Any AstBuilder::visitFunctionInterface(CoLaParser::FunctionInterfaceContext* context) {
	auto typeList = context->types->accept(this).as<TypeList>();
	auto argList = context->args->accept(this).as<ArgDeclList>();
	return handleFunction(Function::INTERFACE, context->name->getText(), std::move(typeList), std::move(argList), context->body);
}

antlrcpp::Any AstBuilder::visitFunctionMacro(CoLaParser::FunctionMacroContext* context) {
	auto typeList = context->types->accept(this).as<TypeList>();
	auto argList = context->args->accept(this).as<ArgDeclList>();
	return handleFunction(Function::MACRO, context->name->getText(), std::move(typeList), std::move(argList), context->body);
}

antlrcpp::Any AstBuilder::visitFunctionInit(CoLaParser::FunctionInitContext* context) {
	return handleFunction(Function::INIT, INIT_NAME, { Type::void_type() }, { }, context->body);
}

void AstBuilder::addFunctionArgumentScope(const Function& function, const ArgDeclList& args) {
	pushScope();
	for (auto [varName, varTypeName] : args) {
		const Type& varType = _types.at(varTypeName);

		if (varType.sort == Sort::VOID) {
			throw ParseError("Type 'void' not allowed for function argument declaration '" + varName + "' in function '" + function.name + "'.");
		}
		if (function.kind == Function::INTERFACE && varType.sort == Sort::PTR) {
			throw ParseError("Argument with name '" + varName + "' of pointer sort not allowed for interface function '" + function.name + "'.");
		}

		auto decl = std::make_unique<VariableDeclaration>(varName, varType, false);
		addVariable(std::move(decl));
	}
}

std::vector<std::unique_ptr<VariableDeclaration>> AstBuilder::retrieveFunctionArgumentScope(const ArgDeclList& args) {
	auto scope = popScope();
	std::vector<std::unique_ptr<VariableDeclaration>> result;
	for (const auto& [varName, varTypeName] : args) {
		for (auto& decl : scope) {
			if (decl && decl->name == varName) {
				result.push_back(std::move(decl));
				break;
			}
		}
	}
	scope.clear();
	return result;
}

antlrcpp::Any AstBuilder::handleFunction(Function::Kind kind, std::string name, TypeList types, ArgDeclList args, CoLaParser::ScopeContext* bodyContext) {
	// prepare function
	auto dummyBody = std::make_unique<Scope>(std::make_unique<Skip>()); // we need to create the function here, but have no body yet
	auto function = std::make_unique<Function>(name, kind, std::move(types), std::move(dummyBody));
	_currentFunction = function.get();
	addFunction(*function);

	// handle arguments and body
	addFunctionArgumentScope(*function, args);
	assert(bodyContext);
	function->body.reset(bodyContext->accept(this).as<Scope*>());
	assert(function->args.empty());
	function->args = retrieveFunctionArgumentScope(args);

	// transfer ownershp
	if (kind == Function::INIT) {
		_program->initializer = std::move(function);
	} else {
		_program->functions.push_back(std::move(function));
	}

	// TODO: check if every path returns?
	return nullptr;
}

antlrcpp::Any AstBuilder::visitTypeList(CoLaParser::TypeListContext* context) {
	TypeList result;
	result.reserve(context->types.size());
	for (auto typeContext : context->types) {
		auto typeName = typeContext->accept(this).as<std::string>();
		const Type& type = lookupType(typeName);
		result.push_back(type);
	}
	if (result.size() > 1) {
		for (const Type& type : result) {
			if (type.sort != Sort::VOID) continue;
			throw ParseError("Type lists must not contain types of sort 'VOID'.");
		}
	}
	return result;
}

antlrcpp::Any AstBuilder::visitArgDeclList(cola::CoLaParser::ArgDeclListContext* context) {
	ArgDeclList dlist; // TODO important: std::make_shared<ArgDeclList>();
	auto size = context->argTypes.size();
	dlist.reserve(size);
	assert(size == context->argNames.size());
	for (std::size_t i = 0; i < size; i++) {
		std::string name = context->argNames.at(i)->getText();
		const Type& type = lookupType(context->argTypes.at(i)->accept(this).as<std::string>());
		// TODO: what the ****?! why can't I pass the reference wrapper here without them being lost after the loop?
		dlist.push_back(std::make_pair(name, type.name));
	}
	return dlist;
}

antlrcpp::Any AstBuilder::visitScope(cola::CoLaParser::ScopeContext* context) {
	pushScope();
	// handle variables
	auto vars = context->var_decl();
	for (auto varDeclContext : vars) {
		varDeclContext->accept(this);
	}

	// handle body
	auto body = mk_stmt_from_list(context->statement());
	assert(body);

	auto scope = new Scope(std::move(body));
	scope->variables = popScope();
	return scope;
}

antlrcpp::Any AstBuilder::visitStruct_decl(cola::CoLaParser::Struct_declContext* /*context*/) {
	throw std::logic_error("Unsupported operation."); // this is done in TypeBuilder.hpp
}

antlrcpp::Any AstBuilder::visitField_decl(cola::CoLaParser::Field_declContext* /*context*/) {
	throw std::logic_error("Unsupported operation."); // this is done in TypeBuilder.hpp
}

antlrcpp::Any AstBuilder::visitNameVoid(cola::CoLaParser::NameVoidContext* /*context*/) {
	std::string name = "void";
	return name;
}

antlrcpp::Any AstBuilder::visitNameBool(cola::CoLaParser::NameBoolContext* /*context*/) {
	std::string name = "bool";
	return name;
}

antlrcpp::Any AstBuilder::visitNameInt(cola::CoLaParser::NameIntContext* /*context*/) {
	throw ParseError("Type 'int' not supported. Use 'data_t' instead.");
}

antlrcpp::Any AstBuilder::visitNameData(cola::CoLaParser::NameDataContext* /*context*/) {
	std::string name = "data_t";
	return name;
}

antlrcpp::Any AstBuilder::visitNameIdentifier(cola::CoLaParser::NameIdentifierContext* context) {
	std::string name = context->Identifier()->getText();
	return name;
}

antlrcpp::Any AstBuilder::visitTypeValue(cola::CoLaParser::TypeValueContext* context) {
	std::string name = context->name->accept(this).as<std::string>();
	return name;
}

antlrcpp::Any AstBuilder::visitTypePointer(cola::CoLaParser::TypePointerContext* context) {
	std::string name = context->name->accept(this).as<std::string>() + "*";
	return name;
}

antlrcpp::Any AstBuilder::visitVarDeclRoot(CoLaParser::VarDeclRootContext* /*context*/) {
	throw ParseError("Annotation '@root' is not supported yet.");
}

antlrcpp::Any AstBuilder::visitVarDeclList(CoLaParser::VarDeclListContext* context) {
	const Type& type = lookupType(context->type()->accept(this).as<std::string>());
	for (auto token : context->names) {
		std::string name = token->getText();
		auto decl = std::make_unique<VariableDeclaration>(name, type, false); // default to non-shared
		addVariable(std::move(decl));
	}
	return nullptr;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static Expression* as_expression(Expression* expr) {
	return expr;
}

antlrcpp::Any AstBuilder::visitValueNull(cola::CoLaParser::ValueNullContext* /*context*/) {
	return as_expression(new NullValue());
}

antlrcpp::Any AstBuilder::visitValueTrue(cola::CoLaParser::ValueTrueContext* /*context*/) {
	return as_expression(new BooleanValue(true));
}

antlrcpp::Any AstBuilder::visitValueFalse(cola::CoLaParser::ValueFalseContext* /*context*/) {
	return as_expression(new BooleanValue(false));
}

antlrcpp::Any AstBuilder::visitValueNDet(cola::CoLaParser::ValueNDetContext* /*context*/) {
	return as_expression(new NDetValue());
}

antlrcpp::Any AstBuilder::visitValueEmpty(cola::CoLaParser::ValueEmptyContext* /*context*/) {
	return as_expression(new EmptyValue());
}

antlrcpp::Any AstBuilder::visitValueMax(cola::CoLaParser::ValueMaxContext* /*context*/) {
	return as_expression(new MaxValue());
}

antlrcpp::Any AstBuilder::visitValueMin(cola::CoLaParser::ValueMinContext* /*context*/) {
	return as_expression(new MinValue());
}

antlrcpp::Any AstBuilder::visitExprValue(cola::CoLaParser::ExprValueContext* context) {
	return as_expression(context->value()->accept(this));
}

antlrcpp::Any AstBuilder::visitExprParens(cola::CoLaParser::ExprParensContext* context) {
	return as_expression(context->expr->accept(this));
}

antlrcpp::Any AstBuilder::visitExprIdentifier(cola::CoLaParser::ExprIdentifierContext* context) {
	std::string name = context->name->getText();
	const VariableDeclaration& decl = lookupVariable(name);
	return as_expression(new VariableExpression(decl));
}

antlrcpp::Any AstBuilder::visitExprNegation(cola::CoLaParser::ExprNegationContext* context) {
	auto expr = context->expr->accept(this).as<Expression*>();
	return as_expression(new NegatedExpression(std::unique_ptr<Expression>(expr)));
}

antlrcpp::Any AstBuilder::visitExprDeref(cola::CoLaParser::ExprDerefContext* context) {
	auto expr = context->expr->accept(this).as<Expression*>();
	std::string fieldname = context->field->getText();
	const Type& exprtype = expr->type();

	if (exprtype.sort != Sort::PTR) {
		throw ParseError("Cannot dereference expression of non-pointer type.");
	}
	if (exprtype == Type::null_type()) {
		throw ParseError("Cannot dereference 'null'.");
	}
	if (!exprtype.has_field(fieldname)) {
		throw ParseError("Expression evaluates to type '" + exprtype.name + "' which does not have a field '" + fieldname + "'.");
	}

	return as_expression(new Dereference(std::unique_ptr<Expression>(expr), fieldname));
}

Expression* AstBuilder::mk_binary_expr(CoLaParser::ExpressionContext* lhsContext, BinaryExpression::Operator op, CoLaParser::ExpressionContext* rhsContext) {
	bool is_logic = op == BinaryExpression::Operator::AND || op == BinaryExpression::Operator::OR;
	bool is_equality = op == BinaryExpression::Operator::EQ || op == BinaryExpression::Operator::NEQ;

	auto lhs = lhsContext->accept(this).as<Expression*>();
	auto& lhsType = lhs->type();

	auto rhs = rhsContext->accept(this).as<Expression*>();
	auto& rhsType = rhs->type();

	if (!comparable(lhsType, rhsType)) {
		throw ParseError("Type error: cannot compare types '" + lhsType.name + "' and " + rhsType.name + "' using operator '" + toString(op) + "'.");
	}
	if (lhsType.sort == Sort::PTR && !is_equality) {
		throw ParseError("Type error: pointer types allow only for (in)equality comparison.");
	}
	if (is_logic && lhsType.sort != Sort::BOOL) {
		throw ParseError("Type error: logic operators require bool type.");
	}

	return as_expression(new BinaryExpression(op, std::unique_ptr<Expression>(lhs), std::unique_ptr<Expression>(rhs)));
}

antlrcpp::Any AstBuilder::visitExprBinaryEq(cola::CoLaParser::ExprBinaryEqContext* context) {
	return mk_binary_expr(context->lhs, BinaryExpression::Operator::EQ, context->rhs);
}
antlrcpp::Any AstBuilder::visitExprBinaryNeq(cola::CoLaParser::ExprBinaryNeqContext* context) {
	return mk_binary_expr(context->lhs, BinaryExpression::Operator::NEQ, context->rhs);
}
antlrcpp::Any AstBuilder::visitExprBinaryLt(cola::CoLaParser::ExprBinaryLtContext* context) {
	return mk_binary_expr(context->lhs, BinaryExpression::Operator::LT, context->rhs);
}
antlrcpp::Any AstBuilder::visitExprBinaryLte(cola::CoLaParser::ExprBinaryLteContext* context) {
	return mk_binary_expr(context->lhs, BinaryExpression::Operator::LEQ, context->rhs);
}
antlrcpp::Any AstBuilder::visitExprBinaryGt(cola::CoLaParser::ExprBinaryGtContext* context) {
	return mk_binary_expr(context->lhs, BinaryExpression::Operator::GT, context->rhs);
}
antlrcpp::Any AstBuilder::visitExprBinaryGte(cola::CoLaParser::ExprBinaryGteContext* context) {
	return mk_binary_expr(context->lhs, BinaryExpression::Operator::GEQ, context->rhs);
}
antlrcpp::Any AstBuilder::visitExprBinaryAnd(cola::CoLaParser::ExprBinaryAndContext* context) {
	return mk_binary_expr(context->lhs, BinaryExpression::Operator::AND, context->rhs);
}
antlrcpp::Any AstBuilder::visitExprBinaryOr(cola::CoLaParser::ExprBinaryOrContext* context) {
	return mk_binary_expr(context->lhs, BinaryExpression::Operator::OR, context->rhs);
}

antlrcpp::Any AstBuilder::visitExprCas(cola::CoLaParser::ExprCasContext* context) {
	return as_expression(context->cas()->accept(this).as<CompareAndSwap*>());
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

antlrcpp::Any AstBuilder::visitBlockStmt(cola::CoLaParser::BlockStmtContext* context) {
	Statement* stmt = context->statement()->accept(this);
	Scope* scope = new Scope(std::unique_ptr<Statement>(stmt));
	return scope;
}

antlrcpp::Any AstBuilder::visitBlockScope(cola::CoLaParser::BlockScopeContext* context) {
	Scope* scope = context->scope()->accept(this).as<Scope*>();
	return scope;
}

static Statement* as_statement(Statement* stmt) {
	return stmt;
}

antlrcpp::Any AstBuilder::visitStmtIf(cola::CoLaParser::StmtIfContext* context) {
	auto ifExpr = std::unique_ptr<Expression>(context->expr->accept(this).as<Expression*>());
	auto ifScope = std::unique_ptr<Scope>(context->bif->accept(this).as<Scope*>());
	std::unique_ptr<Scope> elseScope;
	if (context->belse) {
		elseScope = std::unique_ptr<Scope>(context->belse->accept(this).as<Scope*>());
	} else {
		elseScope = std::make_unique<Scope>(std::make_unique<Skip>());
	}
	assert(elseScope);
	auto result = new IfThenElse(std::move(ifExpr), std::move(ifScope), std::move(elseScope));
	return as_statement(result);
}

antlrcpp::Any AstBuilder::visitStmtWhile(cola::CoLaParser::StmtWhileContext* context) {
	bool was_in_loop = _inside_loop;
	_inside_loop = true;
	auto expr = std::unique_ptr<Expression>(context->expr->accept(this).as<Expression*>());
	auto scope = std::unique_ptr<Scope>(context->body->accept(this).as<Scope*>());
	auto result = new While(std::move(expr), std::move(scope));
	_inside_loop = was_in_loop;
	return as_statement(result);
}

antlrcpp::Any AstBuilder::visitStmtDo(cola::CoLaParser::StmtDoContext* context) {
	bool was_in_loop = _inside_loop;
	_inside_loop = true;
	auto expr = std::unique_ptr<Expression>(context->expr->accept(this).as<Expression*>());
	auto scope = std::unique_ptr<Scope>(context->body->accept(this).as<Scope*>());
	auto result = new DoWhile(std::move(expr), std::move(scope));
	_inside_loop = was_in_loop;
	return as_statement(result);
}

antlrcpp::Any AstBuilder::visitStmtChoose(cola::CoLaParser::StmtChooseContext* context) {
	auto result = new Choice();
	result->branches.reserve(context->scope().size());
	for (auto scope : context->scope()) {
		result->branches.push_back(std::unique_ptr<Scope>(scope->accept(this).as<Scope*>()));
	}
	return as_statement(result);
}

antlrcpp::Any AstBuilder::visitStmtLoop(cola::CoLaParser::StmtLoopContext* context) {
	bool was_in_loop = _inside_loop;
	_inside_loop = true;
	auto scope = std::unique_ptr<Scope>(context->scope()->accept(this).as<Scope*>());
	_inside_loop = was_in_loop;
	return as_statement(new Loop(std::move(scope)));
}

antlrcpp::Any AstBuilder::visitStmtAtomic(cola::CoLaParser::StmtAtomicContext* context) {
	auto scope = std::unique_ptr<Scope>(context->body->accept(this).as<Scope*>());
	auto result = new Atomic(std::move(scope));
	return as_statement(result);
}

antlrcpp::Any AstBuilder::visitStmtCom(cola::CoLaParser::StmtComContext* context) {
	auto result = context->command()->accept(this).as<Statement*>();
	return as_statement(result);
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<std::unique_ptr<Expression>> AstBuilder::mk_expr_from_list(std::vector<cola::CoLaParser::ExpressionContext*> exprs) {
	std::vector<std::unique_ptr<Expression>> result;
	for (const auto& context : exprs) {
		auto expr = std::unique_ptr<Expression>(context->accept(this).as<Expression*>());
		result.push_back(std::move(expr));
	}
	return result;
}

Statement* AstBuilder::as_command(Statement* stmt) {
	return stmt;
}

antlrcpp::Any AstBuilder::visitCmdSkip(cola::CoLaParser::CmdSkipContext* /*context*/) {
	return as_command(new Skip());
}

antlrcpp::Any AstBuilder::visitCmdAssign(cola::CoLaParser::CmdAssignContext* context) {
	// TODO: check if lhs is assignable
	auto lhs = std::unique_ptr<Expression>(context->lhs->accept(this).as<Expression*>());
	auto rhs = std::unique_ptr<Expression>(context->rhs->accept(this).as<Expression*>());
	if (!assignable(lhs->type(), rhs->type())) {
		throw ParseError("Type error: cannot assign to expression of type '" + lhs->type().name + "' from expression of type '" + rhs->type().name + "'.");
	}
	if (lhs->type() == Type::void_type()) {
		throw ParseError("Type error: cannot assign to type 'void'.");
	}
	if (!isExprAssignable(*lhs)) {
		std::stringstream s;
		cola::print(*lhs, s);
		throw ParseError("Expression '" + s.str()  + "' is not assignable.");
	}
	return as_command(new Assignment(std::move(lhs), std::move(rhs)));
}

antlrcpp::Any AstBuilder::visitCmdMalloc(cola::CoLaParser::CmdMallocContext* context) {
	auto name = context->lhs->getText();
	auto& lhs = lookupVariable(name);
	if (lhs.type.sort != Sort::PTR) {
		throw ParseError("Type error: cannot assign to expression of non-pointer type.");
	}
	if (lhs.type == Type::void_type()) {
		throw ParseError("Type error: cannot assign to 'null'.");
	}
	return as_command(new Malloc(lhs));
}

antlrcpp::Any AstBuilder::visitCmdAssume(cola::CoLaParser::CmdAssumeContext* context) {
	auto expr = std::unique_ptr<Expression>(context->expr->accept(this).as<Expression*>());
	return as_command(new Assume(std::move(expr)));
}

antlrcpp::Any AstBuilder::visitCmdAssert(cola::CoLaParser::CmdAssertContext* context) {
	auto expr = std::unique_ptr<Expression>(context->expr->accept(this).as<Expression*>());
	return as_command(new Assert(std::move(expr)));
}

antlrcpp::Any AstBuilder::visitCmdCall(cola::CoLaParser::CmdCallContext* context) {
	std::string name = context->name->getText();
	if (!_functions.count(name)) {
		throw ParseError("Call to undeclared function '" + name + "'.");
	}

	const Function& function = _functions.at(name);
	if (function.kind == Function::INTERFACE) {
		throw ParseError("Call to interface function '" + name + "' from interface function '" + _currentFunction->name + "', can only call 'inline'/'extern' functions.");
	}
	assert(function.kind == Function::MACRO);

	// get arguments
	std::vector<std::unique_ptr<Expression>> args;
	auto argsraw = context->args->accept(this).as<std::vector<Expression*>>();
	args.reserve(argsraw.size());
	for (Expression* expr : argsraw) {
		args.push_back(std::unique_ptr<Expression>(expr));
	}

	// check argument number
	if (function.args.size() != args.size()) {
		std::stringstream msg;
		msg << "Candidate function not applicable: '" << function.name << "' requires " << function.args.size() << " arguments, ";
		msg << args.size() << " arguments provided.";
		throw ParseError(msg.str());
	}

	// check argument types
	for (std::size_t i = 0; i < args.size(); i++) {
		auto& type_is = args.at(i)->type();
		auto& type_required = function.args.at(i)->type;
		if (!assignable(type_required, type_is)) {
			std::stringstream msg;
			msg << "Function '" << function.name << "' requires '" << type_required.name << "' for its " << iTh(i) << " argument, '";
			msg << type_is.name << "' given.";
			throw ParseError(msg.str());
		}
	}

	// get lhs
	std::vector<std::reference_wrapper<const VariableDeclaration>> lhs;
	if (context->lhs) {
		auto lhsraw = context->lhs->accept(this).as<std::vector<Expression*>>();
		args.reserve(lhsraw.size());
		for (Expression* expr : lhsraw) {
			lhs.push_back(expression2Variable(*expr));
		}
	}

	// check lhs number
	if (lhs.size() != 0 && function.return_type.size() != lhs.size()) {
		throw ParseError("Coult not unpack return values of function '" + function.name + "': number of values missmatchs.");
	}

	for (std::size_t i = 0; i < lhs.size(); ++i) {
		auto& type_is = function.return_type.at(i).get();
		auto& type_required = lhs.at(i).get().type;
		if (!assignable(type_required, type_is)) {
			std::stringstream msg;
			msg << "Function '" << function.name << "' returns '" << type_required.name << "' as its " << iTh(i) << " value, '";
			msg << "assigning to type " << type_is.name << "' is not possible.";
			throw ParseError(msg.str());
		}
	}

	auto macro = new Macro(function);
	macro->lhs = std::move(lhs);
	macro->args = std::move(args);
	return as_command(macro);
}

antlrcpp::Any AstBuilder::visitArgList(cola::CoLaParser::ArgListContext* context) {
	std::vector<Expression*> results;
	results.reserve(context->arg.size());
	for (auto context : context->arg) {
		results.push_back(context->accept(this).as<Expression*>());
	}
	return results;
}

antlrcpp::Any AstBuilder::visitCmdContinue(cola::CoLaParser::CmdContinueContext* /*context*/) {
	if (!_inside_loop) {
		throw ParseError("'continue' cannot appear outside loops.");
	}
	return as_command(new Continue());
}

antlrcpp::Any AstBuilder::visitCmdBreak(cola::CoLaParser::CmdBreakContext* /*context*/) {
	if (!_inside_loop) {
		throw ParseError("'break' cannot appear outside loops.");
	}
	return as_command(new Break());
}

static Statement* make_return(const Function& func, std::vector<std::unique_ptr<Expression>> expressions) {
	auto result = new Return();
	if (expressions.size() != func.return_type.size()) {
		throw ParseError("Number of returned values does not match return type of function '" + func.name + "'.");
	}
	for (std::size_t i = 0; i < expressions.size(); ++i) {
		auto& type_is = expressions.at(i)->type();
		auto& type_required = func.return_type.at(i).get();
		if (!assignable(type_required, type_is)) {
			std::stringstream msg;
			msg << "The " << AstBuilder::iTh(i) << " return value of '" << func.name << "' must be of type " << type_required.name << "', ";
			msg << "type " << type_is.name << "' is not compatible.";
			throw ParseError(msg.str());
		}
	}
	result->expressions = std::move(expressions);
	return result;
}

antlrcpp::Any AstBuilder::visitCmdReturnVoid(CoLaParser::CmdReturnVoidContext* /*context*/) {
	assert(_currentFunction);
	return make_return(*_currentFunction, {});
}

antlrcpp::Any AstBuilder::visitCmdReturnSingle(CoLaParser::CmdReturnSingleContext* context) {
	assert(_currentFunction);
	return make_return(*_currentFunction, mk_expr_from_list({ context->expr }));
}

antlrcpp::Any AstBuilder::visitCmdReturnList(CoLaParser::CmdReturnListContext* context) {
	assert(_currentFunction);
	return make_return(*_currentFunction, mk_expr_from_list(context->args->arg));
}


antlrcpp::Any AstBuilder::visitCmdCas(cola::CoLaParser::CmdCasContext* context) {
	return as_command(context->cas()->accept(this).as<CompareAndSwap*>());
}

static CompareAndSwap* make_cas(std::vector<std::unique_ptr<Expression>> dst, std::vector<std::unique_ptr<Expression>> cmp, std::vector<std::unique_ptr<Expression>> src) {
	if (dst.size() != cmp.size() || cmp.size() != src.size()) {
		throw ParseError("Malformed CAS, number of arguments is not balanced.");
	}

	auto result = new CompareAndSwap();
	for (std::size_t i = 0; i < dst.size(); ++i) {
		auto& typeDst = dst.at(i)->type();
		auto& typeCmp = cmp.at(i)->type();
		auto& typeScr = src.at(i)->type();
		if (!comparable(typeDst, typeCmp)) {
			throw ParseError("Type error: cannot compare types '" + typeDst.name + "' and " + typeCmp.name + "' in " + std::to_string(i) + "th argument of CAS.");
		}
		if (!assignable(typeDst, typeScr)) {
			throw ParseError("Type error: cannot assign to expression of type '" + typeDst.name + "' from expression of type '" + typeScr.name + "', in " + std::to_string(i) + "th argument of CAS.");
		}

		result->elems.push_back(CompareAndSwap::Triple(std::move(dst.at(i)), std::move(cmp.at(i)), std::move(src.at(i))));
	}
	return result;
}

antlrcpp::Any AstBuilder::visitCasSingle(CoLaParser::CasSingleContext* context) {
	return make_cas(mk_expr_from_list({context->dst}), mk_expr_from_list({context->cmp}), mk_expr_from_list({context->src}));
}

antlrcpp::Any AstBuilder::visitCasMultiple(CoLaParser::CasMultipleContext* context) {
	return make_cas(mk_expr_from_list(context->dst->arg), mk_expr_from_list(context->cmp->arg), mk_expr_from_list(context->src->arg));
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

antlrcpp::Any AstBuilder::visitOption(cola::CoLaParser::OptionContext* context) {
	std::string key = context->ident->getText();
	std::string val = context->str->getText();
	val.erase(val.begin());
	val.pop_back();

	if (key == "name") {
		_program->name = val;
	}

	auto insertion = _program->options.insert({ key, val });

	if (!insertion.second) {
		throw ParseError("Multiple options with the same key are not supported.");
	}

	return nullptr;
}

antlrcpp::Any AstBuilder::visitDefContains(CoLaParser::DefContainsContext* /*context*/) {
	throw std::logic_error("not yet implemented (AstBuilder::visitDefContains)");
}

antlrcpp::Any AstBuilder::visitDefInvariant(CoLaParser::DefInvariantContext* /*context*/) {
	throw std::logic_error("not yet implemented (AstBuilder::visitDefInvariant)");
}
