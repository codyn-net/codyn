#include "cpg-expression-private.h"
#include "cpg-link-private.h"
#include "cpg-utils.h"
#include "cpg-tokenizer.h"
#include "cpg-math.h"
#include "cpg-debug.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

static int parse_expression(CpgExpression *expression, char const **buffer, CpgContext *context, int priority, int left_assoc);

static CpgInstruction *
cpg_instruction_initialize(CpgInstruction *instruction)
{
	instruction->next = NULL;
	return instruction;
}

#define instruction_new(Type) ((Type *)cpg_instruction_initialize((CpgInstruction *)cpg_new1(Type)))

static char *
instruction_tos(CpgInstruction *inst)
{
	char *res = cpg_new(char, 1024);
	*res = '\0';
	
	switch (inst->type)
	{
		case CPG_INSTRUCTION_TYPE_FUNCTION:
		{
			CpgInstructionFunction *i = (CpgInstructionFunction *)inst;
			snprintf(res, 1024, "FUNC(%s, %d)", i->name, i->arguments);
		}
		break;
		case CPG_INSTRUCTION_TYPE_OPERATOR:
		{
			CpgInstructionFunction *i = (CpgInstructionFunction *)inst;
			snprintf(res, 1024, "OP(%s, %d)", i->name, i->arguments);
		}
		break;
		case CPG_INSTRUCTION_TYPE_PROPERTY:
		{
			CpgInstructionProperty *i = (CpgInstructionProperty *)inst;
			snprintf(res, 1024, "PROP(%s)", i->property->name);
		}
		break;
		case CPG_INSTRUCTION_TYPE_NUMBER:
		{
			CpgInstructionNumber *i = (CpgInstructionNumber *)inst;
			snprintf(res, 1024, "NUM(%f)", i->value);
		}
		break;
		case CPG_INSTRUCTION_TYPE_NONE:
		break;
	}
	
	return res;
}

static CpgInstruction *
cpg_instruction_function_new(unsigned id, char const *name, int arguments)
{
	CpgInstructionFunction *res = instruction_new(CpgInstructionFunction);
	res->parent.type = CPG_INSTRUCTION_TYPE_FUNCTION;
	res->id = id;
	res->name = cpg_strdup(name);
	res->arguments = arguments;
	
	return (CpgInstruction *)res;
}

static CpgInstruction *
cpg_instruction_number_new(double value)
{
	CpgInstructionNumber *res = instruction_new(CpgInstructionNumber);
	res->parent.type = CPG_INSTRUCTION_TYPE_NUMBER;
	res->value = value;
	
	return (CpgInstruction *)res;
}

static CpgInstruction *
cpg_instruction_operator_new(unsigned id, char const *name, int arguments)
{
	CpgInstruction *res = cpg_instruction_function_new(id, name, arguments);
	res->type = CPG_INSTRUCTION_TYPE_OPERATOR;
	
	return res;
}

static CpgInstruction *
cpg_instruction_property_new(CpgProperty *property)
{
	CpgInstructionProperty *res = instruction_new(CpgInstructionProperty);
	res->parent.type = CPG_INSTRUCTION_TYPE_PROPERTY;
	
	res->property = property;
	
	return (CpgInstruction *)res;
}

static void
cpg_instruction_free(CpgInstruction *instruction)
{
	if (instruction->type == CPG_INSTRUCTION_TYPE_FUNCTION ||
		instruction->type == CPG_INSTRUCTION_TYPE_OPERATOR)
		cpg_free(((CpgInstructionFunction *)instruction)->name);

	cpg_free(instruction);
}

static void
instructions_free(CpgExpression *expression)
{
	CpgInstruction *inst = expression->instructions;
	
	while (inst)
	{
		CpgInstruction *next = inst->next;
		cpg_instruction_free(inst);
		inst = next;
	}
	
	expression->instructions = NULL;

	// reset cached and instant flags
	expression->instant = 0;
	expression->has_cache = 0;
}

char const *
cpg_expression_get(CpgExpression *expression)
{
	return expression->expression;
}

