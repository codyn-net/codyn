/*
 * cdn-operator-delayed.c
 * This file is part of codyn
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

#include "cdn-operator-delayed.h"
#include "cdn-operator.h"
#include "cdn-variable.h"
#include "cdn-usable.h"
#include "integrators/cdn-integrator.h"
#include "cdn-network.h"

#include <math.h>
#include <string.h>
#include <codyn/instructions/cdn-instructions.h>

/**
 * SECTION:cdn-operator-delayed
 * @short_description: Math operator for delayed evaluation of an expression
 *
 * The #CdnOperatorDelayed is a special operator that can be used in
 * mathematical expressions ('delay'). When evaluated, it will return the
 * delayed value of its argument (which can be an arbitrary expression).
 *
 */

#define CDN_OPERATOR_DELAYED_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), CDN_TYPE_OPERATOR_DELAYED, CdnOperatorDelayedPrivate))

typedef struct _HistoryItem HistoryItem;

struct _HistoryItem
{
	HistoryItem *next;
	HistoryItem *prev;

	gdouble t;

	gdouble *v;
};

typedef struct
{
	HistoryItem *first;
	HistoryItem *last;
	guint size;
} HistoryList;

struct _CdnOperatorDelayedPrivate
{
	HistoryList history;
	HistoryList history_pool;

	CdnExpression *expression;
	CdnExpression *initial_value;
	CdnExpression *delay_expression;

	gdouble last_t;
	gboolean first_last_t;

	CdnVariable *tvar;

	CdnStackManipulation smanip;
	gint push_dims[2];

	gdouble eval_at_t;
};

G_DEFINE_TYPE (CdnOperatorDelayed,
               cdn_operator_delayed,
               CDN_TYPE_OPERATOR)

enum
{
	PROP_0,
	PROP_EXPRESSION,
	PROP_INITIAL_VALUE
};

static void
history_remove_slice (HistoryList *history,
                      HistoryItem *start,
                      HistoryItem *end,
                      guint        num)
{
	if (num == 0)
	{
		return;
	}

	if (start->prev)
	{
		start->prev->next = end->next;
	}

	if (end->next)
	{
		end->next->prev = start->prev;
	}

	if (history->first == start)
	{
		history->first = end->next;
	}

	if (history->last == end)
	{
		history->last = start->prev;
	}

	history->size -= num;

	start->prev = end->next = NULL;
}

static void
history_append_slice (HistoryList *history,
                      HistoryItem *first,
                      HistoryItem *last,
                      guint         num)
{
	if (num == 0)
	{
		return;
	}

	first->prev = history->last;

	if (history->last)
	{
		history->last->next = first;
	}

	history->last = last;

	if (history->first == NULL)
	{
		history->first = first;
	}

	history->size += num;
}

static void
history_concat (HistoryList *l1,
                HistoryList *l2)
{
	history_append_slice (l1, l2->first, l2->last, l2->size);

	l2->first = NULL;
	l2->last = NULL;
	l2->size = 0;
}

static void
history_to_pool (CdnOperatorDelayed *operator,
                 gdouble             fromt)
{
	HistoryItem *item;
	guint num = 0;
	HistoryItem *first;

	item = operator->priv->history.first;

	if (!item || item->t >= fromt)
	{
		return;
	}

	while (item->next && item->next->t < fromt)
	{
		item = item->next;
		++num;
	}

	item = item->prev;

	if (num == 0 || !item)
	{
		return;
	}

	first = operator->priv->history.first;

	history_remove_slice (&operator->priv->history,
	                      first,
	                      item,
	                      num);

	history_append_slice (&operator->priv->history_pool,
	                      first,
	                      item,
	                      num);
}

static HistoryItem *
pool_to_history (CdnOperatorDelayed  *operator,
                 gint                 num)
{
	HistoryItem *ret;

	if (num == 0)
	{
		return NULL;
	}

	ret = operator->priv->history_pool.first;

	// Remove from the pool
	if (num >= operator->priv->history_pool.size)
	{
		gint i;
		gint n;

		HistoryItem *slice_start = NULL;
		HistoryItem *slice_end = NULL;

		n = num - operator->priv->history_pool.size;

		history_concat (&operator->priv->history,
		                &operator->priv->history_pool);

		for (i = 0; i < n; ++i)
		{
			HistoryItem *h;

			h = g_slice_new0 (HistoryItem);

			if (slice_start == NULL)
			{
				slice_start = slice_end = h;
			}
			else
			{
				h->prev = slice_end;
				slice_end->next = h;

				slice_end = h;
			}
		}

		if (!ret)
		{
			ret = slice_start;
		}

		history_append_slice (&operator->priv->history,
		                      slice_start,
		                      slice_end,
		                      n);
	}
	else
	{
		// Remove first num from pool
		HistoryItem *ptr = ret;
		gint i;

		for (i = 0; i < num - 1; ++i)
		{
			ptr = ptr->next;
		}

		history_remove_slice (&operator->priv->history_pool,
		                      ret,
		                      ptr,
		                      num);

		history_append_slice (&operator->priv->history,
		                      ret,
		                      ptr,
		                      num);
	}

	if (!ret)
	{
		ret = operator->priv->history.first;
	}

	return ret;
}

