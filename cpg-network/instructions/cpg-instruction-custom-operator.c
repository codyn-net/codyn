#include "cpg-instruction-custom-operator.h"
#include <cpg-network/cpg-expression.h>

#define CPG_INSTRUCTION_CUSTOM_OPERATOR_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), CPG_TYPE_INSTRUCTION_CUSTOM_OPERATOR, CpgInstructionCustomOperatorPrivate))

struct _CpgInstructionCustomOperatorPrivate
{
	CpgOperator *op;
	CpgOperatorData *data;
};

G_DEFINE_TYPE (CpgInstructionCustomOperator, cpg_instruction_custom_operator, CPG_TYPE_INSTRUCTION)

static void
cpg_instruction_custom_operator_finalize (CpgMiniObject *object)
{
	CpgInstructionCustomOperator *op;

	op = CPG_INSTRUCTION_CUSTOM_OPERATOR (object);

	cpg_operator_free_data (op->priv->op, op->priv->data);
	g_object_unref (op->priv->op);

	CPG_MINI_OBJECT_CLASS (cpg_instruction_custom_operator_parent_class)->finalize (object);
}

static CpgMiniObject *
cpg_instruction_custom_operator_copy (CpgMiniObject const *object)
{
	CpgMiniObject *ret;
	CpgInstructionCustomOperator *op;
	CpgInstructionCustomOperator const *src;

	ret = CPG_MINI_OBJECT_CLASS (cpg_instruction_custom_operator_parent_class)->copy (object);

	src = CPG_INSTRUCTION_CUSTOM_OPERATOR_CONST (object);
	op = CPG_INSTRUCTION_CUSTOM_OPERATOR (ret);

	op->priv->op = g_object_ref (src->priv->op);
	op->priv->data = cpg_operator_create_data (op->priv->op,
	                                           src->priv->data->expressions);

	return ret;
}

static gchar *
cpg_instruction_custom_operator_to_string (CpgInstruction *instruction)
{
	CpgInstructionCustomOperator *self;

	self = CPG_INSTRUCTION_CUSTOM_OPERATOR (instruction);

	gchar *name = cpg_operator_get_name (self->priv->op);
	gchar *ret = g_strdup_printf ("OPC (%s)", name);
	g_free (name);

	return ret;
}

static void
cpg_instruction_custom_operator_execute (CpgInstruction *instruction,
                                         CpgStack       *stack)
{
	CpgInstructionCustomOperator *self;

	/* Direct cast to reduce overhead of GType cast */
	self = (CpgInstructionCustomOperator *)instruction;
	cpg_operator_execute (self->priv->op, self->priv->data, stack);
}

static gint
cpg_instruction_custom_operator_get_stack_count (CpgInstruction *instruction)
{
	return 1;
}

static GSList *
cpg_instruction_custom_operator_get_dependencies (CpgInstruction *instruction)
{
	CpgInstructionCustomOperator *self;

	self = CPG_INSTRUCTION_CUSTOM_OPERATOR (instruction);

	GSList const *expressions;
	expressions = cpg_operator_get_expressions (self->priv->op, self->priv->data);

	GSList *dependencies = NULL;

	while (expressions)
	{
		CpgExpression *expr = expressions->data;
		GSList *ret;

		ret = g_slist_copy ((GSList *)cpg_expression_get_dependencies (expr));
		dependencies = g_slist_concat (dependencies, ret);

		expressions = g_slist_next (expressions);
	}

	return dependencies;
}

static void
cpg_instruction_custom_operator_class_init (CpgInstructionCustomOperatorClass *klass)
{
	CpgMiniObjectClass *object_class = CPG_MINI_OBJECT_CLASS (klass);
	CpgInstructionClass *inst_class = CPG_INSTRUCTION_CLASS (klass);

	object_class->finalize = cpg_instruction_custom_operator_finalize;
	object_class->copy = cpg_instruction_custom_operator_copy;

	inst_class->to_string = cpg_instruction_custom_operator_to_string;
	inst_class->execute = cpg_instruction_custom_operator_execute;
	inst_class->get_stack_count = cpg_instruction_custom_operator_get_stack_count;
	inst_class->get_dependencies = cpg_instruction_custom_operator_get_dependencies;

	g_type_class_add_private (object_class, sizeof(CpgInstructionCustomOperatorPrivate));
}

static void
cpg_instruction_custom_operator_init (CpgInstructionCustomOperator *self)
{
	self->priv = CPG_INSTRUCTION_CUSTOM_OPERATOR_GET_PRIVATE (self);
}

CpgInstruction *
cpg_instruction_custom_operator_new (CpgOperator  *operator,
                                     GSList const *expressions)
{
	CpgMiniObject *ret;
	CpgInstructionCustomOperator *op;

	ret = cpg_mini_object_new (CPG_TYPE_INSTRUCTION_CUSTOM_OPERATOR);
	op = CPG_INSTRUCTION_CUSTOM_OPERATOR (ret);

	op->priv->op = g_object_ref (operator);
	op->priv->data = cpg_operator_create_data (op->priv->op,
	                                           expressions);

	return CPG_INSTRUCTION (ret);
}

CpgOperator *
cpg_instruction_custom_operator_get_operator (CpgInstructionCustomOperator *op)
{
	g_return_val_if_fail (CPG_IS_INSTRUCTION_CUSTOM_OPERATOR (op), NULL);

	return op->priv->op;
}

CpgOperatorData *
cpg_instruction_custom_operator_get_data (CpgInstructionCustomOperator *op)
{
	g_return_val_if_fail (CPG_IS_INSTRUCTION_CUSTOM_OPERATOR (op), NULL);

	return op->priv->data;
}