void
cpg_expression_set(CpgExpression *expression, char const *value)
{
	if (expression->expression)
		cpg_free(expression->expression);
	
	expression->expression = cpg_strdup(value);
	instructions_free(expression);
	
	cpg_stack_destroy(&(expression->output));
}

CpgExpression *
cpg_expression_new(char const *expression)
{
	CpgExpression *res = cpg_new1(CpgExpression);

	res->expression = cpg_strdup(expression);
	res->instructions = NULL;
	
	cpg_stack_init(&(res->output), 0);

	res->has_cache = 0;
	res->instant = 0;
	res->mutex = NULL;

	return res;
}

CpgInstruction *
cpg_instruction_copy(CpgInstruction *instruction)
{
	switch (instruction->type)
	{
		case CPG_INSTRUCTION_TYPE_NUMBER:
			return cpg_instruction_number_new(((CpgInstructionNumber *)instruction)->value);
		break;
		case CPG_INSTRUCTION_TYPE_PROPERTY:
			return cpg_instruction_property_new(((CpgInstructionProperty *)instruction)->property);
		break;
		case CPG_INSTRUCTION_TYPE_OPERATOR:
		{
			CpgInstructionFunction *inst = (CpgInstructionFunction *)instruction;
			return cpg_instruction_operator_new(inst->id, inst->name, inst->arguments);
		}
		break;
		case CPG_INSTRUCTION_TYPE_FUNCTION:
		{
			CpgInstructionFunction *inst = (CpgInstructionFunction *)instruction;
			return cpg_instruction_function_new(inst->id, inst->name, inst->arguments);
		}
		break;
		default:
		break;
	}
	
	return NULL;
}

CpgExpression *
cpg_expression_copy(CpgExpression *expression)
{
	CpgExpression *res = cpg_new1(CpgExpression);
	
	res->expression = cpg_strdup(expression->expression);
	cpg_stack_init(&(res->output), expression->output.size);

	res->has_cache = expression->has_cache;
	res->instant = expression->instant;
	res->cached_output = expression->cached_output;
	res->mutex = NULL;
	
	CpgInstruction *inst;
	CpgInstruction *prev = NULL;
	res->instructions = NULL;
	
	for (inst = expression->instructions; inst; inst = inst->next)
	{
		CpgInstruction *copy = cpg_instruction_copy(inst);
		
		if (prev)
			prev->next = copy;
		else
			res->instructions = copy;
		
		prev = copy;
		copy->next = NULL;
	}
	
	return res;
}

static void
instructions_push(CpgExpression *expression, CpgInstruction *next)
{
	next->next = expression->instructions;
	expression->instructions = next;
	
	char *res = instruction_tos(next);
	cpg_debug_expression("Pushed expression: %s", res);
	cpg_free(res);
}

static int
parse_function(CpgExpression *expression, char const *name, char const **buffer, CpgContext *context)
{
	// do function lookup
	int arguments;
	unsigned id = cpg_math_function_lookup(name, &arguments);
	
	if (!id)
		return 0;
	
	// parse arguments
	int numargs = 0;
	
	while (1)
	{
		if (!parse_expression(expression, buffer, context, -1, 0))
			return 0;
		
		++numargs;

		// see what's next
		CpgToken *next = cpg_tokenizer_peek(*buffer);
		
		if (!next || !CPG_TOKEN_IS_OPERATOR(next))
			return 0;

		CpgTokenOperatorType type = CPG_TOKEN_OPERATOR(next)->type;
		cpg_token_free(next);

		if (type == CPG_TOKEN_OPERATOR_TYPE_GROUP_END)
		{
			cpg_token_free(cpg_tokenizer_next(buffer));
			break;
		}
		else if (type != CPG_TOKEN_OPERATOR_TYPE_COMMA)
			return 0;
		
		cpg_token_free(cpg_tokenizer_next(buffer));
	}
	
	if (arguments != -1 && numargs > arguments)
	{
		// wrong number of arguments
		return 0;
	}
	
	if (arguments == -1)
		instructions_push(expression, cpg_instruction_number_new((double)numargs));

	cpg_debug_expression("After func: %s", *buffer);
	instructions_push(expression, cpg_instruction_function_new(id, name, numargs));
	return 1;
}

