#include "cpg-embedded-string.h"
#include <string.h>

#define CPG_EMBEDDED_STRING_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), CPG_TYPE_EMBEDDED_STRING, CpgEmbeddedStringPrivate))

typedef struct
{
	CpgEmbeddedStringNodeType type;

	gchar *text;

	struct
	{
		gint parent;
		gint idx;
	} ref;

	GSList *nodes;
} Node;

struct _CpgEmbeddedStringPrivate
{
	GSList *stack;
	gchar *cached;
};

G_DEFINE_TYPE (CpgEmbeddedString, cpg_embedded_string, G_TYPE_OBJECT)

static Node *
node_new (CpgEmbeddedStringNodeType  type,
          gchar const       *text,
          gint               parent,
          gint               idx)
{
	Node *ret;

	ret = g_slice_new0 (Node);

	ret->type = type;
	ret->text = g_strdup (text ? text : "");

	ret->ref.parent = parent;
	ret->ref.idx = idx;

	return ret;
}

static void
node_free (Node *node)
{
	g_slist_foreach (node->nodes, (GFunc)node_free, NULL);
	g_slist_free (node->nodes);
	g_free (node->text);

	g_slice_free (Node, node);
}

static void
cpg_embedded_string_finalize (GObject *object)
{
	CpgEmbeddedString *s;

	s = CPG_EMBEDDED_STRING (object);

	while (s->priv->stack)
	{
		node_free (s->priv->stack->data);
		s->priv->stack = g_slist_delete_link (s->priv->stack,
		                                      s->priv->stack);
	}

	g_free (s->priv->cached);

	G_OBJECT_CLASS (cpg_embedded_string_parent_class)->finalize (object);
}

static void
cpg_embedded_string_class_init (CpgEmbeddedStringClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = cpg_embedded_string_finalize;

	g_type_class_add_private (object_class, sizeof (CpgEmbeddedStringPrivate));
}

static void
cpg_embedded_string_init (CpgEmbeddedString *self)
{
	self->priv = CPG_EMBEDDED_STRING_GET_PRIVATE (self);

	cpg_embedded_string_push (self, CPG_EMBEDDED_STRING_NODE_TEXT, 0);
}

CpgEmbeddedString *
cpg_embedded_string_new ()
{
	return g_object_new (CPG_TYPE_EMBEDDED_STRING, NULL);
}

CpgEmbeddedString *
cpg_embedded_string_new_from_string (gchar const *s)
{
	CpgEmbeddedString *ret;

	ret = g_object_new (CPG_TYPE_EMBEDDED_STRING, NULL);
	cpg_embedded_string_add_text (ret, s);

	return ret;
}

CpgEmbeddedString *
cpg_embedded_string_new_from_double (gdouble s)
{
	gchar buffer[G_ASCII_DTOSTR_BUF_SIZE];
	g_ascii_dtostr (buffer, G_ASCII_DTOSTR_BUF_SIZE, s);

	return cpg_embedded_string_new_from_string (buffer);
}

CpgEmbeddedString *
cpg_embedded_string_new_from_integer (gint s)
{
	gchar *ss;
	CpgEmbeddedString *ret;

	ss = g_strdup_printf ("%d", s);

	ret = cpg_embedded_string_new_from_string (ss);

	g_free (ss);
	return ret;
}

static Node *
add_node (CpgEmbeddedString *s,
          CpgEmbeddedStringNodeType type,
          gchar const       *text,
          gint               parent,
          gint               idx)
{
	Node *node;
	Node *par;

	node = node_new (type, text, parent, idx);

	if (s->priv->stack)
	{
		par = s->priv->stack->data;
		par->nodes = g_slist_prepend (par->nodes, node);
	}
	else
	{
		s->priv->stack = g_slist_prepend (NULL, node);
	}

	return node;
}

