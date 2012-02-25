/*
 * cdn-function-polynomial.c
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

#include "cdn-function-polynomial.h"

/**
 * SECTION:cdn-function-polynomial
 * @short_description: Custom user defined piecewise polynomial
 *
 * This class provides a specialized custom user function which defines
 * and evaluates piecewise polynomials. The piece polynomials can be
 * specified in terms of the interval in which they are evaluated and
 * the polynomial coefficients.
 *
 */

#define CDN_FUNCTION_POLYNOMIAL_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), CDN_TYPE_FUNCTION_POLYNOMIAL, CdnFunctionPolynomialPrivate))

/* signals */
enum
{
	PIECE_ADDED,
	PIECE_REMOVED,
	NUM_SIGNALS
};

struct _CdnFunctionPolynomialPrivate
{
	GSList *pieces;

	CdnVariable *x;
	CdnVariable *order;

	GSList *dx;
};

G_DEFINE_TYPE (CdnFunctionPolynomial, cdn_function_polynomial, CDN_TYPE_FUNCTION)

static guint signals[NUM_SIGNALS] = {0,};

static gint
compare_pieces (CdnFunctionPolynomialPiece *p1,
                CdnFunctionPolynomialPiece *p2)
{
	gdouble b1;
	gdouble b2;

	b1 = cdn_function_polynomial_piece_get_begin (p1);
	b2 = cdn_function_polynomial_piece_get_begin (p2);

	return b1 < b2 ? -1 : (b2 < b1 ? 1 : 0);
}

static void
on_piece_range_changed (CdnFunctionPolynomialPiece *piece,
                        GParamSpec                 *spec,
                        CdnFunctionPolynomial      *function)
{
	function->priv->pieces =
		g_slist_sort (function->priv->pieces,
		              (GCompareFunc)compare_pieces);
}

static void
remove_piece (CdnFunctionPolynomial      *function,
              CdnFunctionPolynomialPiece *piece)
{
	g_signal_handlers_disconnect_by_func (piece,
	                                      on_piece_range_changed,
	                                      function);

	g_signal_emit (function, signals[PIECE_REMOVED], 0, piece);
	g_object_unref (piece);
}

static void
cdn_function_polynomial_finalize (GObject *object)
{
	CdnFunctionPolynomial *function = CDN_FUNCTION_POLYNOMIAL (object);

	cdn_function_polynomial_clear_pieces (function);

	G_OBJECT_CLASS (cdn_function_polynomial_parent_class)->finalize (object);
}

static void
cdn_function_polynomial_evaluate_impl (CdnFunction *function,
                                       CdnStack    *stack)
{
	CdnFunctionPolynomial *pol = CDN_FUNCTION_POLYNOMIAL (function);
	GSList *item;

	/* Evaluate the polynomial at 't' */
	gdouble val = cdn_variable_get_value (pol->priv->x);
	gboolean found = FALSE;
	guint num = 0;
	gdouble ret = 0;
	gdouble mult = 1;

	/* Evaluate multiplier first */
	for (item = pol->priv->dx; item; item = g_slist_next (item))
	{
		CdnFunctionArgument *arg = item->data;
		CdnVariable *v;

		v = _cdn_function_argument_get_variable (arg);
		mult *= cdn_variable_get_value (v);
	}

	for (item = pol->priv->pieces; item; item = g_slist_next (item))
	{
		CdnFunctionPolynomialPiece *piece = item->data;

		gdouble begin = cdn_function_polynomial_piece_get_begin (piece);
		gdouble end = cdn_function_polynomial_piece_get_end (piece);

		if (val >= begin && val < end)
		{
			ret += cdn_function_polynomial_piece_evaluate (piece,
			                                               val - begin);

			++num;
			found = TRUE;
		}
		else if (found)
		{
			break;
		}
	}

	cdn_stack_push (stack, num != 0 ? ret / num * mult : 0);
}

static void
cdn_function_polynomial_copy_impl (CdnObject *object,
                                   CdnObject *source)
{
	/* Chain up */
	if (CDN_OBJECT_CLASS (cdn_function_polynomial_parent_class)->copy != NULL)
	{
		CDN_OBJECT_CLASS (cdn_function_polynomial_parent_class)->copy (object, source);
	}

	/* Copy polynomial definitions */
	GSList *item;

	CdnFunctionPolynomial *source_polynomial = CDN_FUNCTION_POLYNOMIAL (source);
	CdnFunctionPolynomial *target = CDN_FUNCTION_POLYNOMIAL (object);

	for (item = source_polynomial->priv->pieces; item; item = g_slist_next (item))
	{
		CdnFunctionPolynomialPiece *piece;

		piece = cdn_function_polynomial_piece_copy (item->data);

		target->priv->pieces =
			g_slist_prepend (target->priv->pieces,
			                 piece);
	}

	target->priv->pieces = g_slist_reverse (target->priv->pieces);
}