static int
parse_ternary_operator(CpgExpression *expression, CpgTokenOperator *token, char const **buffer, CpgContext *context)
{
	if (!parse_expression(expression, buffer, context, token->priority, token->left_assoc))
		return 0;
	
	// next token should be :
	CpgToken *next = cpg_tokenizer_peek(*buffer);
	
	if (!next)
		return 0;
	
	int istern = CPG_TOKEN_IS_OPERATOR(next) && CPG_TOKEN_OPERATOR(next)->type == CPG_TOKEN_OPERATOR_TYPE_TERNARY_FALSE;
	
	if (!istern)
	{
		cpg_token_free(next);
		return 0;
	}

	cpg_token_free(cpg_tokenizer_next(buffer));
	CpgTokenOperator *op = CPG_TOKEN_OPERATOR(next);
	
	// do next expression
	if (!parse_expression(expression, buffer, context, op->priority, op->left_assoc))
	{
		cpg_token_free(next);
		return 0;
	}
	
	instructions_push(expression, cpg_instruction_operator_new(cpg_math_operator_lookup(CPG_MATH_OPERATOR_TYPE_TERNARY), "?:", 3));
	return 1;
}

static int
parse_group(CpgExpression *expression, char const **buffer, CpgContext *context)
{
	if (!parse_expression(expression, buffer, context, -1, 0))
		return 0;
	
	CpgToken *next = cpg_tokenizer_peek(*buffer);
	int groupend = next && (CPG_TOKEN_IS_OPERATOR(next) ||
				   CPG_TOKEN_OPERATOR(next)->type != CPG_TOKEN_OPERATOR_TYPE_GROUP_END);

	cpg_token_free(next);

	if (!groupend)
		return 0;
	
	cpg_token_free(cpg_tokenizer_next(buffer));

	return 1;
}

static int
parse_unary_operator(CpgExpression *expression, CpgToken *token, char const **buffer, CpgContext *context)
{
	CpgTokenOperator *op = CPG_TOKEN_OPERATOR(token);
	int ret = 1;
	CpgInstruction *inst = NULL;

	// handle group
	if (op->type == CPG_TOKEN_OPERATOR_TYPE_GROUP_START)
	{
		cpg_token_free(cpg_tokenizer_next(buffer));
		return parse_group(expression, buffer, context);
	}

	switch (op->type)
	{
		case CPG_TOKEN_OPERATOR_TYPE_MINUS:
			inst = cpg_instruction_operator_new(cpg_math_operator_lookup(CPG_MATH_OPERATOR_TYPE_UNARY_MINUS), "-", 1);
		break;
		case CPG_TOKEN_OPERATOR_TYPE_PLUS:
		break;
		case CPG_TOKEN_OPERATOR_TYPE_NEGATE:
			inst = cpg_instruction_operator_new(cpg_math_operator_lookup(CPG_MATH_OPERATOR_TYPE_NEGATE), "!", 1);
		break;
		default:
			ret = 0;
		break;
	}
	
	if (ret)
	{
		// consume token
		cpg_token_free(cpg_tokenizer_next(buffer));
		ret = parse_expression(expression, buffer, context, 1000, 1);
	}
	
	if (ret && inst)
		instructions_push(expression, inst);
	
	return ret;
}

