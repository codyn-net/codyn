/*
 * cdn-function.c
 * This file is part of codyn
 *
 * Copyright (C) 2011 - Jesse van den Kieboom
 *
 * codyn is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * codyn is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with codyn; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

#include "cdn-function.h"
#include "cdn-compile-error.h"
#include "cdn-expression-tree-iter.h"
#include "instructions/cdn-instruction-rand.h"

#include <string.h>

/**
 * SECTION:cdn-function
 * @short_description: Custom user defined function
 *
 * It is possible to define custom user functions in the network which can
 * then be used from any expression. This class provides the basic
 * user function functionality. User defined functions can have optional
 * arguments with default values and can reference global constants as well
 * as use other user defined functions in their expressions.
 *
 * The #CdnFunction class can be subclassed to provide more specific types
 * of functions. One such example is the #CdnFunctionPolynomial class which
 * can be used to define and evaluate piecewise polynomials.
 *
 * <refsect2 id="CdnFunction-COPY">
 * <title>CdnFunction Copy Semantics</title>
 * When a function is copied with #cdn_object_copy, the function expression
 * and all the arguments are copied as well.
 * </refsect2>
 *
 */

#define CDN_FUNCTION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), CDN_TYPE_FUNCTION, CdnFunctionPrivate))

/* Properties */
enum
{
	PROP_0,
	PROP_EXPRESSION
};

/* signals */
enum
{
	ARGUMENT_ADDED,
	ARGUMENT_REMOVED,
	ARGUMENTS_REORDERED,
	NUM_SIGNALS
};

struct _CdnFunctionPrivate
{
	CdnExpression *expression;
	GList *arguments;
	GHashTable *arguments_hash;

	guint n_arguments;
	guint n_optional;
	guint n_implicit;
};

G_DEFINE_TYPE (CdnFunction, cdn_function, CDN_TYPE_OBJECT)

static guint signals[NUM_SIGNALS] = {0,};

GQuark
cdn_function_error_quark (void)
{
	static GQuark quark = 0;

	if (G_UNLIKELY (quark == 0))
	{
		quark = g_quark_from_static_string ("cdn_function_error");
	}

	return quark;
}


static void
cdn_function_finalize (GObject *object)
{
	CdnFunction *self = CDN_FUNCTION (object);

	if (self->priv->expression)
	{
		g_object_unref (self->priv->expression);
	}

	g_list_foreach (self->priv->arguments, (GFunc)g_object_unref, NULL);
	g_list_free (self->priv->arguments);

	g_hash_table_destroy (self->priv->arguments_hash);

	G_OBJECT_CLASS (cdn_function_parent_class)->finalize (object);
}

