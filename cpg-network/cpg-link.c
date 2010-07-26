#include "cpg-link.h"
#include "cpg-compile-error.h"
#include "cpg-debug.h"
#include <string.h>

/**
 * SECTION:cpg-link
 * @short_description: Information transfer link
 *
 * A #CpgLink is a connection between two #CpgObject. The link defines actions
 * which consist of a target property in the object to which the link is
 * connected, and an expression by which this target property needs to be
 * updated.
 *
 * <refsect2 id="CpgLink-COPY">
 * <title>CpgLink Copy Semantics</title>
 * When a link is copied with #cpg_object_copy, the link actions are also
 * copied. However, the link #CpgLink:from and #CpgLink:to properties are
 * <emphasis>NOT</emphasis> copied, so that you are free to attach it to
 * two new objects.
 * </refsect2>
 */

#define CPG_LINK_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), CPG_TYPE_LINK, CpgLinkPrivate))

enum
{
	EXT_PROPERTY_ADDED,
	EXT_PROPERTY_REMOVED,
	NUM_EXT_SIGNALS
};

struct _CpgLinkPrivate
{
	// from and to objects
	CpgObject *from;
	CpgObject *to;

	// list of expressions to evaluate
	GSList *actions;

	guint ext_signals[NUM_EXT_SIGNALS];
};

/* Properties */
enum
{
	PROP_0,
	PROP_TO,
	PROP_FROM
};

/* Signals */
enum
{
	ACTION_ADDED,
	ACTION_REMOVED,
	NUM_SIGNALS
};

guint signals[NUM_SIGNALS] = {0,};

G_DEFINE_TYPE (CpgLink, cpg_link, CPG_TYPE_OBJECT)

static void
cpg_link_finalize (GObject *object)
{
	G_OBJECT_CLASS (cpg_link_parent_class)->finalize (object);
}