static int
parse_operator(CpgExpression *expression, CpgToken *token, char const **buffer, CpgContext *context)
{
	CpgTokenOperator *op = CPG_TOKEN_OPERATOR(token);
	
	// handle ternary
	if (op->type == CPG_TOKEN_OPERATOR_TYPE_TERNARY_TRUE)
	{
		// consume token
		cpg_token_free(cpg_tokenizer_next(buffer));

		return parse_ternary_operator(expression, CPG_TOKEN_OPERATOR(token), buffer, context);
	}

	CpgInstruction *inst = NULL;

	switch (op->type)
	{
		// arithmetic
		case CPG_TOKEN_OPERATOR_TYPE_MULTIPLY:
			inst = cpg_instruction_operator_new(cpg_math_operator_lookup(CPG_MATH_OPERATOR_TYPE_MULTIPLY), "*", 2);
		break;
		case CPG_TOKEN_OPERATOR_TYPE_DIVIDE:
			inst = cpg_instruction_operator_new(cpg_math_operator_lookup(CPG_MATH_OPERATOR_TYPE_DIVIDE), "/", 2);
		break;
		case CPG_TOKEN_OPERATOR_TYPE_MODULO:
			inst = cpg_instruction_operator_new(cpg_math_operator_lookup(CPG_MATH_OPERATOR_TYPE_MODULO), "%", 2);
		break;
		case CPG_TOKEN_OPERATOR_TYPE_PLUS:
			inst = cpg_instruction_operator_new(cpg_math_operator_lookup(CPG_MATH_OPERATOR_TYPE_PLUS), "+", 2);
		break;
		case CPG_TOKEN_OPERATOR_TYPE_MINUS:
			inst = cpg_instruction_operator_new(cpg_math_operator_lookup(CPG_MATH_OPERATOR_TYPE_MINUS), "-", 2);
		break;
		case CPG_TOKEN_OPERATOR_TYPE_POWER:
			inst = cpg_instruction_operator_new(cpg_math_operator_lookup(CPG_MATH_OPERATOR_TYPE_POWER), "**", 2);
		break;
		
		// logical
		case CPG_TOKEN_OPERATOR_TYPE_GREATER:
			inst = cpg_instruction_operator_new(cpg_math_operator_lookup(CPG_MATH_OPERATOR_TYPE_GREATER), ">", 2);
		break;
		case CPG_TOKEN_OPERATOR_TYPE_LESS:
			inst = cpg_instruction_operator_new(cpg_math_operator_lookup(CPG_MATH_OPERATOR_TYPE_LESS), "<", 2);
		break;
		case CPG_TOKEN_OPERATOR_TYPE_GREATER_OR_EQUAL:
			inst = cpg_instruction_operator_new(cpg_math_operator_lookup(CPG_MATH_OPERATOR_TYPE_GREATER_OR_EQUAL), ">=", 2);
		break;
		case CPG_TOKEN_OPERATOR_TYPE_LESS_OR_EQUAL:
			inst = cpg_instruction_operator_new(cpg_math_operator_lookup(CPG_MATH_OPERATOR_TYPE_LESS_OR_EQUAL), "<=", 2);
		break;
		case CPG_TOKEN_OPERATOR_TYPE_EQUAL:
			inst = cpg_instruction_operator_new(cpg_math_operator_lookup(CPG_MATH_OPERATOR_TYPE_EQUAL), "==", 2);
		break;
		case CPG_TOKEN_OPERATOR_TYPE_OR:
			inst = cpg_instruction_operator_new(cpg_math_operator_lookup(CPG_MATH_OPERATOR_TYPE_OR), "||", 2);
		break;
		case CPG_TOKEN_OPERATOR_TYPE_AND:
			inst = cpg_instruction_operator_new(cpg_math_operator_lookup(CPG_MATH_OPERATOR_TYPE_AND), "&&", 2);
		break;
		default:
			return 0;
		break;
	}
	
	// parse second part of binary operator
	if (!inst)
		return 0;
		
	// consume token
	cpg_token_free(cpg_tokenizer_next(buffer));

	if (!parse_expression(expression, buffer, context, op->priority, op->left_assoc))
		return 0;
	
	instructions_push(expression, inst);
	
	return 1;
}