void
cpg_embedded_string_add_text (CpgEmbeddedString *s,
                              gchar const       *text)
{
	g_return_if_fail (CPG_IS_EMBEDDED_STRING (s));

	if (text == NULL)
	{
		return;
	}

	add_node (s, CPG_EMBEDDED_STRING_NODE_TEXT, text, 0, 0);
}

void
cpg_embedded_string_add_define (CpgEmbeddedString *s,
                                gchar const       *text)
{
	g_return_if_fail (CPG_IS_EMBEDDED_STRING (s));

	if (text == NULL)
	{
		return;
	}

	add_node (s, CPG_EMBEDDED_STRING_NODE_DEFINE, text, 0, 0);
}

void
cpg_embedded_string_add_reference (CpgEmbeddedString *s,
                                   gint               parent,
                                   gint               idx)
{
	g_return_if_fail (CPG_IS_EMBEDDED_STRING (s));

	add_node (s, CPG_EMBEDDED_STRING_NODE_REF, NULL, parent, idx);
}

CpgEmbeddedString *
cpg_embedded_string_push (CpgEmbeddedString         *s,
                          CpgEmbeddedStringNodeType  type,
                          gint                       num)
{
	Node *node;

	g_return_val_if_fail (CPG_IS_EMBEDDED_STRING (s), NULL);

	node = node_new (type, NULL, num, 0);

	s->priv->stack = g_slist_prepend (s->priv->stack,
	                                  node);

	return s;
}

CpgEmbeddedString *
cpg_embedded_string_pop (CpgEmbeddedString *s)
{
	Node *node;
	Node *parent;

	g_return_val_if_fail (CPG_IS_EMBEDDED_STRING (s), NULL);

	if (!s->priv->stack || !s->priv->stack->next)
	{
		return s;
	}

	node = s->priv->stack->data;
	s->priv->stack = g_slist_delete_link (s->priv->stack,
	                                      s->priv->stack);

	parent = s->priv->stack->data;

	parent->nodes = g_slist_prepend (parent->nodes,
	                                 node);

	return s;
}

static gchar *
resolve_indirection (CpgEmbeddedContext *context,
                     Node               *node,
                     gchar const        *s)
{
	gboolean isnum = TRUE;
	gchar const *ptr = s;

	while (*ptr)
	{
		if (!g_ascii_isdigit (*ptr))
		{
			isnum = FALSE;
			break;
		}

		++ptr;
	}

	if (isnum)
	{
		gint idx = (gint)g_ascii_strtoll (s, NULL, 10);

		return cpg_embedded_context_lookup_ref (context,
		                                        node->ref.parent,
		                                        idx);
	}
	else
	{
		return cpg_embedded_context_lookup_define (context, s);
	}
}

static gchar *
evaluate_node (Node *node,
               CpgEmbeddedContext *context)
{
	GString *ret;
	GSList *item;
	gchar *r = NULL;

	ret = g_string_new ("");

	for (item = node->nodes; item; item = g_slist_next (item))
	{
		g_string_prepend (ret,
		                  evaluate_node (item->data,
		                                 context));
	}

	switch (node->type)
	{
		case CPG_EMBEDDED_STRING_NODE_TEXT:
			g_string_prepend (ret, node->text);
			return g_string_free (ret, FALSE);
		break;
		case CPG_EMBEDDED_STRING_NODE_DEFINE:
			g_string_prepend (ret, node->text);

			if (context)
			{
				r = cpg_embedded_context_lookup_define (context,
				                                        ret->str);
			}
			else
			{
				g_string_prepend_c (ret, '@');
				return g_string_free (ret, FALSE);
			}
		break;
		case CPG_EMBEDDED_STRING_NODE_EQUATION:
			if (context)
			{
				r = cpg_embedded_context_calculate (context, ret->str);
			}
			else
			{
				return g_string_free (ret, FALSE);
			}
		break;
		case CPG_EMBEDDED_STRING_NODE_REF:
		{
			if (context)
			{
				r = cpg_embedded_context_lookup_ref (context,
				                                     node->ref.parent,
				                                     node->ref.idx);
			}
			else
			{
				gchar *n = g_strnfill (node->ref.parent, '@');
				gchar *ss = g_strdup_printf ("%s%d", n, node->ref.idx);
			
				g_string_prepend (ret, ss);

				g_free (n);
				g_free (ss);

				return g_string_free (ret, FALSE);
			}
		}
		break;
		case CPG_EMBEDDED_STRING_NODE_INDIRECTION:
			if (context)
			{
				r = resolve_indirection (context, node, ret->str);
			}
			else
			{
				return g_string_free (ret, FALSE);
			}
		break;
	}

	g_string_free (ret, TRUE);
	return r;
}