static gchar *
cdn_operator_delayed_get_name ()
{
	return g_strdup ("delayed");
}

static gboolean
cdn_operator_delayed_initialize (CdnOperator   *op,
                                 GSList const **expressions,
                                 gint           num_expressions,
                                 GSList const **indices,
                                 gint           num_indices,
                                 gint           num_arguments,
                                 gint          *argdim,
                                 GError       **error)
{
	CdnOperatorDelayed *delayed;
	CdnOperatorClass *cls;
	guint llen = 0;

	cls = CDN_OPERATOR_CLASS (cdn_operator_delayed_parent_class);

	if (!cls->initialize (op,
	                      expressions,
	                      num_expressions,
	                      indices,
	                      num_indices,
	                      num_arguments,
	                      argdim,
	                      error))
	{
		return FALSE;
	}

	if (num_expressions > 0)
	{
		llen = g_slist_length ((GSList *)expressions[0]);
	}

	if (num_expressions != 1 || llen > 2)
	{
		g_set_error (error,
		             CDN_NETWORK_LOAD_ERROR,
		             CDN_NETWORK_LOAD_ERROR_OPERATOR,
		             "The operator `delayed' expects one or two "
		             "expressions, but got %d",
		             llen);

		return FALSE;
	}

	if (num_arguments != 1)
	{
		g_set_error (error,
		             CDN_NETWORK_LOAD_ERROR,
		             CDN_NETWORK_LOAD_ERROR_OPERATOR,
		             "The operator `delayed' expects exactly one "
		             "argument (time delay), but got %d",
		             num_arguments);

		return FALSE;
	}

	if (argdim && argdim[0] * argdim[1] != 1)
	{
		g_set_error (error,
		             CDN_NETWORK_LOAD_ERROR,
		             CDN_NETWORK_LOAD_ERROR_OPERATOR,
		             "The operator `delayed' currently on supports "
		             "1-by-1 time delay (got %d-by-%d)",
		             argdim[0],
		             argdim[1]);

		return FALSE;
	}

	delayed = CDN_OPERATOR_DELAYED (op);
	delayed->priv->expression = g_object_ref_sink (expressions[0]->data);

	cdn_expression_get_dimension (delayed->priv->expression,
	                              &delayed->priv->push_dims[0],
	                              &delayed->priv->push_dims[1]);

	if (expressions[0]->next)
	{
		gint numr;
		gint numc;

		delayed->priv->initial_value =
			g_object_ref_sink (expressions[0]->next->data);

		cdn_expression_get_dimension (delayed->priv->initial_value,
		                              &numr,
		                              &numc);

		if (numr != delayed->priv->push_dims[0] ||
		    numc != delayed->priv->push_dims[1])
		{
			g_set_error (error,
			             CDN_NETWORK_LOAD_ERROR,
			             CDN_NETWORK_LOAD_ERROR_OPERATOR,
			             "The dimensions of the expression "
			             "(%d-by-%d) and the initial value "
			             "(%d-by-%d) must be the same",
			             delayed->priv->push_dims[0],
			             delayed->priv->push_dims[1],
			             numr,
			             numc);

			return FALSE;
		}
	}

	return TRUE;
}

static void
evaluate_on_stack (CdnOperatorDelayed *d,
                   gdouble             t,
                   CdnExpression      *expression,
                   CdnStack           *stack)
{
	gdouble const *ret;
	gint numr;
	gint numc;
	gdouble told;

	// temporarily set t
	told = cdn_variable_get_value (d->priv->tvar);
	cdn_variable_set_value (d->priv->tvar, t);

	if (t != d->priv->eval_at_t)
	{
		cdn_expression_reset_cache (expression);
	}

	ret = cdn_expression_evaluate_values (expression, &numr, &numc);

	d->priv->eval_at_t = t;

	cdn_stack_pushn (stack, ret, numr * numc);

	cdn_variable_set_value (d->priv->tvar, told);
}

