/*
 * cpg-operator-diff.c
 * This file is part of cpg-network
 *
 * Copyright (C) 2011 - Jesse van den Kieboom
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

#include "cpg-operator-pdiff.h"
#include "cpg-operator.h"
#include "cpg-property.h"
#include "cpg-usable.h"
#include "cpg-integrator.h"
#include "cpg-symbolic.h"
#include "cpg-function.h"
#include "cpg-expression-tree-iter.h"
#include "cpg-network.h"

#include "instructions/cpg-instructions.h"

#include <math.h>

/**
 * SECTION:cpg-operator-diff
 * @short_description: Math operator for diff evaluation of an expression
 *
 * The #CpgOperatorPDiff is a special operator that can be used in
 * mathematical expressions ('delay'). When evaluated, it will return the
 * diff value of its argument (which can be an arbitrary expression).
 *
 */

#define CPG_OPERATOR_PDIFF_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), CPG_TYPE_OPERATOR_PDIFF, CpgOperatorPDiffPrivate))

struct _CpgOperatorPDiffPrivate
{
	CpgFunction *function;
};

G_DEFINE_TYPE (CpgOperatorPDiff,
               cpg_operator_pdiff,
               CPG_TYPE_OPERATOR)

static gchar *
cpg_operator_pdiff_get_name ()
{
	return g_strdup ("pdiff");
}

static CpgFunction *
derived_function (CpgExpression *expr)
{
	GSList const *instr;

	instr = cpg_expression_get_instructions (expr);

	if (instr->next)
	{
		return NULL;
	}

	if (CPG_IS_INSTRUCTION_CUSTOM_FUNCTION_REF (instr->data))
	{
		return cpg_instruction_custom_function_ref_get_function (instr->data);
	}
	else if (CPG_IS_INSTRUCTION_CUSTOM_OPERATOR_REF (instr->data))
	{
		CpgOperator *op;

		op = cpg_instruction_custom_operator_ref_get_operator (instr->data);
		return cpg_operator_get_primary_function (op);
	}

	return NULL;
}

static GHashTable *
get_property_map (GList const *args,
                  GList const *nargs,
                  gint         num,
                  GHashTable  *ret)
{
	if (ret == NULL)
	{
		// Map properties of func to properties in nf
		ret = g_hash_table_new_full (g_direct_hash,
		                             g_direct_equal,
		                             NULL,
		                             (GDestroyNotify)cpg_expression_tree_iter_free);
	}

	while (args && nargs && num != 0)
	{
		CpgFunctionArgument *arg;
		CpgFunctionArgument *argnf;
		GSList *instr;

		CpgProperty *p;
		CpgProperty *pnf;

		arg = args->data;
		argnf = nargs->data;

		args = g_list_next (args);
		nargs = g_list_next (nargs);

		p = _cpg_function_argument_get_property (arg);
		pnf = _cpg_function_argument_get_property (argnf);

		instr = g_slist_prepend (NULL,
		                         cpg_instruction_property_new (pnf));

		g_hash_table_insert (ret,
		                     p,
		                     cpg_expression_tree_iter_new_from_instructions (instr));

		g_slist_foreach (instr, (GFunc)cpg_mini_object_free, NULL);
		g_slist_free (instr);

		--num;
	}

	return ret;
}

static CpgFunctionArgument *
derived_arg (CpgFunction   *func,
             CpgExpression *expr)
{
	GSList const *instr;
	gchar const *pname;

	instr = cpg_expression_get_instructions (expr);

	if (instr->next || !CPG_IS_INSTRUCTION_PROPERTY (instr->data))
	{
		return NULL;
	}

	pname = cpg_property_get_name (cpg_instruction_property_get_property (instr->data));
	return cpg_function_get_argument (func, pname);
}

static CpgProperty *
derived_property (CpgFunction   *func,
                  CpgExpression *expr)
{
	CpgFunctionArgument *arg;

	arg = derived_arg (func, expr);

	if (!arg)
	{
		return NULL;
	}

	return _cpg_function_argument_get_property (arg);
}

