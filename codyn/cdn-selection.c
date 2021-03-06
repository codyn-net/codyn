/*
 * cdn-selection.c
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

#include "cdn-selection.h"
#include "cdn-expansion.h"

/**
 * CdnSelection:
 *
 * Selection object.
 *
 * #CdnSelection is an object which contains the result of a selector.
 * It contains both the selected object and an expansion context which was
 * the result of how the object got selected.
 */

struct _CdnSelection
{
	gpointer  object;
	CdnExpansionContext *context;
	guint ref_count;
	gchar *override_name;
};

GType
cdn_selection_get_type (void)
{
	static volatile gsize g_define_type_id__volatile = 0;

	if (g_once_init_enter (&g_define_type_id__volatile))
	{
		GType g_define_type_id;

		g_define_type_id =
			g_boxed_type_register_static (g_intern_static_string ("CdnSelection"),
			                              (GBoxedCopyFunc)cdn_selection_ref,
			                              (GBoxedFreeFunc)cdn_selection_unref);

		g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
	}

	return g_define_type_id__volatile;
}

/**
 * cdn_selection_unref:
 * @selection: the #CdnSelection
 *
 * Decrease the reference count on the selection. If the reference count drops
 * to 0, then the selection will be destroyed and is no longer valid.
 *
 */
void
cdn_selection_unref (CdnSelection *selection)
{
	if (!g_atomic_int_dec_and_test ((gint *)&selection->ref_count))
	{
		return;
	}

	if (selection->object)
	{
		g_object_unref (selection->object);
	}

	if (selection->context)
	{
		cdn_expansion_context_unref (selection->context);
	}

	g_free (selection->override_name);

	g_slice_free (CdnSelection, selection);
}

/**
 * cdn_selection_ref:
 * @selection: the #CdnSelection
 *
 * Increase the reference count on the selection
 *
 * Returns: (transfer full): @selection
 *
 */
CdnSelection *
cdn_selection_ref (CdnSelection *selection)
{
	g_atomic_int_inc ((gint *)&selection->ref_count);
	return selection;
}

/**
 * cdn_selection_new:
 * @object: (type GObject*): The object
 * @context: (transfer none) (allow-none): The expansion context
 *
 * Create a new selection.
 *
 * Returns: (transfer full): A #CdnSelection
 *
 **/
CdnSelection *
cdn_selection_new (gpointer             object,
                   CdnExpansionContext *context)
{
	CdnSelection *ret;

	ret = g_slice_new0 (CdnSelection);
	ret->ref_count = 1;

	ret->object = object ? g_object_ref (object) : NULL;

	if (context == NULL)
	{
		ret->context = cdn_expansion_context_new (NULL);
	}
	else
	{
		ret->context = cdn_expansion_context_ref (context);
	}

	return ret;
}

/**
 * cdn_selection_copy:
 * @selection: A #CdnSelection
 *
 * Copy a selection.
 *
 * Returns: (transfer full): A #CdnSelection
 *
 **/
CdnSelection *
cdn_selection_copy (CdnSelection *selection)
{
	if (selection == NULL)
	{
		return NULL;
	}

	return cdn_selection_new (selection->object,
	                          cdn_expansion_context_new_unreffed (selection->context));
}

/**
 * cdn_selection_get_object:
 * @selection: A #CdnSelection
 *
 * Get the object being selected.
 *
 * Returns: (transfer none) (type GObject*): The object being selected
 *
 **/
gpointer
cdn_selection_get_object (CdnSelection *selection)
{
	g_return_val_if_fail (selection != NULL, NULL);

	return selection->object;
}

/**
 * cdn_selection_set_object:
 * @selection: A #CdnSelection
 * @object: the object
 *
 * Set the object being selected.
 *
 */
void
cdn_selection_set_object (CdnSelection *selection,
                          gpointer      object)
{
	g_return_if_fail (selection != NULL);
	g_return_if_fail (object == NULL || G_IS_OBJECT (object));

	if (selection->object)
	{
		g_object_unref (selection->object);
		selection->object = NULL;
	}

	if (object)
	{
		selection->object = g_object_ref (object);
	}
}

/**
 * cdn_selection_get_context:
 * @selection: A #CdnSelection
 *
 * Get the selection context.
 *
 * Returns: (transfer none): a #CdnExpansionContext
 *
 **/
CdnExpansionContext *
cdn_selection_get_context (CdnSelection *selection)
{
	g_return_val_if_fail (selection != NULL, NULL);

	return selection->context;
}

/**
 * cdn_selection_set_context:
 * @selection: (transfer none): a #CdnSelection.
 *
 * Set the selection context.
 *
 **/
void
cdn_selection_set_context (CdnSelection        *selection,
                           CdnExpansionContext *context)
{
	g_return_if_fail (selection != NULL);

	cdn_expansion_context_unref (selection->context);
	selection->context = cdn_expansion_context_ref (context);
}

gchar const *
_cdn_selection_get_override_name (CdnSelection *selection)
{
	g_return_val_if_fail (selection != NULL, NULL);

	return selection->override_name;
}

void
_cdn_selection_set_override_name (CdnSelection *selection,
                                  gchar const  *name)
{
	g_return_if_fail (selection != NULL);

	g_free (selection->override_name);
	selection->override_name = g_strdup (name);
}

