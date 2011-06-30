/*
 * cpg-expansion.h
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

#ifndef __CPG_EXPANSION_H__
#define __CPG_EXPANSION_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define CPG_TYPE_EXPANSION		(cpg_expansion_get_type ())
#define CPG_EXPANSION(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), CPG_TYPE_EXPANSION, CpgExpansion))
#define CPG_EXPANSION_CONST(obj)	(G_TYPE_CHECK_INSTANCE_CAST ((obj), CPG_TYPE_EXPANSION, CpgExpansion const))
#define CPG_EXPANSION_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), CPG_TYPE_EXPANSION, CpgExpansionClass))
#define CPG_IS_EXPANSION(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), CPG_TYPE_EXPANSION))
#define CPG_IS_EXPANSION_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), CPG_TYPE_EXPANSION))
#define CPG_EXPANSION_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), CPG_TYPE_EXPANSION, CpgExpansionClass))

typedef struct _CpgExpansion		CpgExpansion;
typedef struct _CpgExpansionClass	CpgExpansionClass;
typedef struct _CpgExpansionPrivate	CpgExpansionPrivate;

struct _CpgExpansion
{
	GObject parent;

	CpgExpansionPrivate *priv;
};

struct _CpgExpansionClass
{
	GObjectClass parent_class;
};

GType         cpg_expansion_get_type         (void) G_GNUC_CONST;

CpgExpansion *cpg_expansion_new              (gchar const * const    *items);
CpgExpansion *cpg_expansion_new_one          (gchar const            *item);

CpgExpansion *cpg_expansion_copy             (CpgExpansion           *id);

gint          cpg_expansion_num              (CpgExpansion           *id);

gchar const  *cpg_expansion_get              (CpgExpansion           *id,
                                              gint                    idx);

gint          cpg_expansion_get_index        (CpgExpansion           *id,
                                              gint                    idx);

void          cpg_expansion_set_index        (CpgExpansion           *id,
                                              gint                    idx,
                                              gint                    val);

void          cpg_expansion_add              (CpgExpansion           *id,
                                              gchar const            *item);

void          cpg_expansion_set              (CpgExpansion           *id,
                                              gint                    idx,
                                              gchar const            *val);

gchar        *cpg_expansions_expand          (GSList                 *expansions,
                                              gchar const            *s,
                                              GRegex                 *regex);

void          cpg_expansions_annotate_indices (GSList                *expansions);

G_END_DECLS

#endif /* __CPG_EXPANSION_H__ */