static void
cdn_operator_delayed_execute (CdnOperator     *op,
                              CdnStack        *stack)
{
	CdnOperatorDelayed *d;
	gdouble td;
	HistoryItem *h;
	gdouble delay;
	gdouble t;

	d = (CdnOperatorDelayed *)op;

	delay = cdn_stack_pop (stack);

	t = cdn_variable_get_value (d->priv->tvar);

	if (fabs (delay) < 1e-13)
	{
		evaluate_on_stack (d,
		                   t,
		                   d->priv->expression,
		                   stack);

		return;
	}

	td = t - delay;

	if (d->priv->first_last_t || t < d->priv->last_t)
	{
		d->priv->last_t = t;
		d->priv->first_last_t = FALSE;
	}

	h = d->priv->history.first;

	if (!h || h->t > td)
	{
		if (d->priv->initial_value)
		{
			// Generate dynamically
			evaluate_on_stack (d,
			                   td,
			                   d->priv->initial_value,
			                   stack);
		}
		else
		{
			cdn_stack_pushni (stack,
			                  0,
			                  d->priv->push_dims[0] *
			                  d->priv->push_dims[1]);
		}

		return;
	}

	while (h->next && h->next->t < td)
	{
		h = h->next;
	}

	if (!h->next)
	{
		if (fabs (h->t - td) > 1e-13)
		{
			if (d->priv->initial_value)
			{
				// Generate dynamically
				evaluate_on_stack (d,
				                   td,
				                   d->priv->initial_value,
				                   stack);
			}
			else
			{
				g_warning ("Needed history in the future, "
				           "which I don't have...");

				// Note that this will not really be the right
				// results but at least it doesn't crash
				evaluate_on_stack (d,
				                   td,
				                   d->priv->expression,
				                   stack);
			}

			return;
		}
	}
	else
	{
		// Interpolate value between h and h->next
		gdouble factor;
		gint i;

		if (fabs (td - h->t) < 1e-13)
		{
			factor = 0;
		}
		else
		{
			factor = (td - h->t) / (h->next->t - h->t);
		}

		for (i = 0; i < d->priv->push_dims[0] *
		                d->priv->push_dims[1]; ++i)
		{
			cdn_stack_push (stack,
			                h->v[i] +
			                factor * (h->next->v[i] - h->v[i]));
		}
	}
}

static void
history_free (HistoryList *history)
{
	HistoryItem *item;

	item = history->first;

	while (item)
	{
		HistoryItem *next;
		next = item->next;

		g_free (item->v);
		g_slice_free (HistoryItem, item);
		item = next;
	}

	history->first = history->last = NULL;
	history->size = 0;
}

static void
cdn_operator_delayed_finalize (GObject *object)
{
	CdnOperatorDelayed *delayed;

	delayed = CDN_OPERATOR_DELAYED (object);

	history_free (&delayed->priv->history);
	history_free (&delayed->priv->history_pool);

	if (delayed->priv->expression)
	{
		g_object_unref (delayed->priv->expression);
	}

	if (delayed->priv->initial_value)
	{
		g_object_unref (delayed->priv->initial_value);
	}

	G_OBJECT_CLASS (cdn_operator_delayed_parent_class)->finalize (object);
}