static int
parse_property(CpgExpression *expression, char *propname, CpgContext *context)
{
	CpgProperty *property = NULL;
	
	cpg_debug_expression("Parsing property: %s", propname);
	
	// iterate over contexts
	while (context && !property)
	{
		if (context->object)
			property = cpg_object_property(context->object, propname);
		
		context = context->next;
	}
	
	if (!property)
	{
		cpg_debug_expression("Property %s not found", propname);
		return 0;
	}
	
	instructions_push(expression, cpg_instruction_property_new(property));
	return 1;
}

static CpgLink *
find_link(CpgContext *context)
{
	while (context)
	{
		if (CPG_OBJECT_IS_LINK(context->object))
			return (CpgLink *)context->object;
		
		context = context->next;
	}
	
	return NULL;
}

static int
parse_link_property(CpgExpression *expression, char *id, char *propname, CpgLink *link)
{
	CpgProperty *property = NULL;

	if (strcmp(id, "from") == 0)
	{
		property = cpg_object_property(link->from, propname);
	}
	else if (strcmp(id, "to") == 0)
	{
		property = cpg_object_property(link->to, propname);
	}
	
	if (!property)
		return 0;
	
	instructions_push(expression, cpg_instruction_property_new(property));
	return 1;
}

static int
parse_constant(CpgExpression *expression, char const *name)
{
	int found = 0;
	double val = cpg_math_constant_lookup(name, &found);
	
	if (!found)
		return 0;

	instructions_push(expression, cpg_instruction_number_new(val));
	return 1;
}

static int
parse_number(CpgExpression *expression, CpgTokenNumber *token)
{
	instructions_push(expression, cpg_instruction_number_new(token->value));
	return 1;
}

static int
parse_identifier(CpgExpression *expression, CpgTokenIdentifier *token, char const **buffer, CpgContext *context)
{
	char *id = token->identifier;
	int ret = 0;
	
	// consume token and peek the next to see if the identifier is a function
	// call
	cpg_token_free(cpg_tokenizer_next(buffer));
	CpgToken *next = cpg_tokenizer_peek(*buffer);
	CpgLink *link;

	if (next && CPG_TOKEN_IS_OPERATOR(next) && 
		CPG_TOKEN_OPERATOR(next)->type == CPG_TOKEN_OPERATOR_TYPE_GROUP_START)
	{
		// consume peeked group start
		cpg_token_free(cpg_tokenizer_next(buffer));
		ret = parse_function(expression, id, buffer, context);
	}
	else if (next && CPG_TOKEN_IS_OPERATOR(next) &&
			 CPG_TOKEN_OPERATOR(next)->type == CPG_TOKEN_OPERATOR_TYPE_DOT && (link = find_link(context)))
	{
		// consume peeked dot
		cpg_token_free(cpg_tokenizer_next(buffer));
		CpgToken *propname = cpg_tokenizer_next(buffer);
	
		if (CPG_TOKEN_IS_IDENTIFIER(propname))
			ret = parse_link_property(expression, id, CPG_TOKEN_IDENTIFIER(propname)->identifier, link);
	
		cpg_token_free(propname);
	}
	else
	{
		// try to parse property
		ret = parse_property(expression, id, context);
	
		if (!ret)
		{	
			// try parsing constants
			ret = parse_constant(expression, id);
		}
	}

	cpg_token_free(next);
	return ret;
}