gchar const *
cpg_embedded_string_expand (CpgEmbeddedString  *s,
                            CpgEmbeddedContext *ctx)
{
	gchar *r;

	g_return_val_if_fail (CPG_IS_EMBEDDED_STRING (s), NULL);
	g_return_val_if_fail (ctx == NULL || CPG_IS_EMBEDDED_CONTEXT (ctx), NULL);

	if (!s->priv->stack)
	{
		r = g_strdup ("");
	}
	else
	{
		r = evaluate_node (s->priv->stack->data, ctx);
	}

	g_free (s->priv->cached);
	s->priv->cached = r;

	return s->priv->cached;
}

static GSList *
parse_expansion_range (gchar const *s,
                       gint         len)
{
	static GRegex *rangereg = NULL;
	GMatchInfo *info;
	gchar *id;
	GSList *ret = NULL;

	if (rangereg == NULL)
	{
		rangereg = g_regex_new ("([0-9]+)-([0-9]+)",
		                        G_REGEX_ANCHORED,
		                        G_REGEX_MATCH_ANCHORED,
		                        NULL);
	}

	id = g_strndup (s, len);

	if (g_regex_match (rangereg, id, 0, &info))
	{
		gchar *start = g_match_info_fetch (info, 1);
		gchar *end = g_match_info_fetch (info, 2);

		gint cstart = (gint)g_ascii_strtoll (start, NULL, 10);
		gint cend = (gint)g_ascii_strtoll (end, NULL, 10);

		while (TRUE)
		{
			gchar *it;

			it = g_strdup_printf ("%d", cstart);

			ret = g_slist_prepend (ret,
			                       cpg_expansion_new_one (it));

			g_free (it);

			if (cstart == cend)
			{
				break;
			}

			if (cstart > cend)
			{
				--cstart;
			}
			else
			{
				++cstart;
			}
		}

		g_free (start);
		g_free (end);

		g_match_info_free (info);
	}
	else
	{
		ret = g_slist_prepend (ret,
		                       cpg_expansion_new_one (id));
	}

	g_free (id);

	return g_slist_reverse (ret);
}

static void
expansions_add (GSList      *expansions,
                gchar const *s,
                gint         len,
                gboolean     prepend)
{
	gchar *ss;

	if (len < 0)
	{
		len = strlen (s);
	}

	if (len == 0)
	{
		return;
	}

	ss = g_strndup (s, len);

	while (expansions)
	{
		gchar *c;
		gchar const *cur = cpg_expansion_get (expansions->data, 0);

		if (prepend)
		{
			c = g_strconcat (ss, cur, NULL);
		}
		else
		{
			c = g_strconcat (cur, ss, NULL);
		}

		cpg_expansion_set (expansions->data, 0, c);
		g_free (c);

		expansions = g_slist_next (expansions);
	}

	g_free (ss);
}

static void
expansions_prepend (GSList      *expansions,
                    gchar const *s,
                    gint         len)
{
	expansions_add (expansions, s, len, TRUE);
}

static void
expansions_append (GSList      *expansions,
                   gchar const *s,
                   gint         len)
{
	expansions_add (expansions, s, len, FALSE);
}

