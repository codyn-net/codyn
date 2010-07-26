#include "cpg-link-action.h"

/**
 * SECTION:cpg-link-action
 * @short_description: Link action equation
 *
 * A #CpgLinkAction is an action inside a link which sets a target
 * #CpgProperty to the value of a particular #CpgExpression equation.
 */

#define CPG_LINK_ACTION_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), CPG_TYPE_LINK_ACTION, CpgLinkActionPrivate))

struct _CpgLinkActionPrivate
{
	gchar *target;
	CpgExpression *equation;
	CpgProperty *property;

	guint equation_proxy_id;
};

/* Properties */
enum
{
	PROP_0,
	PROP_TARGET,
	PROP_EQUATION,
	PROP_TARGET_PROPERTY
};

G_DEFINE_TYPE (CpgLinkAction, cpg_link_action, G_TYPE_INITIALLY_UNOWNED)

static void
set_property (CpgLinkAction *action,
              CpgProperty   *property)
{
	if (action->priv->property == property)
	{
		return;
	}

	if (action->priv->property)
	{
		_cpg_property_unuse (action->priv->property);
		g_object_unref (action->priv->property);
		action->priv->property = NULL;
	}

	if (property)
	{
		action->priv->property = g_object_ref_sink (property);
		_cpg_property_use (action->priv->property);
	}
}

static void
set_target (CpgLinkAction *action,
            gchar const   *target)
{
	if (g_strcmp0 (action->priv->target, target) == 0)
	{
		return;
	}

	g_free (action->priv->target);
	action->priv->target = g_strdup (target);

	g_object_notify (G_OBJECT (action), "target");
}

static void
on_expression_changed (CpgLinkAction *action)
{
	g_object_notify (G_OBJECT (action), "equation");
}

static void
set_equation (CpgLinkAction *action,
              CpgExpression *equation)
{
	if (action->priv->equation == equation)
	{
		return;
	}

	if (action->priv->equation)
	{
		g_signal_handler_disconnect (action->priv->equation,
		                             action->priv->equation_proxy_id);

		g_object_unref (action->priv->equation);
		action->priv->equation = NULL;
	}

	if (equation)
	{
		action->priv->equation = g_object_ref_sink (equation);

		action->priv->equation_proxy_id =
			g_signal_connect_swapped (action->priv->equation,
			                          "notify::expression",
			                          G_CALLBACK (on_expression_changed),
			                          action);
	}
}

static void
cpg_link_action_dispose (GObject *object)
{
	CpgLinkAction *action = CPG_LINK_ACTION (object);

	set_property (action, NULL);

	G_OBJECT_CLASS (cpg_link_action_parent_class)->dispose (object);
}

static void
cpg_link_action_finalize (GObject *object)
{
	CpgLinkAction *action = CPG_LINK_ACTION (object);

	set_target (action, NULL);
	set_equation (action, NULL);

	G_OBJECT_CLASS (cpg_link_action_parent_class)->finalize (object);
}