static void
cpg_link_get_property (GObject     *object,
                       guint        prop_id,
                       GValue      *value,
                       GParamSpec  *pspec)
{
	CpgLink *link = CPG_LINK (object);

	switch (prop_id)
	{
		case PROP_TO:
			g_value_set_object (value, link->priv->to);
		break;
		case PROP_FROM:
			g_value_set_object (value, link->priv->from);
		break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
update_action_property (CpgLink       *link,
                        CpgLinkAction *action)
{
	gchar const *target = cpg_link_action_get_target (action);
	CpgProperty *prop = NULL;

	if (link->priv->to)
	{
		prop =  cpg_object_get_property (link->priv->to, target);
	}

	_cpg_link_action_set_target_property (action, prop);
}


static void
resolve_link_actions (CpgLink *link)
{
	GSList *item;
	GSList *copy = g_slist_copy (link->priv->actions);

	for (item = copy; item; item = g_slist_next (item))
	{
		update_action_property (link, item->data);
	}

	g_slist_free (copy);

	cpg_object_taint (CPG_OBJECT (link));
}

static void
on_property_added_removed (CpgLink *link)
{
	resolve_link_actions (link);
}

static void
set_to (CpgLink   *link,
        CpgObject *target)
{
	if (link->priv->to)
	{
		_cpg_object_unlink (link->priv->to, link);

		g_signal_handler_disconnect (link->priv->to,
		                             link->priv->ext_signals[EXT_PROPERTY_ADDED]);

		g_signal_handler_disconnect (link->priv->to,
		                             link->priv->ext_signals[EXT_PROPERTY_REMOVED]);

		g_object_unref (link->priv->to);

		link->priv->to = NULL;
	}

	if (target)
	{
		link->priv->to = g_object_ref (target);
		_cpg_object_link (target, link);

		link->priv->ext_signals[EXT_PROPERTY_ADDED] =
			g_signal_connect_swapped (link->priv->to,
			                          "property-added",
			                          G_CALLBACK (on_property_added_removed),
			                          link);

		link->priv->ext_signals[EXT_PROPERTY_REMOVED] =
			g_signal_connect_swapped (link->priv->to,
			                          "property-removed",
			                          G_CALLBACK (on_property_added_removed),
			                          link);
	}

	resolve_link_actions (link);

	cpg_object_taint (CPG_OBJECT (link));
}

static void
set_from (CpgLink     *link,
          CpgProperty *target)
{
	if (link->priv->from)
	{
		g_object_unref (link->priv->from);
		link->priv->from = NULL;
	}

	if (target)
	{
		link->priv->from = g_object_ref (target);
	}

	cpg_object_taint (CPG_OBJECT (link));
}

static void
cpg_link_set_property (GObject       *object,
                       guint          prop_id,
                       GValue const  *value,
                       GParamSpec    *pspec)
{
	CpgLink *link = CPG_LINK (object);

	switch (prop_id)
	{
		case PROP_TO:
		{
			set_to (link, g_value_get_object (value));
		}
		break;
		case PROP_FROM:
		{
			set_from (link, g_value_get_object (value));
		}
		break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
on_action_target_changed (CpgLinkAction *action,
                          GParamSpec    *spec,
                          CpgLink       *link)
{
	update_action_property (link, action);
}

static void
remove_action (CpgLink       *link,
               CpgLinkAction *action)
{
	_cpg_link_action_set_target_property (action, NULL);

	g_signal_handlers_disconnect_by_func (action,
	                                      on_action_target_changed,
	                                      link);
}

static void
cpg_link_dispose (GObject *object)
{
	CpgLink *link = CPG_LINK (object);

	set_to (link, NULL);
	set_from (link, NULL);

	GSList *item;

	for (item = link->priv->actions; item; item = g_slist_next (item))
	{
		remove_action (link, item->data);
		g_object_unref (item->data);
	}

	g_slist_free (link->priv->actions);
	link->priv->actions = NULL;

	G_OBJECT_CLASS (cpg_link_parent_class)->dispose (object);
}

static void
cpg_link_foreach_expression_impl (CpgObject                *object,
                                  CpgForeachExpressionFunc  func,
                                  gpointer                  userdata)
{
	/* Chain up */
	if (CPG_OBJECT_CLASS (cpg_link_parent_class)->foreach_expression != NULL)
	{
		CPG_OBJECT_CLASS (cpg_link_parent_class)->foreach_expression (object,
		                                                              func,
		                                                              userdata);
	}

	/* Reset action expressions */
	GSList *item;

	for (item = CPG_LINK (object)->priv->actions; item; item = g_slist_next (item))
	{
		func (cpg_link_action_get_equation (item->data), userdata);
	}
}

static void
cpg_link_copy_impl (CpgObject *object,
                    CpgObject *source)
{
	/* Chain up */
	if (CPG_OBJECT_CLASS (cpg_link_parent_class)->copy != NULL)
	{
		CPG_OBJECT_CLASS (cpg_link_parent_class)->copy (object, source);
	}

	/* Copy over link actions */
	GSList *item;
	CpgLink *source_link = CPG_LINK (source);
	CpgLink *target = CPG_LINK (object);

	for (item = source_link->priv->actions; item; item = g_slist_next (item))
	{
		cpg_link_add_action (target,
		                     cpg_link_action_copy (item->data));
	}
}

static gboolean
cpg_link_compile_impl (CpgObject         *object,
                       CpgCompileContext *context,
                       CpgCompileError   *error)
{
	CpgLink *link = CPG_LINK (object);

	cpg_compile_context_save (context);
	cpg_compile_context_prepend_object (context, link->priv->from);
	cpg_compile_context_prepend_object (context, object);

	/* Chain up, compile object */
	if (CPG_OBJECT_CLASS (cpg_link_parent_class)->compile)
	{
		if (!CPG_OBJECT_CLASS (cpg_link_parent_class)->compile (object, context, error))
		{
			cpg_compile_context_restore (context);
			return FALSE;
		}
	}

	/* Parse all link expressions */
	GSList const *actions = cpg_link_get_actions (link);
	gboolean ret = TRUE;

	while (actions)
	{
		CpgLinkAction *action = actions->data;
		CpgExpression *expr = cpg_link_action_get_equation (action);
		GError *gerror = NULL;

		if (!cpg_expression_compile (expr, context, &gerror))
		{
			cpg_debug_error ("Error while parsing expression [%s]<%s>: %s",
			                 cpg_object_get_id (object),
			                 cpg_expression_get_as_string (expr),
			                 gerror->message);

			if (error)
			{
				cpg_compile_error_set (error, gerror, object, NULL, action);
			}

			g_error_free (gerror);

			ret = FALSE;
			break;
		}

		actions = g_slist_next (actions);
	}

	cpg_compile_context_restore (context);

	return ret;
}

static gboolean
cpg_link_equal_impl (CpgObject *first, CpgObject *second)
{
	if (!CPG_OBJECT_CLASS (cpg_link_parent_class)->equal (first, second))
	{
		return FALSE;
	}

	CpgLink *link1 = CPG_LINK (first);
	CpgLink *link2 = CPG_LINK (second);

	if ((link1->priv->from == NULL && link2->priv->from != NULL) ||
	    (link2->priv->from == NULL && link1->priv->from != NULL) ||
	    (link1->priv->to == NULL && link2->priv->to != NULL) ||
	    (link2->priv->to == NULL && link1->priv->to != NULL))
	{
		return FALSE;
	}

	if (link1->priv->from &&
	    g_strcmp0 (cpg_object_get_id (link1->priv->from),
	               cpg_object_get_id (link2->priv->from)) != 0)
	{
		return FALSE;
	}

	if (link1->priv->to &&
	    g_strcmp0 (cpg_object_get_id (link1->priv->to),
	               cpg_object_get_id (link2->priv->to)) != 0)
	{
		return FALSE;
	}

	return TRUE;
}

static void
cpg_link_class_init (CpgLinkClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	CpgObjectClass *cpgobject_class = CPG_OBJECT_CLASS (klass);

	object_class->finalize = cpg_link_finalize;
	object_class->dispose = cpg_link_dispose;

	object_class->get_property = cpg_link_get_property;
	object_class->set_property = cpg_link_set_property;

	cpgobject_class->foreach_expression = cpg_link_foreach_expression_impl;
	cpgobject_class->copy = cpg_link_copy_impl;
	cpgobject_class->compile = cpg_link_compile_impl;
	cpgobject_class->equal = cpg_link_equal_impl;

	/**
	 * CpgLink::action-added:
	 * @object: a #CpgObject
	 * @action: the added #CpgLinkAction
	 *
	 * Emitted when a link action is added to the link
	 *
	 **/
	signals[ACTION_ADDED] =
		g_signal_new ("action-added",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (CpgLinkClass,
		                               action_added),
		              NULL,
		              NULL,
		              g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE,
		              1,
		              CPG_TYPE_LINK_ACTION);

	/**
	 * CpgLink::action-removed:
	 * @object: a #CpgObject
	 * @action: the removed #CpgLinkAction
	 *
	 * Emitted when a link action is removed from the link
	 *
	 **/
	signals[ACTION_REMOVED] =
		g_signal_new ("action-removed",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (CpgLinkClass,
		                               action_removed),
		              NULL,
		              NULL,
		              g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE,
		              1,
		              CPG_TYPE_LINK_ACTION);

	/**
	 * CpgLink:from:
	 *
	 * The from #CpgObject
	 *
	 **/
	g_object_class_install_property (object_class,
	                                 PROP_FROM,
	                                 g_param_spec_object ("from",
	                                                      "FROM",
	                                                      "The link from object",
	                                                      CPG_TYPE_OBJECT,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	/**
	 * CpgLink:to:
	 *
	 * The to #CpgObject
	 *
	 **/
	g_object_class_install_property (object_class,
	                                 PROP_TO,
	                                 g_param_spec_object ("to",
	                                                      "TO",
	                                                      "The link to object",
	                                                      CPG_TYPE_OBJECT,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_type_class_add_private (object_class, sizeof (CpgLinkPrivate));
}

static void
cpg_link_init (CpgLink *self)
{
	self->priv = CPG_LINK_GET_PRIVATE (self);
}

/**
 * cpg_link_new:
 * @id: the object id
 * @from: (allow-none): a #CpgObject
 * @to: (allow-none): a #CpgObject
 *
 * Create a new #CpgLink
 *
 * Returns: a new #CpgLink
 *
 **/
CpgLink *
cpg_link_new (gchar const  *id,
              CpgObject    *from,
              CpgObject    *to)
{
	return g_object_new (CPG_TYPE_LINK,
	                     "id", id,
	                     "from", from,
	                     "to", to, NULL);
}

/**
 * cpg_link_add_action:
 * @link: the #CpgLink
 * @action: the #CpgLinkAction
 *
 * Add a new action to be performed when the link is evaluated during
 * simulation.
 *
 * Returns: %TRUE if @action could be successfully added, %FALSE otherwise
 *
 **/
gboolean
cpg_link_add_action (CpgLink       *link,
                     CpgLinkAction *action)
{
	g_return_val_if_fail (CPG_IS_LINK (link), FALSE);
	g_return_val_if_fail (CPG_IS_LINK_ACTION (action), FALSE);

	GSList *item;
	GSList *copy = g_slist_copy (link->priv->actions);

	gchar const *target = cpg_link_action_get_target (action);

	for (item = copy; item; item = g_slist_next (item))
	{
		CpgLinkAction *ac = item->data;
		gchar const *targ = cpg_link_action_get_target (ac);

		if (g_strcmp0 (targ, target) == 0)
		{
			return FALSE;
		}
	}

	g_slist_free (copy);

	update_action_property (link, action);

	link->priv->actions = g_slist_append (link->priv->actions,
	                                      action);

	g_object_ref_sink (action);

	g_signal_connect (action,
	                  "notify::target",
	                  G_CALLBACK (on_action_target_changed),
	                  link);

	cpg_object_taint (CPG_OBJECT (link));

	g_signal_emit (link, signals[ACTION_ADDED], 0, action);

	return TRUE;
}

/**
 * cpg_link_remove_action:
 * @link: the #CpgLink
 * @action: the #CpgLinkAction
 *
 * Removes an action from the link.
 *
 * Returns: %TRUE if the action was successfully removed
 *
 **/
gboolean
cpg_link_remove_action (CpgLink       *link,
                        CpgLinkAction *action)
{
	g_return_val_if_fail (CPG_IS_LINK (link), FALSE);
	g_return_val_if_fail (CPG_IS_LINK_ACTION (action), FALSE);

	GSList *item = g_slist_find (link->priv->actions, action);

	if (item != NULL)
	{
		link->priv->actions = g_slist_delete_link (link->priv->actions, item);

		remove_action (link, action);

		g_signal_emit (link, signals[ACTION_REMOVED], 0, action);
		g_object_unref (action);

		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/**
 * cpg_link_get_from:
 * @link: the #CpgLink
 *
 * Returns the from #CpgObject of the link
 *
 * Return value: the from #CpgObject
 *
 **/
CpgObject *
cpg_link_get_from (CpgLink *link)
{
	g_return_val_if_fail (CPG_IS_LINK (link), NULL);

	return link->priv->from;
}

/**
 * cpg_link_get_to:
 * @link: the #CpgLink
 *
 * Returns the to #CpgObject of the link
 *
 * Return value: the to #CpgObject
 *
 **/
CpgObject *
cpg_link_get_to (CpgLink *link)
{
	g_return_val_if_fail (CPG_IS_LINK (link), NULL);

	return link->priv->to;
}

/**
 * cpg_link_get_actions:
 * @link: the #CpgLink
 *
 * Get link actions
 *
 * Returns: (element-type CpgLinkAction): list of #CpgLinkAction. The list is
 *          owned by the link and should not be freed
 *
 **/
GSList const *
cpg_link_get_actions (CpgLink *link)
{
	g_return_val_if_fail (CPG_IS_LINK (link), NULL);

	return link->priv->actions;
}

/**
 * cpg_link_get_action:
 * @link: A #CpgLink
 * @target: The target property name
 *
 * Get a #CpgLinkAction targetting the property @target.
 *
 * Returns: A #CpgLinkAction
 *
 **/
CpgLinkAction *
cpg_link_get_action (CpgLink     *link,
                     gchar const *target)
{
	g_return_val_if_fail (CPG_IS_LINK (link), NULL);
	g_return_val_if_fail (target != NULL, NULL);

	GSList *actions = link->priv->actions;

	while (actions)
	{
		CpgLinkAction *action = actions->data;

		if (g_strcmp0 (cpg_link_action_get_target (action), target) == 0)
		{
			return action;
		}

		actions = g_slist_next (actions);
	}

	return NULL;
}

/**
 * cpg_link_attach:
 * @link: (allow-none): A #CpgLink
 * @from: (allow-none): A #CpgObject
 * @to: A #CpgObject
 *
 * Attach @link to the objects @from and @to. This is equivalent to:
 * <informalexample>
 * <programlisting>
 * g_object_set (link, "from", from, "to", to);
 * </programlisting>
 * </informalexample>
 *
 **/
void
cpg_link_attach (CpgLink   *link,
                 CpgObject *from,
                 CpgObject *to)
{
	g_return_if_fail (CPG_IS_LINK (link));
	g_return_if_fail ((from == NULL) == (to == NULL));
	g_return_if_fail (from == NULL || CPG_IS_OBJECT (from));
	g_return_if_fail (to == NULL || CPG_IS_OBJECT (to));

	g_object_set (link, "from", from, "to", to, NULL);
}
