#include "implementation.h"
#include "defines.h"

#include <string.h>

typedef struct
{
	GModule *module;
	CdnRawcNetwork *network;

	union
	{
		double *data;
		float *dataf;
	};

	double *buffer;

	union
	{
		void (*init) (double t);
		void (*initf) (float t);
	};

	union
	{
		void (*step) (double t, double dt);
		void (*stepf) (float t, float dt);
	};

	CdnRawcDimension const *(*get_dimension) (uint32_t i);
} RawcData;

static gboolean
rawc_is_float (RawcData *data)
{
	return sizeof (float) == data->network->type_size;
}

static void
copy_buffer (RawcData *data)
{
	gint i;

	for (i = 0; i < data->network->data_size; ++i)
	{
		data->buffer[i] = (gdouble)data->dataf[i];
	}
}

static gboolean
monitor_free (CdnMonitorImplementation *implementation)
{
	RawcData *data = implementation->userdata;
	g_module_close (data->module);

	return FALSE;
}

static gchar *
variable_get_name (CdnMonitorVariable *v)
{
	RawcData *data = v->userdata;

	if (v->state >= data->network->meta.states_size)
	{
		return NULL;
	}

	// TODO: full name
	return g_strdup (data->network->meta.states[v->state].name);
}

static gdouble const *
variable_get_values (CdnMonitorVariable *v)
{
	RawcData *data = v->userdata;

	return data->buffer + data->network->meta.states[v->state].index;
}

static CdnMonitorVariable *
create_monitor_variable (CdnMonitorImplementation *implementation,
                         uint32_t                  state)
{
	CdnMonitorVariable *ret;
	RawcData *data = implementation->userdata;
	CdnRawcDimension const *dim;

	ret = g_slice_new0 (CdnMonitorVariable);

	ret->state = state;
	ret->userdata = data;

	dim = data->get_dimension (data->network->meta.states[state].index);

	if (dim)
	{
		ret->dimension.rows = dim->rows;
		ret->dimension.columns = dim->columns;
	}
	else
	{
		ret->dimension.rows = 1;
		ret->dimension.columns = 1;
	}

	ret->get_name = variable_get_name;
	ret->get_values = variable_get_values;

	ret->row = -1;
	ret->col = -1;

	return ret;
}

static void
set_seed (CdnMonitorImplementation *implementation,
          guint                     seed)
{
	srand (seed);
}

static CdnMonitorVariable *
monitor_get_time (CdnMonitorImplementation *implementation)
{
	RawcData *data = implementation->userdata;

	return create_monitor_variable (implementation, data->network->meta.t);
}

static gdouble
monitor_step (CdnMonitorImplementation *implementation,
              gdouble                   t,
              gdouble                   timestep)
{
	RawcData *data = implementation->userdata;

	if (rawc_is_float (data))
	{
		data->stepf ((gfloat)t, (gfloat)timestep);
		copy_buffer (data);
	}
	else
	{
		data->step (t, timestep);
	}

	return data->buffer[data->network->meta.dt];
}

static void
monitor_begin (CdnMonitorImplementation *implementation,
               gdouble                   t,
               gdouble                   timestep)
{
	RawcData *data = implementation->userdata;

	if (rawc_is_float (data))
	{
		data->initf ((gfloat)t);
		copy_buffer (data);
	}
	else
	{
		data->init (t);
	}
}

typedef struct
{
	uint32_t id;
	uint8_t is_node;
} Selection;

static Selection *
selection_new (uint32_t id, uint8_t is_node)
{
	Selection *ret;

	ret = g_slice_new0 (Selection);
	ret->id = id;
	ret->is_node = is_node;

	return ret;
}

static void
selection_free (Selection *selection)
{
	g_slice_free (Selection, selection);
}

