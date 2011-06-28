/*
 * cpg-attribute.c
 * This file is part of cpg-network
 *
 * Copyright (C) 2011 - Jesse van den Kieboom
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

#include "cpg-attribute.h"

#define CPG_ATTRIBUTE_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), CPG_TYPE_ATTRIBUTE, CpgAttributePrivate))

struct _CpgAttributePrivate
{
	gchar *id;
	GSList *arguments;
	gint num_arguments;
};

G_DEFINE_TYPE (CpgAttribute, cpg_attribute, G_TYPE_OBJECT)

enum
{
	PROP_0,
	PROP_ID
};

static void
cpg_attribute_finalize (GObject *object)
{
	CpgAttribute *self = CPG_ATTRIBUTE (object);

	g_free (self->priv->id);

	cpg_attribute_set_arguments (self, NULL);

	G_OBJECT_CLASS (cpg_attribute_parent_class)->finalize (object);
}

static void
cpg_attribute_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	CpgAttribute *self = CPG_ATTRIBUTE (object);

	switch (prop_id)
	{
		case PROP_ID:
			self->priv->id = g_value_dup_string (value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
cpg_attribute_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	CpgAttribute *self = CPG_ATTRIBUTE (object);

	switch (prop_id)
	{
		case PROP_ID:
			g_value_set_string (value, self->priv->id);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
cpg_attribute_class_init (CpgAttributeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	object_class->finalize = cpg_attribute_finalize;

	object_class->get_property = cpg_attribute_get_property;
	object_class->set_property = cpg_attribute_set_property;

	g_type_class_add_private (object_class, sizeof(CpgAttributePrivate));

	g_object_class_install_property (object_class,
	                                 PROP_ID,
	                                 g_param_spec_string ("id",
	                                                      "Id",
	                                                      "Id",
	                                                      NULL,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
cpg_attribute_init (CpgAttribute *self)
{
	self->priv = CPG_ATTRIBUTE_GET_PRIVATE (self);
}

CpgAttribute *
cpg_attribute_new (gchar const *id)
{
	CpgAttribute *ret;

	ret = g_object_new (CPG_TYPE_ATTRIBUTE,
	                    "id", id,
	                     NULL);

	return ret;
}

CpgAttribute *
cpg_attribute_newv (gchar const *id,
                    ...)
{
	CpgAttribute *ret;
	va_list ap;
	GObject *obj;

	ret = g_object_new (CPG_TYPE_ATTRIBUTE,
	                    "id", id,
	                     NULL);

	va_start (ap, id);

	while ((obj = G_OBJECT (va_arg (ap, GObject *))) != NULL)
	{
		ret->priv->arguments =
			g_slist_prepend (ret->priv->arguments,
			                 g_object_ref (obj));

		++ret->priv->num_arguments;
	}

	ret->priv->arguments = g_slist_reverse (ret->priv->arguments);

	va_end (ap);

	return ret;
}


void
cpg_attribute_set_arguments (CpgAttribute *attribute,
                             GSList       *arguments)
{
	g_return_if_fail (CPG_IS_ATTRIBUTE (attribute));

	g_slist_foreach (attribute->priv->arguments, (GFunc)g_object_unref, NULL);
	g_slist_free (attribute->priv->arguments);

	attribute->priv->arguments = NULL;
	attribute->priv->num_arguments = 0;

	while (arguments)
	{
		attribute->priv->arguments =
			g_slist_prepend (attribute->priv->arguments,
			                 g_object_ref (arguments->data));

		++attribute->priv->num_arguments;
		arguments = g_slist_next (arguments);
	}

	attribute->priv->arguments = g_slist_reverse (attribute->priv->arguments);
}

gchar const *
cpg_attribute_get_id (CpgAttribute *attr)
{
	g_return_val_if_fail (CPG_IS_ATTRIBUTE (attr), NULL);

	return attr->priv->id;
}

GSList *
cpg_attribute_get_arguments (CpgAttribute *attr)
{
	g_return_val_if_fail (CPG_IS_ATTRIBUTE (attr), NULL);

	return attr->priv->arguments;
}

gpointer
cpg_attribute_get_argument (CpgAttribute *attr,
                            gint          i)
{
	g_return_val_if_fail (CPG_IS_ATTRIBUTE (attr), NULL);

	return g_slist_nth_data (attr->priv->arguments, i);
}

gint
cpg_attribute_num_arguments (CpgAttribute *attr)
{
	g_return_val_if_fail (CPG_IS_ATTRIBUTE (attr), 0);

	return attr->priv->num_arguments;
}