static gboolean
validate_arguments (GSList const  *expressions,
                    GSList const  *towardsexpr,
                    CpgFunction  **func,
                    CpgProperty  **towards,
                    GList        **syms,
                    gint          *order,
                    GError       **error)
{
	CpgExpression *expr;

	expr = expressions->data;

	*func = derived_function (expressions->data);

	if (!*func)
	{
		g_set_error (error,
		             CPG_SYMBOLIC_ERROR,
		             CPG_SYMBOLIC_ERROR_UNSUPPORTED,
		             "Expected function reference but got `%s'. Use df_dt[] for deriving expressions",
		             cpg_expression_get_as_string (expressions->data));

		return FALSE;
	}

	*syms = NULL;
	*order = 1;

	expressions = expressions->next;
	*towards = derived_property (*func, expressions->data);

	if (!*towards)
	{
		g_set_error (error,
		             CPG_SYMBOLIC_ERROR,
		             CPG_SYMBOLIC_ERROR_UNSUPPORTED,
		             "Expected partial function variable reference but got `%s'.",
		             cpg_expression_get_as_string (expressions->data));

		return FALSE;
	}

	if (expressions->next)
	{
		*order = cpg_expression_evaluate (expressions->next->data);
	}

	while (towardsexpr)
	{
		CpgFunctionArgument *arg;

		arg = derived_arg (*func, towardsexpr->data);

		if (!arg)
		{
			g_set_error (error,
			             CPG_SYMBOLIC_ERROR,
			             CPG_SYMBOLIC_ERROR_UNSUPPORTED,
			             "Expected function variable but got `%s' for pdiff of `%s'",
			             cpg_expression_get_as_string (towardsexpr->data),
			             cpg_expression_get_as_string (expr));

			g_list_free (*syms);
			return FALSE;
		}
		else
		{
			*syms = g_list_prepend (*syms, arg);
		}

		towardsexpr = g_slist_next (towardsexpr);
	}

	if (!*syms)
	{
		// By default add all properties of the function as syms
		GList const *args;

		args = cpg_function_get_arguments (*func);

		while (args)
		{
			if (cpg_function_argument_get_explicit (args->data))
			{
				*syms = g_list_prepend (*syms, args->data);
			}

			args = g_list_next (args);
		}
	}

	*syms = g_list_reverse (*syms);

	return TRUE;
}

static GSList *
resolve_symprops (CpgFunction *f,
                  GList const *symargs)
{
	GSList *ret = NULL;

	while (symargs)
	{
		CpgFunctionArgument *arg;

		arg = cpg_function_get_argument (f,
		                                 cpg_function_argument_get_name (symargs->data));

		ret = g_slist_prepend (ret, _cpg_function_argument_get_property (arg));

		symargs = g_list_next (symargs);
	}

	return g_slist_reverse (ret);
}

static gboolean
cpg_operator_pdiff_initialize (CpgOperator   *op,
                               GSList const **expressions,
                               gint           num_expressions,
                               GSList const **indices,
                               gint           num_indices,
                               gint           num_arguments,
                               GError       **error)
{
	CpgOperatorPDiff *diff;
	CpgFunction *func;
	GList *symargs;
	GHashTable *property_map;
	GHashTable *diff_map;
	CpgFunction *nf = NULL;
	gchar *s;
	CpgProperty *towards;
	GSList *syms;
	gint order;
	CpgExpression *derived;

	if (!CPG_OPERATOR_CLASS (cpg_operator_pdiff_parent_class)->initialize (op,
	                                                                       expressions,
	                                                                       num_expressions,
	                                                                       indices,
	                                                                       num_indices,
	                                                                       num_arguments,
	                                                                       error))
	{
		return FALSE;
	}

	if (num_expressions <= 0 ||
	    num_expressions > 2 ||
	    (expressions[0]->next && expressions[0]->next->next) ||
	    (num_expressions > 1 && expressions[1]->next))
	{
		g_set_error (error,
		             CPG_NETWORK_LOAD_ERROR,
		             CPG_NETWORK_LOAD_ERROR_OPERATOR,
		             "The operator `pdiff' expects arguments [Func, towards{,order}{;<vars>}] {optional} <list>");

		return FALSE;
	}

	diff = CPG_OPERATOR_PDIFF (op);

	if (!validate_arguments (expressions[0],
	                         num_expressions > 1 ? expressions[1] : NULL,
	                         &func,
	                         &towards,
	                         &symargs,
	                         &order,
	                         error))
	{
		return FALSE;
	}

	// Here we are going to generate a new function with represents
	// the symbolic derivation
	s = g_strconcat ("pd",
	                 cpg_object_get_id (CPG_OBJECT (func)),
	                 "dt",
	                 NULL);

	nf = CPG_FUNCTION (cpg_object_copy (CPG_OBJECT (func)));
	cpg_object_set_id (CPG_OBJECT (nf), s);
	g_free (s);

	cpg_function_set_expression (nf, cpg_expression_new0 ());

	// Map original function properties to the new function arguments
	property_map = get_property_map (cpg_function_get_arguments (func),
	                                 cpg_function_get_arguments (nf),
	                                 -1,
	                                 NULL);

	// We use the diff map in partial derivation to indicate towards
	// which variable we differentiate
	towards = _cpg_function_argument_get_property (cpg_function_get_argument (nf, cpg_property_get_name (towards)));

	diff_map = g_hash_table_new (g_direct_hash, g_direct_equal);
	g_hash_table_insert (diff_map, towards, NULL);

	// newsymargs contains the CpgFunctionArgument of the new function
	syms = resolve_symprops (nf, symargs);

	derived = cpg_symbolic_derive (expressions[0]->data,
	                               syms,
	                               property_map,
	                               diff_map,
	                               order,
	                               CPG_SYMBOLIC_DERIVE_PARTIAL |
	                               CPG_SYMBOLIC_DERIVE_SIMPLIFY,
	                               error);

	g_slist_free (syms);

	cpg_function_set_expression (nf, derived);
	diff->priv->function = nf;

	if (property_map)
	{
		g_hash_table_destroy (property_map);
	}

	if (diff_map)
	{
		g_hash_table_destroy (diff_map);
	}

	g_list_free (symargs);

	return derived != NULL;
}