static gchar const *
selection_name (CdnRawcNetwork *network,
                Selection *selection)
{
	if (selection->is_node)
	{
		if (selection->id < network->meta.nodes_size)
		{
			return network->meta.nodes[selection->id].name;
		}
	}
	else
	{
		if (selection->id < network->meta.states_size)
		{
			return network->meta.states[selection->id].name;
		}
	}

	return NULL;
}

static GSList *
selector_identifier (RawcData     *data,
                     GSList       *selection,
                     GSList const *items)
{
	GSList *ret = NULL;

	while (selection)
	{
		Selection *sel = selection->data;
		gchar const *name;
		GSList const *item;

		name = selection_name (data->network, sel);

		selection = g_slist_delete_link (selection, selection);

		if (name == NULL)
		{
			selection_free (sel);
			continue;
		}

		for (item = items; item; item = g_slist_next (item))
		{
			CdnExpansion *ex = items->data;
			gchar const *nm = cdn_expansion_get (ex, 0);

			if (strcmp (nm, name) == 0)
			{
				ret = g_slist_prepend (ret, sel);
				sel = NULL;
				break;
			}
		}

		if (sel)
		{
			selection_free (sel);
		}
	}

	return g_slist_reverse (ret);
}

static GSList *
selector_regex (RawcData *data,
                GSList   *selection,
                GRegex   *regex)
{
	GSList *ret = NULL;

	while (selection)
	{
		Selection *sel = selection->data;
		gchar const *name;

		name = selection_name (data->network, sel);

		selection = g_slist_delete_link (selection, selection);

		if (name == NULL)
		{
			selection_free (sel);
			continue;
		}

		if (g_regex_match (regex, name, 0, NULL))
		{
			ret = g_slist_prepend (ret, sel);
		}
		else
		{
			selection_free (sel);
		}
	}

	return g_slist_reverse (ret);
}

static GSList *
selector_children (RawcData *data,
                   GSList   *selection)
{
	GSList *ret = NULL;

	while (selection)
	{
		Selection *sel = selection->data;

		selection = g_slist_delete_link (selection, selection);

		if (!sel->is_node || sel->id >= data->network->meta.nodes_size)
		{
			selection_free (sel);
			continue;
		}

		uint32_t child = data->network->meta.nodes[sel->id].first_child;

		while (child != 0)
		{
			CdnRawcChildMeta const *cm = &data->network->meta.children[child];
			Selection *childsel = selection_new (cm->index, cm->is_node);

			ret = g_slist_prepend (ret, childsel);
			child = cm->next;
		}

		selection_free (sel);
	}

	return g_slist_reverse (ret);
}

static GSList *
monitor_resolve (CdnMonitorImplementation *implementation,
                 CdnSelector              *selector)
{
	GSList const *parts;
	GSList *selection;
	RawcData *data = implementation->userdata;
	GSList *ret = NULL;

	parts = cdn_selector_get_parts (selector);

	// Index 1 is the root network node in the meta nodes
	selection = g_slist_prepend (NULL, selection_new (1, 1));

	if (cdn_selector_get_implicit_children (selector))
	{
		selection = selector_children (data, selection);
	}

	while (parts)
	{
		CdnSelectorPart *p = parts->data;

		switch (cdn_selector_part_type (p))
		{
			case CDN_SELECTOR_PART_TYPE_IDENTIFIER:
			{
				GSList *items;

				items = cdn_selector_part_identifier (p);

				selection = selector_identifier (data,
				                                 selection,
				                                 items);

				g_slist_foreach (items, (GFunc)cdn_expansion_unref, NULL);
				g_slist_free (items);
			}
			break;
			case CDN_SELECTOR_PART_TYPE_REGEX:
			{
				GRegex *regex;

				regex = cdn_selector_part_regex (p);

				selection = selector_regex (data,
				                            selection,
				                            regex);

				g_regex_unref (regex);
			}
			break;
			case CDN_SELECTOR_PART_TYPE_PSEUDO:
			{
				// Only children work
				CdnSelectorPseudoType ptype;

				ptype = cdn_selector_part_pseudo_type (p);

				if (ptype == CDN_SELECTOR_PSEUDO_TYPE_CHILDREN)
				{
					selection = selector_children (data,
					                               selection);
				}
				else
				{
					g_printerr ("Only the `children' selector is currently supported for rawc.\n");
				}
			}
			break;
		}

		parts = g_slist_next (parts);
	}

	while (selection)
	{
		Selection *sel = selection->data;

		if (!sel->is_node)
		{
			ret = g_slist_prepend (ret,
			                       create_monitor_variable (implementation,
			                                                sel->id));
		}

		selection_free (sel);
		selection = g_slist_delete_link (selection, selection);
	}

	return g_slist_reverse (ret);
}