static int
parse_expression(CpgExpression *expression, char const **buffer, CpgContext *context, int priority, int left_assoc)
{
	static int depth = 0;
	
	// peek next token
	CpgToken *token;
	int ret = 0;
	int num = 0;
	
	cpg_debug_expression("Parse begin (%d): %s", ++depth, *buffer);
	
	while ((token = cpg_tokenizer_peek(*buffer)))
	{
		if (!token)
			break;
		
		ret = 1;
		
		cpg_debug_expression("Parsing next: (%d) %d, %s", depth, num, *buffer);
	
		switch (token->type)
		{
			case CPG_TOKEN_TYPE_NUMBER:
				// simply push a number on the stack
				ret = parse_number(expression, CPG_TOKEN_NUMBER(token));
			break;
			case CPG_TOKEN_TYPE_IDENTIFIER:
			{
				ret = parse_identifier(expression, CPG_TOKEN_IDENTIFIER(token), buffer, context);
				cpg_token_free(token);
				token = NULL;
			}
			break;
			case CPG_TOKEN_TYPE_OPERATOR:
			{
				CpgTokenOperator *op = CPG_TOKEN_OPERATOR(token);
				
				// group end
				if (op->type == CPG_TOKEN_OPERATOR_TYPE_GROUP_END || op->type == CPG_TOKEN_OPERATOR_TYPE_COMMA || op->type == CPG_TOKEN_OPERATOR_TYPE_TERNARY_FALSE)
				{
					cpg_token_free(token);
					cpg_debug_expression("Parse end group (%d): %s", depth--, *buffer);
					return 1;
				}
				
				if (num == 0)
				{
					ret = parse_unary_operator(expression, token, buffer, context);
					
					if (ret)
					{
						cpg_token_free(token);
						token = NULL;
					}
				}
				else if (op->priority < priority ||
						(op->priority == priority && left_assoc))
				{
					// Do not handle the operator here yet
					cpg_token_free(token);
					cpg_debug_expression("Parse end op (%d): %s", depth--, *buffer);
					return 1;
				}
				else
				{
					ret = parse_operator(expression, token, buffer, context);
					
					if (ret)
					{
						cpg_token_free(token);
						token = NULL;
					}
				}
			}
			break;
			case CPG_TOKEN_TYPE_NONE:
				ret = 0;
			break;
		}
	
		if (token)
		{
			// consume token
			if (ret)
				cpg_token_free(cpg_tokenizer_next(buffer));

			cpg_token_free(token);
		}
		
		++num;
		
		if (ret == 0)
			break;
	}
	
	cpg_debug_expression("Parse end (%d): %s", depth--, *buffer);
	
	return ret;
}

static void
instructions_reverse(CpgExpression *expression)
{
	CpgInstruction *rev = NULL;
	CpgInstruction *last = expression->instructions;
	
	while (last)
	{
		CpgInstruction *tmp = last->next;
		last->next = rev;
		
		rev = last;
		last = tmp;
	}
	
	expression->instructions = rev;
}

static int
validate_stack(CpgExpression *expression)
{
	CpgInstruction *inst;
	int stack = 0;
	int maxstack = 1;
	
	// check for empty instruction set
	if (!expression->instructions)
		instructions_push(expression, cpg_instruction_number_new(0.0));
	
	for (inst = expression->instructions; inst; inst = inst->next)
	{
		switch (inst->type)
		{
			case CPG_INSTRUCTION_TYPE_OPERATOR:
			case CPG_INSTRUCTION_TYPE_FUNCTION:
			{
				CpgInstructionFunction *i = (CpgInstructionFunction *)inst;
				stack -= i->arguments;
				
				if (stack < 0)
					return 0;
				
				// TODO: add number of return values to function instruction
				++stack;
			}
			break;
			case CPG_INSTRUCTION_TYPE_PROPERTY:
				++stack;
			break;
			case CPG_INSTRUCTION_TYPE_NUMBER:
				// increase stack here
				++stack;
			break;
			case CPG_INSTRUCTION_TYPE_NONE:
			break;
		}
		
		if (stack > maxstack)
			maxstack = stack;
	}
	
	if (stack != 1)
		return 0;
	
	cpg_stack_destroy(&(expression->output));
	cpg_stack_init(&(expression->output), maxstack);
	
	return 1;
}