static void
cdn_function_polynomial_clear_impl (CdnObject *object)
{
	/* Chain up */
	if (CDN_OBJECT_CLASS (cdn_function_polynomial_parent_class)->clear != NULL)
	{
		CDN_OBJECT_CLASS (cdn_function_polynomial_parent_class)->clear (object);
	}

	cdn_function_polynomial_clear_pieces (CDN_FUNCTION_POLYNOMIAL (object));
}

static void
cdn_function_polynomial_get_dimension_impl (CdnFunction *function,
                                            gint        *numr,
                                            gint        *numc)
{
	*numr = 1;
	*numc = 1;
}

static CdnFunction *
cdn_function_polynomial_get_derivative_impl (CdnFunction *function,
                                             gint         order,
                                             GList       *towards)
{
	CdnFunctionPolynomial *self;
	CdnFunctionPolynomial *ret;
	GSList *item;
	gint i;
	GString *dx;

	if (order == 0)
	{
		return CDN_FUNCTION (cdn_object_copy (CDN_OBJECT (CDN_FUNCTION (function))));
	}

	self = CDN_FUNCTION_POLYNOMIAL (function);

	ret = cdn_function_polynomial_new (cdn_object_get_id (CDN_OBJECT (function)));

	// Add dx arguments
	dx = g_string_new ("dx");

	for (item = self->priv->dx; item; item = g_slist_next (item))
	{
		CdnFunctionArgument *arg;

		arg = cdn_function_argument_copy (item->data);

		cdn_function_add_argument (CDN_FUNCTION (ret), arg);
		g_string_prepend_c (dx, 'd');

		ret->priv->dx = g_slist_prepend (self->priv->dx, arg);
	}

	for (i = 0; i < order; ++i)
	{
		CdnFunctionArgument *arg;
		CdnExpression *expr;

		expr = cdn_expression_new ("1");
		cdn_expression_compile (expr, NULL, NULL);

		arg = cdn_function_argument_new (dx->str,
		                                 TRUE,
		                                 expr);

		cdn_function_add_argument (CDN_FUNCTION (ret), arg);
		g_string_prepend_c (dx, 'd');

		ret->priv->dx = g_slist_prepend (self->priv->dx, arg);
	}

	ret->priv->dx = g_slist_reverse (ret->priv->dx);

	g_string_free (dx, TRUE);

	// Derive all the pieces
	for (item = self->priv->pieces; item; item = g_slist_next (item))
	{
		CdnFunctionPolynomialPiece *piece;

		piece = cdn_function_polynomial_piece_get_derivative (item->data,
		                                                      order);

		ret->priv->pieces = g_slist_prepend (ret->priv->pieces, piece);
	}

	ret->priv->pieces = g_slist_reverse (ret->priv->pieces);
	return CDN_FUNCTION (ret);
}

static void
cdn_function_polynomial_class_init (CdnFunctionPolynomialClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	CdnFunctionClass *function_class = CDN_FUNCTION_CLASS (klass);
	CdnObjectClass *cdn_object_class = CDN_OBJECT_CLASS (klass);

	function_class->evaluate = cdn_function_polynomial_evaluate_impl;
	function_class->get_dimension = cdn_function_polynomial_get_dimension_impl;
	function_class->get_derivative = cdn_function_polynomial_get_derivative_impl;

	cdn_object_class->copy = cdn_function_polynomial_copy_impl;
	cdn_object_class->clear = cdn_function_polynomial_clear_impl;

	object_class->finalize = cdn_function_polynomial_finalize;

	/**
	 * CdnFunctionPolynomial::piece-added:
	 * @object: a #CdnFunctionPolynomial
	 * @piece: a #CdnFunctionPolynomialPiece
	 *
	 * Emitted when a new #CdnFunctionPolynomialPiece is added to the
	 * piecewise polynomial function.
	 *
	 **/
	signals[PIECE_ADDED] =
		g_signal_new ("piece-added",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (CdnFunctionPolynomialClass, piece_added),
		              NULL,
		              NULL,
		              g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE,
		              1,
		              CDN_TYPE_FUNCTION_POLYNOMIAL_PIECE);

	/**
	 * CdnFunctionPolynomial::piece-removed:
	 * @object: a #CdnFunctionPolynomial
	 * @piece: a #CdnFunctionPolynomialPiece
	 *
	 * Emitted when a #CdnFunctionPolynomialPiece is removed from the
	 * piecewise polynomial function.
	 *
	 **/
	signals[PIECE_REMOVED] =
		g_signal_new ("piece-removed",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (CdnFunctionPolynomialClass, piece_removed),
		              NULL,
		              NULL,
		              g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE,
		              1,
		              CDN_TYPE_FUNCTION_POLYNOMIAL_PIECE);

	g_type_class_add_private (object_class, sizeof(CdnFunctionPolynomialPrivate));
}