static CpgExpansion *
expansion_concat (CpgExpansion *s1,
                  CpgExpansion *s2)
{
	CpgExpansion *copy;
	gchar *n0;
	gint i;

	copy = cpg_expansion_copy (s1);
	n0 = g_strconcat (cpg_expansion_get (s1, 0),
	                  cpg_expansion_get (s2, 0),
	                  NULL);

	cpg_expansion_set (copy, 0, n0);
	g_free (n0);

	for (i = 1; i < cpg_expansion_num (s2); ++i)
	{
		cpg_expansion_add (copy, cpg_expansion_get (s2, i));
	}

	return copy;
}

static GSList *
expansions_concat (GSList *s1,
                   GSList *s2)
{
	GSList *ret = NULL;

	if (s1 == NULL)
	{
		s1 = g_slist_prepend (s1, cpg_expansion_new (NULL));
	}

	while (s1)
	{
		GSList *item = s2;

		while (item)
		{
			ret = g_slist_prepend (ret,
			                       expansion_concat (s1->data,
			                                         item->data));

			if (s1->next)
			{
				item = g_slist_next (item);
			}
			else
			{
				cpg_expansion_free (item->data);
				item = g_slist_delete_link (item, item);
			}
		}

		cpg_expansion_free (s1->data);
		s1 = g_slist_delete_link (s1, s1);
	}

	return g_slist_reverse (ret);
}

static GSList *expand_id_recurse (gchar const **id, gchar const *endings);

static void
expansion_shift (CpgExpansion *expansion)
{
	gint i;

	cpg_expansion_add (expansion, "");

	for (i = cpg_expansion_num (expansion) - 2; i >= 0; --i)
	{
		cpg_expansion_set (expansion,
		                   i + 1,
		                   cpg_expansion_get (expansion, i));
	}
}

static GSList *
parse_expansion (gchar const **id)
{
	GSList *ret = NULL;

	while (**id)
	{
		GSList *items;
		GSList *it;

		items = expand_id_recurse (id, ",}");

		for (it = items; it; it = g_slist_next (it))
		{
			expansion_shift (it->data);
		}

		ret = g_slist_concat (ret, items);

		if (**id)
		{
			if (*((*id)++) == '}')
			{
				break;
			}
		}
	}

	return ret;
}

GSList *
expand_id_recurse (gchar const **id,
                   gchar const *endings)
{
	GSList *ret = NULL;
	gchar const *ptr = *id;

	while (**id && strchr (endings, **id) == NULL)
	{
		if (**id == '{')
		{
			GSList *ex;
			gint len = *id - ptr;

			++*id;

			/* Recursively parse the expansions */
			ex = parse_expansion (id);

			/* Prepend what we got till now */
			expansions_prepend (ex, ptr, len);

			/* Concatenate the expansions */
			ret = expansions_concat (ret, ex);
			ptr = *id;
		}
		else if (**id)
		{
			++*id;
		}
	}

	if (ptr != *id)
	{
		if (ret != NULL)
		{
			expansions_append (ret, ptr, *id - ptr);
		}
		else
		{
			ret = parse_expansion_range (ptr, *id - ptr);
		}
	}

	return ret;
}

GSList *
cpg_embedded_string_expand_multiple (CpgEmbeddedString  *s,
                                     CpgEmbeddedContext *ctx)
{
	gchar const *id;

	g_return_val_if_fail (CPG_IS_EMBEDDED_STRING (s), NULL);
	g_return_val_if_fail (ctx == NULL || CPG_IS_EMBEDDED_CONTEXT (ctx), NULL);

	id = cpg_embedded_string_expand (s, ctx);

	if (!*id)
	{
		CpgExpansion *ex = cpg_expansion_new_one ("");
		return g_slist_prepend (NULL, ex);
	}
	else
	{
		return expand_id_recurse (&id, "\0");
	}
}