static void
cdn_operator_delayed_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
	switch (prop_id)
	{
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
cdn_operator_delayed_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
	CdnOperatorDelayed *self = CDN_OPERATOR_DELAYED (object);

	switch (prop_id)
	{
		case PROP_EXPRESSION:
			g_value_set_object (value, self->priv->expression);
			break;
		case PROP_INITIAL_VALUE:
			g_value_set_object (value, self->priv->initial_value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static gboolean
cdn_operator_delayed_equal (CdnOperator *op,
                            CdnOperator *other)
{
	CdnOperatorDelayed *delayed;
	CdnOperatorDelayed *odel;

	if (!CDN_IS_OPERATOR_DELAYED (other))
	{
		return FALSE;
	}

	delayed = CDN_OPERATOR_DELAYED (op);
	odel = CDN_OPERATOR_DELAYED (other);

	if ((delayed->priv->initial_value != NULL) !=
	    (odel->priv->initial_value != NULL))
	{
		return FALSE;
	}

	if (!cdn_expression_equal (delayed->priv->expression,
	                           odel->priv->expression))
	{
		return FALSE;
	}

	if (delayed->priv->initial_value &&
	    !cdn_expression_equal (delayed->priv->expression,
	                           odel->priv->expression))
	{
		return FALSE;
	}

	return TRUE;
}

static void
cdn_operator_delayed_step (CdnOperator *op,
                           gdouble      t,
                           gdouble      timestep)
{
	CdnOperatorDelayed *d;
	HistoryItem *current;
	gdouble const *v;
	gint nd;
	gint numr;
	gint numc;

	// direct cast for efficiency
	d = (CdnOperatorDelayed *)op;

	// release ancient history
	history_to_pool (d, d->priv->last_t);

	// append current value to history
	current = pool_to_history (d, 1);

	current->t = t;

	if (t != d->priv->eval_at_t)
	{
		cdn_expression_reset_cache (d->priv->expression);
	}

	v = cdn_expression_evaluate_values (d->priv->expression,
	                                    &numr,
	                                    &numc);

	d->priv->eval_at_t = t;

	nd = numr * numc;

	if (current->v == NULL)
	{
		current->v = g_new (gdouble, nd);
	}

	memcpy (current->v, v, sizeof (gdouble) * nd);
	d->priv->first_last_t = TRUE;
}

static void
cdn_operator_delayed_initialize_integrate (CdnOperator   *op,
                                           CdnIntegrator *integrator)
{
	CdnOperatorDelayed *d;

	d = CDN_OPERATOR_DELAYED (op);
	d->priv->tvar = cdn_object_get_variable (CDN_OBJECT (integrator), "t");
}

static CdnStackManipulation const *
cdn_operator_delayed_get_stack_manipulation (CdnOperator *op)
{
	CdnStackManipulation const *par;
	CdnOperatorDelayed *d;
	CdnOperatorClass *cls;

	d = (CdnOperatorDelayed *)op;
	cls = CDN_OPERATOR_CLASS (cdn_operator_delayed_parent_class);

	par = cls->get_stack_manipulation (op);

	d->priv->smanip.pop_dims = par->pop_dims;
	d->priv->smanip.num_pop = par->num_pop;

	return &d->priv->smanip;
}

static void
cdn_operator_delayed_class_init (CdnOperatorDelayedClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	CdnOperatorClass *op_class = CDN_OPERATOR_CLASS (klass);

	object_class->finalize = cdn_operator_delayed_finalize;

	object_class->get_property = cdn_operator_delayed_get_property;
	object_class->set_property = cdn_operator_delayed_set_property;

	op_class->get_name = cdn_operator_delayed_get_name;
	op_class->execute = cdn_operator_delayed_execute;
	op_class->initialize = cdn_operator_delayed_initialize;
	op_class->equal = cdn_operator_delayed_equal;
	op_class->step = cdn_operator_delayed_step;
	op_class->initialize_integrate = cdn_operator_delayed_initialize_integrate;
	op_class->get_stack_manipulation = cdn_operator_delayed_get_stack_manipulation;

	g_type_class_add_private (object_class, sizeof(CdnOperatorDelayedPrivate));

	g_object_class_install_property (object_class,
	                                 PROP_EXPRESSION,
	                                 g_param_spec_object ("expression",
	                                                      "Expression",
	                                                      "Expression",
	                                                      CDN_TYPE_EXPRESSION,
	                                                      G_PARAM_READABLE));

	g_object_class_install_property (object_class,
	                                 PROP_INITIAL_VALUE,
	                                 g_param_spec_object ("initial-value",
	                                                      "Initial Value",
	                                                      "Initial value",
	                                                      CDN_TYPE_EXPRESSION,
	                                                      G_PARAM_READABLE));
}

static void
cdn_operator_delayed_init (CdnOperatorDelayed *self)
{
	self->priv = CDN_OPERATOR_DELAYED_GET_PRIVATE (self);

	self->priv->smanip.push_dims = self->priv->push_dims;
	self->priv->smanip.num_push = 1;
}

CdnOperatorDelayed *
cdn_operator_delayed_new ()
{
	return g_object_new (CDN_TYPE_OPERATOR_DELAYED, NULL);
}

/**
 * cdn_operator_delayed_get_expression:
 * @delayed: A #CdnOperatorDelayed
 *
 * Get the expression to be delayed.
 *
 * Returns: (transfer none): A #CdnExpression
 *
 **/
CdnExpression *
cdn_operator_delayed_get_expression (CdnOperatorDelayed *delayed)
{
	g_return_val_if_fail (CDN_IS_OPERATOR_DELAYED (delayed), NULL);

	return delayed->priv->expression;
}

/**
 * cdn_operator_delayed_get_initial_value:
 * @delayed: A #CdnOperatorDelayed
 *
 * Get the initial value of the expression to be delayed.
 *
 * Returns: (transfer none): A #CdnExpression
 *
 **/
CdnExpression *
cdn_operator_delayed_get_initial_value (CdnOperatorDelayed *delayed)
{
	g_return_val_if_fail (CDN_IS_OPERATOR_DELAYED (delayed), NULL);

	return delayed->priv->initial_value;
}
