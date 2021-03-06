#include "cdn-instruction-custom-function.h"

/**
 * CdnInstructionCustomFunction:
 *
 * Custom function call instruction.
 *
 * The #CdnInstructionCustomFunction is a special #CdnInstruction subtype
 * which represents a custom function call.
 *
 */

G_DEFINE_TYPE (CdnInstructionCustomFunction, cdn_instruction_custom_function, CDN_TYPE_INSTRUCTION)

#define CDN_INSTRUCTION_CUSTOM_FUNCTION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), CDN_TYPE_INSTRUCTION_CUSTOM_FUNCTION, CdnInstructionCustomFunctionPrivate))

struct _CdnInstructionCustomFunctionPrivate
{
	CdnFunction *function;
};

static void
cdn_instruction_custom_function_finalize (CdnMiniObject *object)
{
	CDN_MINI_OBJECT_CLASS (cdn_instruction_custom_function_parent_class)->finalize (object);
}

static CdnMiniObject *
cdn_instruction_custom_function_copy (CdnMiniObject *object)
{
	CdnMiniObject *ret;
	CdnInstructionCustomFunction *func;
	CdnInstructionCustomFunction const *src;

	ret = CDN_MINI_OBJECT_CLASS (cdn_instruction_custom_function_parent_class)->copy (object);

	src = CDN_INSTRUCTION_CUSTOM_FUNCTION_CONST (object);
	func = CDN_INSTRUCTION_CUSTOM_FUNCTION (ret);

	func->priv->function = g_object_ref (src->priv->function);

	return ret;
}

static gchar *
cdn_instruction_custom_function_to_string (CdnInstruction *instruction)
{
	CdnInstructionCustomFunction *self;

	self = CDN_INSTRUCTION_CUSTOM_FUNCTION (instruction);

	return g_strdup_printf ("FNC (%s)", cdn_object_get_id (CDN_OBJECT (self->priv->function)));
}

static void
cdn_instruction_custom_function_execute (CdnInstruction *instruction,
                                         CdnStack       *stack)
{
	CdnInstructionCustomFunction *self;

	/* Direct cast to reduce overhead of GType cast */
	self = (CdnInstructionCustomFunction *)instruction;

	cdn_function_execute (self->priv->function,
	                      stack);
}

static CdnStackManipulation const *
cdn_instruction_custom_function_get_stack_manipulation (CdnInstruction  *instruction,
                                                        GError         **error)
{
	CdnInstructionCustomFunction *self;

	self = CDN_INSTRUCTION_CUSTOM_FUNCTION (instruction);

	if (self->priv->function)
	{
		return cdn_function_get_stack_manipulation (self->priv->function);
	}

	return NULL;
}

static gboolean
cdn_instruction_custom_function_equal (CdnInstruction *i1,
                                       CdnInstruction *i2,
                                       gboolean        asstring)
{
	CdnInstructionCustomFunction *f1 = CDN_INSTRUCTION_CUSTOM_FUNCTION (i1);
	CdnInstructionCustomFunction *f2 = CDN_INSTRUCTION_CUSTOM_FUNCTION (i2);

	if (asstring)
	{
		return g_strcmp0 (cdn_object_get_id (CDN_OBJECT (f1->priv->function)),
		                  cdn_object_get_id (CDN_OBJECT (f2->priv->function))) == 0;
	}
	else
	{
		return f1->priv->function == f2->priv->function;
	}
}

static GSList *
cdn_instruction_custom_function_get_dependencies (CdnInstruction *inst)
{
	CdnInstructionCustomFunction *f = CDN_INSTRUCTION_CUSTOM_FUNCTION (inst);

	if (!f->priv->function)
	{
		return NULL;
	}

	return g_slist_copy ((GSList *)cdn_function_get_dependencies (f->priv->function));
}

static void
cdn_instruction_custom_function_class_init (CdnInstructionCustomFunctionClass *klass)
{
	CdnMiniObjectClass *object_class = CDN_MINI_OBJECT_CLASS (klass);
	CdnInstructionClass *inst_class = CDN_INSTRUCTION_CLASS (klass);

	object_class->copy = cdn_instruction_custom_function_copy;
	object_class->finalize = cdn_instruction_custom_function_finalize;

	inst_class->to_string = cdn_instruction_custom_function_to_string;
	inst_class->execute = cdn_instruction_custom_function_execute;
	inst_class->get_stack_manipulation = cdn_instruction_custom_function_get_stack_manipulation;
	inst_class->equal = cdn_instruction_custom_function_equal;
	inst_class->get_dependencies = cdn_instruction_custom_function_get_dependencies;

	g_type_class_add_private (object_class, sizeof(CdnInstructionCustomFunctionPrivate));
}

static void
cdn_instruction_custom_function_init (CdnInstructionCustomFunction *self)
{
	self->priv = CDN_INSTRUCTION_CUSTOM_FUNCTION_GET_PRIVATE (self);
}

static void
set_function (CdnInstructionCustomFunction *function,
              CdnFunction                  *func)
{
	g_return_if_fail (CDN_IS_INSTRUCTION_CUSTOM_FUNCTION (function));
	g_return_if_fail (function == NULL || CDN_IS_FUNCTION (func));

	if (function->priv->function)
	{
		g_object_unref (function->priv->function);
		function->priv->function = NULL;
	}

	if (func)
	{
		function->priv->function = g_object_ref (func);
	}
}

/**
 * cdn_instruction_custom_function_new:
 * @function: a #CdnFunction
 * @argdim: the args
 *
 * Create a new custom function instruction representing a call
 * to a custom function. @argdim indicates the dimensionality of the
 * arguments passed on the stack when this instruction should call
 * the corresponding function.
 *
 * Returns: (transfer full) (type CdnInstructionCustomFunction): a new #CdnInstructionCustomFunction
 *
 */
CdnInstruction *
cdn_instruction_custom_function_new (CdnFunction        *function,
                                     CdnStackArgs const *argdim)
{
	CdnInstructionCustomFunction *custom;

	custom = CDN_INSTRUCTION_CUSTOM_FUNCTION (
		cdn_mini_object_new (CDN_TYPE_INSTRUCTION_CUSTOM_FUNCTION));

	set_function (custom, function);

	return CDN_INSTRUCTION (custom);
}

/**
 * cdn_instruction_custom_function_get_function:
 * @function: A #CdnInstructionCustomFunction
 *
 * Get the function executed by the instruction.
 *
 * Returns: (transfer none): A #CdnFunction
 *
 **/
CdnFunction *
cdn_instruction_custom_function_get_function (CdnInstructionCustomFunction *function)
{
	g_return_val_if_fail (CDN_IS_INSTRUCTION_CUSTOM_FUNCTION (function), NULL);

	return function->priv->function;
}

/**
 * cdn_instruction_custom_function_set_function:
 * @function: A #CdnInstructionCustomFunction
 * @f: the function
 *
 * Set the function executed by the instruction.
 *
 **/
void
cdn_instruction_custom_function_set_function (CdnInstructionCustomFunction *function,
                                              CdnFunction                  *f)
{
	g_return_if_fail (CDN_IS_INSTRUCTION_CUSTOM_FUNCTION (function));

	set_function (function, f);
}