static void
cdn_function_polynomial_init (CdnFunctionPolynomial *self)
{
	self->priv = CDN_FUNCTION_POLYNOMIAL_GET_PRIVATE (self);

	/* Add 't' argument */
	cdn_function_add_argument (CDN_FUNCTION (self),
	                           cdn_function_argument_new ("x", TRUE, NULL));

	self->priv->x = cdn_object_get_variable (CDN_OBJECT (self), "x");
}

/**
 * cdn_function_polynomial_new:
 * @name: The function name
 *
 * Create a new polynomial function. This is a special kind of user function
 * which calculates a piecewise polynomial. The function is called with
 * one argument (the point at which to interpolate).
 *
 * Returns: A #CdnFunctionPolynomial
 *
 **/
CdnFunctionPolynomial *
cdn_function_polynomial_new (const gchar *name)
{
	return g_object_new (CDN_TYPE_FUNCTION_POLYNOMIAL, "id", name, NULL);
}

/**
 * cdn_function_polynomial_add:
 * @function: A #CdnFunctionPolynomial
 * @piece: A #CdnFunctionPolynomialPiece
 *
 * Add a polynomial piece.
 *
 * Returns: %TRUE if the piece could be successfully added, %FALSE otherwise
 *
 **/
gboolean
cdn_function_polynomial_add (CdnFunctionPolynomial      *function,
                             CdnFunctionPolynomialPiece *piece)
{
	g_return_val_if_fail (CDN_IS_FUNCTION_POLYNOMIAL (function), FALSE);
	g_return_val_if_fail (piece != NULL, FALSE);

	if (g_slist_find (function->priv->pieces, piece))
	{
		return FALSE;
	}

	function->priv->pieces = g_slist_insert_sorted (function->priv->pieces,
	                                                g_object_ref_sink (piece),
	                                                (GCompareFunc)compare_pieces);

	g_signal_connect (piece,
	                  "notify::begin",
	                  G_CALLBACK (on_piece_range_changed),
	                  function);

	g_signal_emit (function, signals[PIECE_ADDED], 0, piece);
	return TRUE;
}

/**
 * cdn_function_polynomial_remove:
 * @function: A #CdnFunctionPolynomial
 * @piece: A #CdnFunctionPolynomialPiece
 *
 * Remove a polynomial piece.
 *
 * Returns: %TRUE if the piece could be successfully removed, %FALSE otherwise
 *
 **/
gboolean
cdn_function_polynomial_remove (CdnFunctionPolynomial      *function,
                                CdnFunctionPolynomialPiece *piece)
{
	GSList *item;

	g_return_val_if_fail (CDN_IS_FUNCTION_POLYNOMIAL (function), FALSE);
	g_return_val_if_fail (piece != NULL, FALSE);

	item = g_slist_find (function->priv->pieces, piece);

	if (item)
	{
		function->priv->pieces = g_slist_delete_link (function->priv->pieces,
		                                              item);

		remove_piece (function, piece);
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/**
 * cdn_function_polynomial_clear_pieces:
 * @function: A #CdnFunctionPolynomial
 *
 * Remove all the polynomial pieces.
 *
 **/
void
cdn_function_polynomial_clear_pieces (CdnFunctionPolynomial *function)
{
	g_return_if_fail (CDN_IS_FUNCTION_POLYNOMIAL (function));

	GSList *item;

	for (item = function->priv->pieces; item; item = g_slist_next (item))
	{
		remove_piece (function, item->data);
	}

	g_slist_free (function->priv->pieces);
	function->priv->pieces = NULL;
}

/**
 * cdn_function_polynomial_get_pieces:
 * @function: A #CdnFunctionPolynomial
 *
 * Get a list of the polynomials which make up the function. This returns
 * the internally used list which should not be modified or freed.
 *
 * Returns: (element-type CdnFunctionPolynomialPiece) (transfer none):
 *          A #GSList of #CdnFunctionPolynomialPiece
 *
 **/
const GSList *
cdn_function_polynomial_get_pieces (CdnFunctionPolynomial *function)
{
	g_return_val_if_fail (CDN_IS_FUNCTION_POLYNOMIAL (function), NULL);

	return function->priv->pieces;
}