int
cpg_expression_compile(CpgExpression *expression, CpgContext *context, char **error)
{
	char *buffer = expression->expression;
	
	instructions_free(expression);
	
	cpg_stack_destroy(&(expression->output));
	expression->has_cache = 0;
	
	int ret = parse_expression(expression, (char const **)&buffer, context, -1, 0);
	
	if (!ret)
	{
		instructions_free(expression);

		// could not parse full thing
		if (error)
		{
			char msg[1024];
			snprintf(msg, 1024, "Invalid token at: %s", buffer ? buffer : "");
			
			*error = cpg_strdup(msg);
		}
		
		return 0;
	}
	else
	{
		// reverse instructions
		instructions_reverse(expression);

		if (!validate_stack(expression))
		{
			if (error)
			{
				char msg[4096];
				CpgInstruction *inst;
				char *ptr;
				
				snprintf(msg, 4096, "Invalid stack produced: \n\t%s\n\t", expression->expression);
				ptr = msg + strlen(msg);

				for (inst = expression->instructions; inst; inst = inst->next)
				{
					char *res = instruction_tos(inst);
					snprintf(ptr, msg + 4096 - ptr, "%s ", res);
					ptr += strlen(res) + 1;
					free(res);
				}
				
				*error = cpg_strdup(msg);
				instructions_free(expression);
			}
			
			return 0;
		}
	}
	
	return 1;
}

inline void
cpg_expression_set_value(CpgExpression *expression, double value)
{
	instructions_free(expression);
	
	expression->instructions = cpg_instruction_number_new(value);

	// this can be instant and lets also set the cache already
	expression->instant = 1;
	expression->has_cache = 1;

	expression->cached_output = value;
}

double
cpg_expression_evaluate(CpgExpression *expression)
{
	if (!expression)
		return 0.0;
		
	if (expression->has_cache)
		return expression->cached_output;
	
	// make sure to lock the mutex because there is only one output stack
	cpg_mutex_lock(expression->mutex);

	CpgInstruction *instruction;
	cpg_stack_reset(&(expression->output));
	
	if (expression->output.size == 0)
	{
		cpg_debug_error("Stack size should not be 0!");
		return 0.0;
	}
	
	if (!expression->instructions)
	{
		fprintf(stderr, "No instructions found, maybe the expression was not parsed?");
		cpg_mutex_unlock(expression->mutex);
		return 0.0;
	}
	
	for (instruction = expression->instructions; instruction; instruction = instruction->next)
	{
		switch (instruction->type)
		{
			case CPG_INSTRUCTION_TYPE_NUMBER:
				cpg_stack_push(&(expression->output), ((CpgInstructionNumber *)instruction)->value, NULL);
			break;
			case CPG_INSTRUCTION_TYPE_PROPERTY:
			{
				CpgInstructionProperty *property = (CpgInstructionProperty *)instruction;
				cpg_stack_push(&(expression->output), cpg_expression_evaluate(property->property->value), NULL);
			}
			break;
			case CPG_INSTRUCTION_TYPE_FUNCTION:
				cpg_math_function_execute(((CpgInstructionFunction *)instruction)->id, &(expression->output), NULL);
			break;
			case CPG_INSTRUCTION_TYPE_OPERATOR:
				cpg_math_operator_execute(((CpgInstructionFunction *)instruction)->id, &(expression->output), NULL);
			break;
			default:
			break;
		}
	}
	
	if (cpg_stack_count(&(expression->output)) != 1)
	{
		fprintf(stderr, "Invalid output stack after evaluating: %s!\n", expression->expression);

		cpg_mutex_unlock(expression->mutex);
		return NAN;
	}
	
	expression->has_cache = 1;
	expression->cached_output = cpg_stack_pop(&(expression->output), NULL);
	
	cpg_mutex_unlock(expression->mutex);
	return expression->cached_output;
}

void
cpg_expression_free(CpgExpression *expression)
{
	if (!expression)
		return;

	instructions_free(expression);

	cpg_stack_destroy(&(expression->output));
		
	if (expression->expression)
		free(expression->expression);

	free(expression);
}

void
cpg_expression_print_instructions(CpgExpression *expression, FILE *f)
{
	CpgInstruction *inst;
	
	for (inst = expression->instructions; inst; inst = inst->next)
	{
		char *res = instruction_tos(inst);
		
		fprintf(f, "%s ", res);
		free(res);
	}
	
	fprintf(f, "\n");;
}

inline void
cpg_expression_reset_cache(CpgExpression *expression)
{
	if (!expression->instant)
		expression->has_cache = 0;
}