static void
cdn_function_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	CdnFunction *self = CDN_FUNCTION (object);

	switch (prop_id)
	{
		case PROP_EXPRESSION:
		{
			CdnExpression *expr;

			if (self->priv->expression)
			{
				g_object_unref (self->priv->expression);
			}

			expr = g_value_get_object (value);

			if (expr)
			{
				self->priv->expression = g_object_ref_sink (expr);
			}
			else
			{
				self->priv->expression = NULL;
			}

			if (self->priv->expression)
			{
				cdn_expression_set_has_cache (self->priv->expression,
				                              FALSE);
			}

			cdn_object_taint (CDN_OBJECT (self));
		}
		break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
cdn_function_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	CdnFunction *self = CDN_FUNCTION (object);

	switch (prop_id)
	{
		case PROP_EXPRESSION:
			g_value_set_object (value, self->priv->expression);
		break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static gboolean
cdn_function_compile_impl (CdnObject         *object,
                           CdnCompileContext *context,
                           CdnCompileError   *error)
{
	CdnFunction *self = CDN_FUNCTION (object);
	gboolean ret = TRUE;
	GList *item;

	if (cdn_object_is_compiled (object))
	{
		return TRUE;
	}

	if (context)
	{
		cdn_compile_context_save (context);
		g_object_ref (context);
	}

	context = cdn_object_get_compile_context (object, context);

	if (CDN_OBJECT_CLASS (cdn_function_parent_class)->compile)
	{
		if (!CDN_OBJECT_CLASS (cdn_function_parent_class)->compile (object,
		                                                            context,
		                                                            error))
		{
			g_object_unref (context);
			return FALSE;
		}
	}

	for (item = self->priv->arguments; item; item = g_list_next (item))
	{
		CdnExpression *expr;

		expr = cdn_function_argument_get_default_value (item->data);

		if (expr)
		{
			if (!cdn_expression_compile (expr, context, error))
			{
				if (error)
				{
					cdn_compile_error_set (error,
					                       NULL,
					                       object,
					                       NULL,
					                       NULL,
					                       NULL);
				}

				ret = FALSE;

				break;
			}
		}
	}

	if (!self->priv->expression || !ret)
	{
		cdn_compile_context_restore (context);
		g_object_unref (context);
		return ret;
	}

	if (!cdn_expression_compile (self->priv->expression,
	                             context,
	                             error))
	{
		if (error)
		{
			cdn_compile_error_set (error,
			                       NULL,
			                       object,
			                       NULL,
			                       NULL,
			                       NULL);
		}

		ret = FALSE;
	}

	cdn_compile_context_restore (context);
	g_object_unref (context);

	return ret;
}

static gdouble
cdn_function_evaluate_impl (CdnFunction *function)
{
	if (function->priv->expression)
	{
		g_slist_foreach ((GSList *)cdn_expression_get_rand_instructions (function->priv->expression),
		                 (GFunc)cdn_instruction_rand_next,
		                 NULL);

		gdouble ret = cdn_expression_evaluate (function->priv->expression);
		return ret;
	}
	else
	{
		return 0;
	}
}

static void
cdn_function_execute_impl (CdnFunction *function,
                           guint        nargs,
                           CdnStack    *stack)
{
	GList *item;
	guint i;
	GList *from;

	from = g_list_nth (function->priv->arguments, nargs - 1);
	item = from;

	/* Set provided arguments */
	for (i = 0; i < nargs; ++i)
	{
		CdnFunctionArgument *argument = item->data;
		gdouble val;
		CdnVariable *property = _cdn_function_argument_get_variable (argument);

		val = cdn_stack_pop (stack);

		cdn_variable_set_value (property, val);
		item = g_list_previous (item);
	}

	/* Evaluate the expression */
	if (CDN_FUNCTION_GET_CLASS (function)->evaluate)
	{
		cdn_stack_push (stack, CDN_FUNCTION_GET_CLASS (function)->evaluate (function));
	}
	else
	{
		cdn_stack_push (stack, 0);
	}
}

static void
cdn_function_copy_impl (CdnObject *object,
                        CdnObject *source)
{
	/* Chain up */
	if (CDN_OBJECT_CLASS (cdn_function_parent_class)->copy != NULL)
	{
		CDN_OBJECT_CLASS (cdn_function_parent_class)->copy (object, source);
	}

	// Copy expression
	CdnFunction *source_function = CDN_FUNCTION (source);
	CdnFunction *target = CDN_FUNCTION (object);

	if (source_function->priv->expression)
	{
		g_object_set (target,
		              "expression",
		              cdn_expression_copy (source_function->priv->expression),
		              NULL);
	}

	// Copy arguments
	GList *item;

	for (item = source_function->priv->arguments; item; item = g_list_next (item))
	{
		CdnFunctionArgument *orig = (CdnFunctionArgument *)item->data;
		CdnFunctionArgument *argument = cdn_function_argument_copy (orig);

		cdn_function_add_argument (target, argument);
	}
}

static void
cdn_function_reset_impl (CdnObject *object)
{
	/* Chain up */
	if (CDN_OBJECT_CLASS (cdn_function_parent_class)->reset != NULL)
	{
		CDN_OBJECT_CLASS (cdn_function_parent_class)->reset (object);
	}

	CdnFunction *function = CDN_FUNCTION (object);

	if (function->priv->expression)
	{
		cdn_expression_reset (function->priv->expression);
	}

	GList *item;

	for (item = function->priv->arguments; item; item = g_list_next (item))
	{
		CdnExpression *def;

		def = cdn_function_argument_get_default_value (item->data);

		if (def != NULL)
		{
			cdn_expression_reset (def);
		}
	}
}

static void
cdn_function_foreach_expression_impl (CdnObject                *object,
                                      CdnForeachExpressionFunc  func,
                                      gpointer                  userdata)
{
	CdnFunction *function = CDN_FUNCTION (object);
	GList *item;

	/* Chain up */
	if (CDN_OBJECT_CLASS (cdn_function_parent_class)->foreach_expression != NULL)
	{
		CDN_OBJECT_CLASS (cdn_function_parent_class)->foreach_expression (object,
		                                                                  func,
		                                                                  userdata);
	}

	for (item = function->priv->arguments; item; item = g_list_next (item))
	{
		CdnExpression *def;

		def = cdn_function_argument_get_default_value (item->data);

		if (def != NULL)
		{
			func (def, userdata);
		}
	}

	if (function->priv->expression)
	{
		func (function->priv->expression, userdata);
	}
}

static void
cdn_function_constructed (GObject *object)
{
	CdnFunction *function = CDN_FUNCTION (object);

	if (function->priv->expression != NULL)
	{
		g_object_ref_sink (function->priv->expression);
		g_object_unref (function->priv->expression);
	}
}

static void
cdn_function_template_expression_changed (CdnFunction *function,
                                          GParamSpec  *spec,
                                          CdnFunction *templ)
{
	GSList const *templates;
	GSList *last;

	templates = cdn_object_get_applied_templates (CDN_OBJECT (function));
	last = g_slist_last ((GSList *)templates);

	if (!last)
	{
		return;
	}

	if (templ == last->data)
	{
		g_object_set (function,
		              "expression",
		              cdn_expression_copy (cdn_function_get_expression (templ)),
		              NULL);
	}
}

static CdnFunction *
last_template (CdnFunction *target)
{
	CdnFunction *ret = NULL;
	GSList const *templates;

	templates = cdn_object_get_applied_templates (CDN_OBJECT (target));

	while (templates)
	{
		if (CDN_IS_FUNCTION (templates->data))
		{
			ret = templates->data;
		}

		templates = g_slist_next (templates);
	}

	return ret;
}

static gboolean
is_from_template (CdnFunction *target)
{
	CdnFunction *templ;

	templ = last_template (target);

	if (!templ)
	{
		return target->priv->expression == NULL &&
		       target->priv->n_arguments == 0;
	}

	return cdn_object_equal (CDN_OBJECT (target), CDN_OBJECT (templ));
}

static void
from_template (CdnFunction *target,
               CdnFunction *source)
{
	GList *item;

	g_list_foreach (target->priv->arguments,
	                (GFunc)g_object_unref,
	                NULL);

	g_list_free (target->priv->arguments);
	target->priv->arguments = NULL;

	target->priv->n_arguments = 0;
	target->priv->n_optional = 0;
	target->priv->n_implicit = 0;

	cdn_function_template_expression_changed (target, NULL, source);

	for (item = source->priv->arguments; item; item = g_list_next (item))
	{
		cdn_function_add_argument (target,
		                           cdn_function_argument_copy (item->data));
	}
}

static void
cdn_function_template_argument_added (CdnFunction         *source,
                                      CdnFunctionArgument *arg,
                                      CdnFunction         *target)
{
	if (last_template (target) == source)
	{
		cdn_function_add_argument (target, cdn_function_argument_copy (arg));
	}
}

static gboolean
arguments_equal (CdnFunction         *a,
                 CdnFunction         *b,
                 CdnFunctionArgument *ignore)
{
	GList *arg1;
	GList *arg2;

	arg1 = a->priv->arguments;
	arg2 = a->priv->arguments;

	while (arg1 != NULL && arg2 != NULL)
	{
		CdnFunctionArgument *a1;
		CdnFunctionArgument *a2;

		if ((arg1 == NULL) != (arg2 == NULL))
		{
			return FALSE;
		}

		a1 = arg1->data;
		a2 = arg2->data;

		arg1 = g_list_next (arg1);
		arg2 = g_list_next (arg2);

		if (ignore == a1)
		{
			continue;
		}

		if (g_strcmp0 (cdn_function_argument_get_name (a1),
		               cdn_function_argument_get_name (a2)) != 0)
		{
			return FALSE;
		}

		if (cdn_function_argument_get_explicit (a1) !=
		    cdn_function_argument_get_explicit (a2))
		{
			return FALSE;
		}

		if (cdn_function_argument_get_optional (a1) !=
		    cdn_function_argument_get_optional (a2))
		{
			return FALSE;
		}

		if (cdn_function_argument_get_optional (a1))
		{
			if (!cdn_expression_equal (cdn_function_argument_get_default_value (a1),
			                           cdn_function_argument_get_default_value (a2)))
			{
				return FALSE;
			}
		}
	}

	return TRUE;
}

static void
cdn_function_template_argument_removed (CdnFunction         *source,
                                        CdnFunctionArgument *arg,
                                        CdnFunction         *target)
{
	CdnFunction *templ;
	CdnFunctionArgument *item;

	item = cdn_function_get_argument (target, cdn_function_argument_get_name (arg));

	templ = last_template (target);

	if (templ &&
	    item &&
	    arguments_equal (target, templ, item) &&
	    cdn_expression_equal (target->priv->expression,
	                          templ->priv->expression))
	{
		cdn_function_remove_argument (target,
		                              item,
		                              NULL);
	}
}

static gboolean
cdn_function_apply_template_impl (CdnObject  *object,
                                  CdnObject  *templ,
                                  GError    **error)
{
	CdnFunction *target = NULL;
	CdnFunction *source = NULL;
	gboolean apply;

	target = CDN_FUNCTION (object);

	if (CDN_IS_FUNCTION (templ))
	{
		source = CDN_FUNCTION (templ);
	}

	apply = source && is_from_template (target);

	/* Remove all function arguments */
	if (apply)
	{
		g_list_foreach (target->priv->arguments,
		                (GFunc)g_object_unref,
		                NULL);

		g_list_free (target->priv->arguments);
		target->priv->arguments = NULL;

		target->priv->n_arguments = 0;
		target->priv->n_optional = 0;
		target->priv->n_implicit = 0;
	}

	if (CDN_OBJECT_CLASS (cdn_function_parent_class)->apply_template)
	{
		if (!CDN_OBJECT_CLASS (cdn_function_parent_class)->apply_template (object, templ, error))
		{
			return FALSE;
		}
	}

	if (source)
	{
		g_signal_connect_swapped (source,
		                          "notify::expression",
		                          G_CALLBACK (cdn_function_template_expression_changed),
		                          target);

		g_signal_connect (source,
		                  "argument-added",
		                  G_CALLBACK (cdn_function_template_argument_added),
		                  target);

		g_signal_connect (source,
		                  "argument-removed",
		                  G_CALLBACK (cdn_function_template_argument_removed),
		                  target);
	}

	if (apply)
	{
		from_template (target, source);
	}

	return TRUE;
}


static gboolean
cdn_function_unapply_template_impl (CdnObject  *object,
                                    CdnObject  *templ,
                                    GError    **error)
{
	GSList *last;
	GSList const *templates;
	gboolean waslast = FALSE;
	CdnFunction *target = NULL;
	CdnFunction *source = NULL;

	templates = cdn_object_get_applied_templates (object);
	last = g_slist_last ((GSList *)templates);

	target = CDN_FUNCTION (object);

	/* Remove all function arguments */
	if (CDN_IS_FUNCTION (templ) && last && last->data == templ)
	{
		source = CDN_FUNCTION (templ);

		g_list_foreach (target->priv->arguments,
		                (GFunc)g_object_unref,
		                NULL);

		g_list_free (target->priv->arguments);
		target->priv->arguments = NULL;

		target->priv->n_arguments = 0;
		target->priv->n_optional = 0;
		target->priv->n_implicit = 0;

		waslast = TRUE;
	}

	if (CDN_OBJECT_CLASS (cdn_function_parent_class)->unapply_template)
	{
		if (!CDN_OBJECT_CLASS (cdn_function_parent_class)->unapply_template (object, templ, error))
		{
			return FALSE;
		}
	}

	if (source)
	{
		g_signal_handlers_disconnect_by_func (target,
		                                      G_CALLBACK (cdn_function_template_argument_added),
		                                      object);

		g_signal_handlers_disconnect_by_func (target,
		                                      G_CALLBACK (cdn_function_template_argument_removed),
		                                      object);

		g_signal_handlers_disconnect_by_func (target,
		                                      G_CALLBACK (cdn_function_template_expression_changed),
		                                      object);
	}

	if (waslast)
	{
		templates = cdn_object_get_applied_templates (object);
		last = g_slist_last ((GSList *)templates);

		if (last)
		{
			from_template (source, last->data);
		}
		else
		{
			g_object_set (source, "expression", NULL, NULL);
		}
	}

	return TRUE;
}

static gboolean
cdn_function_equal_impl (CdnObject *a,
                         CdnObject *b)
{
	CdnFunction *fa;
	CdnFunction *fb;

	if (!CDN_OBJECT_CLASS (cdn_function_parent_class)->equal (a, b))
	{
		return FALSE;
	}

	fa = CDN_FUNCTION (a);
	fb = CDN_FUNCTION (b);

	if (!arguments_equal (fa, fb, NULL))
	{
		return FALSE;
	}

	return cdn_expression_equal (fa->priv->expression, fb->priv->expression);
}

static gboolean
on_argument_invalidate_name (CdnFunctionArgument *argument,
                             gchar const         *name,
                             CdnFunction         *function)
{
	CdnVariable *property = cdn_object_get_variable (CDN_OBJECT (function),
	                                                 name);

	CdnVariable *current = _cdn_function_argument_get_variable (argument);

	return property && current != property;
}

static void
on_argument_name_changed (CdnFunctionArgument *argument,
                          GParamSpec          *spec,
                          CdnFunction         *function)
{
	gchar const *name = cdn_function_argument_get_name (argument);
	CdnVariable *property = cdn_object_get_variable (CDN_OBJECT (function),
	                                                 name);

	CdnVariable *current = _cdn_function_argument_get_variable (argument);

	if (property == current)
	{
		return;
	}
	else if (property || !cdn_variable_set_name (current, name))
	{
		cdn_function_argument_set_name (argument,
		                                cdn_variable_get_name (current));
	}
}

static void
on_argument_optional_changed (CdnFunctionArgument *argument,
                              GParamSpec          *spec,
                              CdnFunction         *function)
{
	gboolean opt = cdn_function_argument_get_optional (argument);

	GList *item;

	/* Get the item which represents the first optional argument at the
	   moment */
	item = g_list_nth (function->priv->arguments,
	                   function->priv->n_arguments - function->priv->n_optional - function->priv->n_implicit);

	if (item == NULL || item->data != argument)
	{
		/* An argument other than the first optional one has changed
		   it optionality */

		/* First remove the argument from the list of arguments */
		function->priv->arguments =
			g_list_remove (function->priv->arguments,
			               argument);

		if (opt || item)
		{
			function->priv->arguments =
				g_list_insert_before (function->priv->arguments,
				                      item,
				                      argument);
		}
		else
		{
			function->priv->arguments =
				g_list_append (function->priv->arguments,
				               argument);
		}
	}

	if (opt)
	{
		++function->priv->n_optional;
	}
	else
	{
		--function->priv->n_optional;
	}

	g_signal_emit (function, signals[ARGUMENTS_REORDERED], 0);
}

static void
cdn_function_argument_added_impl (CdnFunction         *function,
                                  CdnFunctionArgument *argument)
{
	gchar const *name;

	if (!cdn_function_argument_get_explicit (argument))
	{
		/* Just append */
		function->priv->arguments = g_list_append (function->priv->arguments,
		                                           g_object_ref_sink (argument));
	}
	else if (cdn_function_argument_get_optional (argument))
	{
		/* Insert before first implicit */
		gint n;

		n = (gint)function->priv->n_arguments -
		    (gint)function->priv->n_implicit;

		function->priv->arguments = g_list_insert (function->priv->arguments,
		                                           g_object_ref_sink (argument),
		                                           n);
	}
	else
	{
		/* Insert before first optional */
		gint n;

		n = (gint)function->priv->n_arguments -
		    (gint)function->priv->n_optional -
		    (gint)function->priv->n_implicit;

		function->priv->arguments = g_list_insert (function->priv->arguments,
		                                           g_object_ref_sink (argument),
		                                           n);
	}

	name = cdn_function_argument_get_name (argument);

	if (g_str_has_suffix (name, "'"))
	{
		gchar *pname;
		CdnFunctionArgument *parg;

		pname = g_strndup (name, strlen (name) - 1);
		parg = cdn_function_get_argument (function, pname);

		if (parg)
		{
			cdn_variable_set_derivative (_cdn_function_argument_get_variable (parg),
			                             _cdn_function_argument_get_variable (argument));
		}

		g_free (pname);
	}

	g_hash_table_insert (function->priv->arguments_hash,
	                     g_strdup (cdn_function_argument_get_name (argument)),
	                     argument);

	++function->priv->n_arguments;

	if (cdn_function_argument_get_optional (argument))
	{
		++function->priv->n_optional;
	}

	if (!cdn_function_argument_get_explicit (argument))
	{
		++function->priv->n_implicit;
	}

	g_signal_connect (argument,
	                  "notify::name",
	                  G_CALLBACK (on_argument_name_changed),
	                  function);

	g_signal_connect (argument,
	                  "invalidate-name",
	                  G_CALLBACK (on_argument_invalidate_name),
	                  function);

	g_signal_connect (argument,
	                  "notify::optional",
	                  G_CALLBACK (on_argument_optional_changed),
	                  function);

	cdn_object_taint (CDN_OBJECT (function));
}

static void
cdn_function_class_init (CdnFunctionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	CdnObjectClass *cdn_object_class = CDN_OBJECT_CLASS (klass);

	object_class->finalize = cdn_function_finalize;
	object_class->constructed = cdn_function_constructed;

	object_class->set_property = cdn_function_set_property;
	object_class->get_property = cdn_function_get_property;

	cdn_object_class->compile = cdn_function_compile_impl;
	cdn_object_class->copy = cdn_function_copy_impl;
	cdn_object_class->reset = cdn_function_reset_impl;
	cdn_object_class->foreach_expression = cdn_function_foreach_expression_impl;
	cdn_object_class->apply_template = cdn_function_apply_template_impl;
	cdn_object_class->unapply_template = cdn_function_unapply_template_impl;
	cdn_object_class->equal = cdn_function_equal_impl;

	klass->execute = cdn_function_execute_impl;
	klass->evaluate = cdn_function_evaluate_impl;
	klass->argument_added = cdn_function_argument_added_impl;

	g_object_class_install_property (object_class,
	                                 PROP_EXPRESSION,
	                                 g_param_spec_object ("expression",
	                                                      "Expression",
	                                                      "Expression",
	                                                      CDN_TYPE_EXPRESSION,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	/**
	 * CdnFunction::argument-added:
	 * @function: a #CdnFunction
	 * @argument: a #CdnFunctionArgument
	 *
	 * Emitted when an argument has been added to the function
	 *
	 **/
	signals[ARGUMENT_ADDED] =
		g_signal_new ("argument-added",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (CdnFunctionClass,
		                               argument_added),
		              NULL,
		              NULL,
		              g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE,
		              1,
		              CDN_TYPE_FUNCTION_ARGUMENT);

	/**
	 * CdnFunction::argument-removed:
	 * @function: a #CdnFunction
	 * @argument: a #CdnFunctionArgument
	 *
	 * Emitted when an argument has been removed from the function
	 *
	 **/
	signals[ARGUMENT_REMOVED] =
		g_signal_new ("argument-removed",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (CdnFunctionClass,
		                               argument_removed),
		              NULL,
		              NULL,
		              g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE,
		              1,
		              CDN_TYPE_FUNCTION_ARGUMENT);

	/**
	 * Cdnfunction::arguments-reordered:
	 * @function: a #CdnFunction
	 *
	 * Emitted when the order of the function arguments has changed.
	 *
	 **/
	signals[ARGUMENTS_REORDERED] =
		g_signal_new ("arguments-reordered",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (CdnFunctionClass, arguments_reordered),
		              NULL,
		              NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE,
		              0);

	g_type_class_add_private (object_class, sizeof(CdnFunctionPrivate));
}

static void
cdn_function_init (CdnFunction *self)
{
	self->priv = CDN_FUNCTION_GET_PRIVATE (self);

	self->priv->arguments_hash = g_hash_table_new_full (g_str_hash,
	                                                    g_str_equal,
	                                                    (GDestroyNotify)g_free,
	                                                    NULL);
}

/**
 * cdn_function_new:
 * @name: The function name
 * @expression: The function expression
 *
 * Create a new custom user function. After creation, function arguments
 * can be added to the function using #cdn_function_add_argument.
 *
 * Returns: A #CdnFunction
 *
 **/
CdnFunction *
cdn_function_new (gchar const   *name,
                  CdnExpression *expr)
{
	CdnFunction *ret;

	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (expr == NULL || CDN_IS_EXPRESSION (expr), NULL);

	if (expr && g_object_is_floating (G_OBJECT (expr)))
	{
		g_object_ref_sink (expr);
	}
	else if (expr)
	{
		g_object_ref (expr);
	}

	ret = g_object_new (CDN_TYPE_FUNCTION,
	                    "id", name,
	                    "expression", expr,
	                    NULL);

	if (expr)
	{
		g_object_unref (expr);
	}

	return ret;
}

/**
 * cdn_function_add_argument:
 * @function: A #CdnFunction
 * @argument: A #CdnFunctionArgument
 *
 * Add a function argument. A proxy property for the argument will be
 * automatically created if it does not exist yet. If the argument already
 * exists it will not be added.
 *
 **/
void
cdn_function_add_argument (CdnFunction         *function,
                           CdnFunctionArgument *argument)
{
	g_return_if_fail (CDN_IS_FUNCTION (function));
	g_return_if_fail (CDN_IS_FUNCTION_ARGUMENT (argument));
	g_return_if_fail (_cdn_function_argument_get_variable (argument) == NULL);

	gchar const *name = cdn_function_argument_get_name (argument);

	CdnVariable *property = cdn_object_get_variable (CDN_OBJECT (function),
	                                                 name);

	if (property == NULL)
	{
		/* Add the proxy property */
		property = cdn_variable_new (name,
		                             cdn_expression_new0 (),
		                             CDN_VARIABLE_FLAG_NONE);

		if (!cdn_object_add_variable (CDN_OBJECT (function), property, NULL))
		{
			return;
		}
	}
	else if (cdn_function_get_argument (function,
	                                    cdn_function_argument_get_name (argument)))
	{
		return;
	}

	_cdn_function_argument_set_variable (argument, property);

	cdn_expression_set_has_cache (cdn_variable_get_expression (property),
	                              FALSE);

	g_signal_emit (function, signals[ARGUMENT_ADDED], 0, argument);
}

/**
 * cdn_function_remove_argument:
 * @function: A #CdnFunction
 * @argument: A #CdnFunctionArgument
 * @error: A #GError
 *
 * Remove a function argument.
 *
 * Returns: %TRUE if the argument could be removed, %FALSE otherwise
 *
 **/
gboolean
cdn_function_remove_argument (CdnFunction          *function,
                              CdnFunctionArgument  *argument,
                              GError              **error)
{
	gchar const *name;

	g_return_val_if_fail (CDN_IS_FUNCTION (function), FALSE);
	g_return_val_if_fail (CDN_IS_FUNCTION_ARGUMENT (argument), FALSE);

	GList *item = g_list_find (function->priv->arguments, argument);

	if (item == NULL)
	{
		if (error)
		{
			g_set_error (error,
			             cdn_function_error_quark (),
			             CDN_FUNCTION_ERROR_ARGUMENT_NOT_FOUND,
			             "Function argument %s not found",
			             cdn_function_argument_get_name (argument));
		}

		return FALSE;
	}

	name = cdn_function_argument_get_name (argument);

	if (cdn_object_remove_variable (CDN_OBJECT (function), name, error))
	{
		if (cdn_function_argument_get_optional (argument))
		{
			--function->priv->n_optional;
		}

		if (!cdn_function_argument_get_explicit (argument))
		{
			--function->priv->n_implicit;
		}

		function->priv->arguments = g_list_delete_link (function->priv->arguments,
		                                                item);

		g_hash_table_remove (function->priv->arguments_hash,
		                     cdn_function_argument_get_name (argument));

		_cdn_function_argument_set_variable (argument, NULL);

		--function->priv->n_arguments;

		g_signal_handlers_disconnect_by_func (argument,
		                                      on_argument_invalidate_name,
		                                      function);

		g_signal_handlers_disconnect_by_func (argument,
		                                      on_argument_name_changed,
		                                      function);

		g_signal_handlers_disconnect_by_func (argument,
		                                      on_argument_optional_changed,
		                                      function);

		if (g_str_has_suffix (name, "'"))
		{
			gchar *pname;
			CdnFunctionArgument *parg;

			pname = g_strndup (name, strlen (name) - 1);
			parg = cdn_function_get_argument (function, pname);

			if (parg)
			{
				cdn_variable_set_derivative (_cdn_function_argument_get_variable (parg),
				                             NULL);
			}

			g_free (pname);
		}

		cdn_object_taint (CDN_OBJECT (function));

		g_signal_emit (function, signals[ARGUMENT_REMOVED], 0, argument);

		g_object_unref (argument);
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/**
 * cdn_function_get_arguments:
 * @function: A #CdnFunction
 *
 * Get the list of function arguments. The returned list is used internally
 * and should not be modified or freed.
 *
 * Returns: (element-type CdnFunctionArgument) (transfer none): A #GList
 *
 **/
const GList *
cdn_function_get_arguments (CdnFunction *function)
{
	g_return_val_if_fail (CDN_IS_FUNCTION (function), NULL);

	return function->priv->arguments;
}

/**
 * cdn_function_execute:
 * @function: A #CdnFunction
 * @stack: A #CdnStack
 *
 * Execute the function. This is used internally when the function needs to be
 * evaluated.
 *
 **/
void
cdn_function_execute (CdnFunction *function,
                      guint        nargs,
                      CdnStack    *stack)
{
	CDN_FUNCTION_GET_CLASS (function)->execute (function,
	                                            nargs,
	                                            stack);
}

/**
 * cdn_function_get_expression:
 * @function: A #CdnFunction
 *
 * Get the function expression.
 *
 * Returns: (type CdnExpression) (transfer none): A #CdnExpression
 *
 **/
CdnExpression *
cdn_function_get_expression (CdnFunction *function)
{
	g_return_val_if_fail (CDN_IS_FUNCTION (function), NULL);

	return function->priv->expression;
}

/**
 * cdn_function_set_expression:
 * @function: A #CdnFunction
 * @expression: A #CdnExpression
 *
 * Set the function expression.
 *
 **/
void
cdn_function_set_expression (CdnFunction   *function,
                             CdnExpression *expression)
{
	g_return_if_fail (CDN_IS_FUNCTION (function));

	g_object_set (G_OBJECT (function), "expression", expression, NULL);
}

/**
 * cdn_function_clear_arguments:
 * @function: A #CdnFunction
 * @error: A #GError
 *
 * Remove all the function arguments.
 *
 * Returns: %TRUE if all arguments could be successfully removed, %FALSE otherwise
 *
 **/
gboolean
cdn_function_clear_arguments (CdnFunction  *function,
                              GError      **error)
{
	g_return_val_if_fail (CDN_IS_FUNCTION (function), FALSE);

	GList *copy = g_list_copy (function->priv->arguments);
	GList *item;

	for (item = copy; item; item = g_list_next (item))
	{
		if (!cdn_function_remove_argument (function, item->data, error))
		{
			g_list_free (copy);
			return FALSE;
		}
	}

	g_list_free (copy);
	return TRUE;
}

/**
 * cdn_function_get_n_optional:
 * @function: A #CdnFunction
 *
 * Get the number of optional arguments. The optional arguments are always
 * at the end of the list of arguments of the function.
 *
 * Returns: the number of optional arguments
 *
 **/
guint
cdn_function_get_n_optional (CdnFunction *function)
{
	g_return_val_if_fail (CDN_IS_FUNCTION (function), 0);

	return function->priv->n_optional;
}

/**
 * cdn_function_get_n_arguments:
 * @function: A #CdnFunction
 *
 * Get the number of arguments. This value is cached and is thus faster than
 * using #cdn_function_get_arguments and #g_list_length.
 *
 * Returns: the number of arguments
 *
 **/
guint
cdn_function_get_n_arguments (CdnFunction *function)
{
	g_return_val_if_fail (CDN_IS_FUNCTION (function), 0);

	return function->priv->n_arguments;
}

/**
 * cdn_function_get_n_implicit:
 * @function: A #CdnFunction
 *
 * Get the number of implicit arguments.
 *
 * Returns: the number of implicit arguments
 *
 **/
guint
cdn_function_get_n_implicit (CdnFunction *function)
{
	g_return_val_if_fail (CDN_IS_FUNCTION (function), 0);

	return function->priv->n_implicit;
}

CdnFunctionArgument *
cdn_function_get_argument (CdnFunction *function,
                           gchar const *name)
{
	g_return_val_if_fail (CDN_IS_FUNCTION (function), NULL);

	return g_hash_table_lookup (function->priv->arguments_hash, name);
}