static void
cpg_operator_pdiff_finalize (GObject *object)
{
	G_OBJECT_CLASS (cpg_operator_pdiff_parent_class)->finalize (object);
}

static CpgFunction *
cpg_operator_pdiff_get_function (CpgOperator *op,
                                 gint        *idx,
                                 gint         numidx)
{
	return CPG_OPERATOR_PDIFF (op)->priv->function;
}

static void
cpg_operator_pdiff_foreach_function (CpgOperator *op,
                                     CpgForeachFunctionFunc func,
                                     gpointer userdata)
{
	func (CPG_OPERATOR_PDIFF (op)->priv->function, userdata);
}

static CpgOperator *
cpg_operator_pdiff_copy (CpgOperator *op)
{
	CpgOperatorPDiff *diff;
	CpgOperatorPDiff *ret;

	diff = CPG_OPERATOR_PDIFF (op);

	ret = CPG_OPERATOR_PDIFF (g_object_new (CPG_TYPE_OPERATOR_PDIFF, NULL));

	CPG_OPERATOR_CLASS (cpg_operator_pdiff_parent_class)->initialize (CPG_OPERATOR (ret),
	                                                                 cpg_operator_all_expressions (op),
	                                                                 cpg_operator_num_expressions (op),
	                                                                 cpg_operator_all_indices (op),
	                                                                 cpg_operator_num_indices (op),
	                                                                 cpg_operator_get_num_arguments (op),
	                                                                 NULL);

	if (diff->priv->function)
	{
		ret->priv->function = g_object_ref (CPG_OBJECT (diff->priv->function));
	}

	return CPG_OPERATOR (ret);
}

static void
cpg_operator_pdiff_class_init (CpgOperatorPDiffClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	CpgOperatorClass *op_class = CPG_OPERATOR_CLASS (klass);

	object_class->finalize = cpg_operator_pdiff_finalize;

	op_class->get_name = cpg_operator_pdiff_get_name;
	op_class->initialize = cpg_operator_pdiff_initialize;
	op_class->get_function = cpg_operator_pdiff_get_function;
	op_class->foreach_function = cpg_operator_pdiff_foreach_function;
	op_class->copy = cpg_operator_pdiff_copy;

	g_type_class_add_private (object_class, sizeof(CpgOperatorPDiffPrivate));
}

static void
cpg_operator_pdiff_init (CpgOperatorPDiff *self)
{
	self->priv = CPG_OPERATOR_PDIFF_GET_PRIVATE (self);
}

CpgOperatorPDiff *
cpg_operator_pdiff_new ()
{
	return g_object_new (CPG_TYPE_OPERATOR_PDIFF, NULL);
}