static void
cpg_link_action_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
	CpgLinkAction *self = CPG_LINK_ACTION (object);

	switch (prop_id)
	{
		case PROP_TARGET:
			set_target (self, g_value_get_string (value));
		break;
		case PROP_EQUATION:
			set_equation (self,
			              CPG_EXPRESSION (g_value_get_object (value)));
		break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
cpg_link_action_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
	CpgLinkAction *self = CPG_LINK_ACTION (object);
	
	switch (prop_id)
	{
		case PROP_TARGET:
			g_value_set_object (value, self->priv->target);
		break;
		case PROP_EQUATION:
			g_value_set_object (value, self->priv->equation);
		break;
		case PROP_TARGET_PROPERTY:
			g_value_set_object (value, self->priv->property);
		break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
cpg_link_action_class_init (CpgLinkActionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = cpg_link_action_finalize;
	object_class->dispose = cpg_link_action_dispose;

	object_class->set_property = cpg_link_action_set_property;
	object_class->get_property = cpg_link_action_get_property;

	/**
	 * CpgLinkAction:target:
	 *
	 * The target #CpgProperty
	 *
	 **/
	g_object_class_install_property (object_class,
	                                 PROP_TARGET,
	                                 g_param_spec_string ("target",
	                                                      "Target",
	                                                      "Target",
	                                                      NULL,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	/**
	 * CpgLinkAction:equation:
	 *
	 * The equation
	 *
	 **/
	g_object_class_install_property (object_class,
	                                 PROP_EQUATION,
	                                 g_param_spec_object ("equation",
	                                                      "Equation",
	                                                      "Equation",
	                                                      CPG_TYPE_EXPRESSION,
	                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	/**
	 * CpgLinkAction:target-property:
	 *
	 * The target property
	 *
	 **/
	g_object_class_install_property (object_class,
	                                 PROP_TARGET_PROPERTY,
	                                 g_param_spec_object ("target-property",
	                                                      "Target property",
	                                                      "Target Property",
	                                                      CPG_TYPE_PROPERTY,
	                                                      G_PARAM_READABLE));

	g_type_class_add_private (object_class, sizeof(CpgLinkActionPrivate));
}

static void
cpg_link_action_init (CpgLinkAction *self)
{
	self->priv = CPG_LINK_ACTION_GET_PRIVATE (self);
}

/**
 * cpg_link_action_new:
 * @target: A #CpgProperty
 * @equation: A #CpgExpression
 * 
 * Create a new #CpgLinkAction.
 *
 * Returns: A new #CpgLinkAction
 *
 **/
CpgLinkAction *
cpg_link_action_new (gchar const   *target,
                     CpgExpression *equation)
{
	return g_object_new (CPG_TYPE_LINK_ACTION,
	                     "target", target,
	                     "equation", equation,
	                     NULL);
}

/**
 * cpg_link_action_get_target:
 * @action: A #CpgLinkAction
 *
 * Get the target of the action.
 *
 * Returns: A #CpgProperty
 *
 **/
gchar const *
cpg_link_action_get_target (CpgLinkAction *action)
{
	g_return_val_if_fail (CPG_IS_LINK_ACTION (action), NULL);

	return action->priv->target;
}

/**
 * cpg_link_action_set_target:
 * @action: A #CpgLinkAction
 * @target: A #CpgProperty
 *
 * Set the target of the action.
 *
 **/
void
cpg_link_action_set_target (CpgLinkAction *action,
                            gchar const   *target)
{
	g_return_if_fail (CPG_IS_LINK_ACTION (action));

	set_target (action, target);
}

/**
 * cpg_link_action_get_equation:
 * @action: A #CpgLinkAction
 *
 * Get the equation of the action.
 *
 * Returns: A #CpgExpression
 *
 **/
CpgExpression *
cpg_link_action_get_equation (CpgLinkAction *action)
{
	g_return_val_if_fail (CPG_IS_LINK_ACTION (action), NULL);

	return action->priv->equation;
}

/**
 * cpg_link_action_set_equation:
 * @action: A #CpgLinkAction
 * @equation: A #CpgExpression
 *
 * Set the equation of the action.
 *
 **/
void
cpg_link_action_set_equation (CpgLinkAction *action,
                              CpgExpression *equation)
{
	g_return_if_fail (CPG_IS_LINK_ACTION (action));
	g_return_if_fail (equation == NULL || CPG_IS_EXPRESSION (equation));

	set_equation (action, equation);

	if (action->priv->equation == equation)
	{
		g_object_notify (G_OBJECT (action), "equation");
	}
}

/**
 * cpg_link_action_depends:
 * @action: A #CpgLinkAction
 * @property: A #CpgProperty
 *
 * Check whether the action depends on a certain property.
 *
 * Returns: %TRUE if the action depends on @property, %FALSE otherwise
 *
 **/
gboolean
cpg_link_action_depends (CpgLinkAction *action,
                         CpgProperty   *property)
{
	g_return_val_if_fail (CPG_IS_LINK_ACTION (action), FALSE);
	g_return_val_if_fail (CPG_IS_PROPERTY (property), FALSE);

	return g_slist_find ((GSList *)cpg_expression_get_dependencies (action->priv->equation),
	                     property) != NULL;
}

/**
 * cpg_link_action_copy:
 * @action: A #CpgLinkAction
 *
 * Create a copy of a #CpgLinkAction.
 *
 * Returns: A #CpgLinkAction
 *
 **/
CpgLinkAction *
cpg_link_action_copy (CpgLinkAction *action)
{
	g_return_val_if_fail (CPG_IS_LINK_ACTION (action), NULL);

	return cpg_link_action_new (g_strdup (action->priv->target),
	                            cpg_expression_copy (action->priv->equation));
}

/**
 * cpg_link_action_get_target_property:
 * @action: A #CpgLinkAction
 *
 * Get the target property of the link action.
 *
 * Returns: A #CpgProperty
 *
 **/
CpgProperty *
cpg_link_action_get_target_property (CpgLinkAction *action)
{
	g_return_val_if_fail (CPG_IS_LINK_ACTION (action), NULL);

	return action->priv->property;
}

void
_cpg_link_action_set_target_property (CpgLinkAction *action,
                                      CpgProperty   *property)
{
	g_return_if_fail (CPG_IS_LINK_ACTION (action));
	g_return_if_fail (property == NULL || CPG_IS_PROPERTY (property));

	set_property (action, property);
}