CdnMonitorImplementation *
cdn_monitor_implementation_rawc_new (gchar const *filename)
{
	CdnMonitorImplementation *ret;
	GModule *module;
	gchar *name;
	gchar const *ptr;
	gchar *symname;

	module = g_module_open (filename, G_MODULE_BIND_LOCAL);

	if (!module)
	{
		return NULL;
	}

	name = g_path_get_basename (filename);
	ptr = name;

	if (g_str_has_prefix (ptr, "lib"))
	{
		ptr += 3;
	}

	if (g_str_has_suffix (name, DYLIB_SUFFIX))
	{
		name[strlen(name) - strlen (DYLIB_SUFFIX)] = '\0';
	}

	symname = g_strconcat ("cdn_rawc_", ptr, "_reset", NULL);

	gpointer initsym;

	if (!g_module_symbol (module, symname, &initsym))
	{
		g_printerr ("Could not find %s\n", symname);

		g_free (name);
		g_free (symname);
		return NULL;
	}

	g_free (symname);
	symname = g_strconcat ("cdn_rawc_", ptr, "_step", NULL);

	gpointer stepsym;

	if (!g_module_symbol (module, symname, &stepsym))
	{
		g_printerr ("Could not find %s\n", symname);

		g_free (name);
		g_free (symname);

		return NULL;
	}

	g_free (symname);

	symname = g_strconcat ("cdn_rawc_", ptr, "_get_dimension", NULL);

	gpointer getdimsym;

	if (!g_module_symbol (module, symname, &getdimsym))
	{
		g_printerr ("Could not find %s\n", symname);

		g_free (name);
		g_free (symname);

		return NULL;
	}

	g_free (symname);

	symname = g_strconcat ("cdn_rawc_", ptr, "_network", NULL);

	CdnRawcNetwork *(*get_network) ();

	if (!g_module_symbol (module, symname, (gpointer *)&get_network))
	{
		g_printerr ("Could not find %s\n", symname);

		g_free (name);
		g_free (symname);

		return NULL;
	}

	g_free (symname);
	symname = g_strconcat ("cdn_rawc_", ptr, "_data", NULL);

	gdouble *(*get_data) ();

	if (!g_module_symbol (module, symname, (gpointer *)&get_data))
	{
		g_printerr ("Could not find %s\n", symname);

		g_free (name);
		g_free (symname);

		return NULL;
	}

	ret = cdn_monitor_implementation_new ();

	RawcData *data = g_slice_new0 (RawcData);

	data->data = get_data ();

	data->network = get_network ();
	data->module = module;

	if (rawc_is_float (data))
	{
		data->buffer = g_new0 (gdouble, data->network->data_size);
	}
	else
	{
		data->buffer = data->data;
	}

	if (data->network->meta.states_size == 0)
	{
		g_warning ("It seems that the network does not export any metadata, monitor will not be able to find anything to monitor!");
	}

	data->init = initsym;
	data->step = stepsym;
	data->get_dimension = getdimsym;

	ret->free = monitor_free;
	ret->userdata = data;
	ret->set_seed = set_seed;

	ret->resolve = monitor_resolve;
	ret->begin = monitor_begin;
	ret->step = monitor_step;
	ret->get_time = monitor_get_time;

	return ret;
}
