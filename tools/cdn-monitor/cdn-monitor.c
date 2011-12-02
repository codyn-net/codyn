/*
 * cdn-monitor.c
 * This file is part of codyn
 *
 * Copyright (C) 2011 - Jesse van den Kieboom
 *
 * codyn is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * codyn is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <codyn/codyn.h>
#include <codyn/cdn-selector.h>
#include <gio/gio.h>
#include <glib/gprintf.h>
#include <string.h>
#include <unistd.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include <sys/time.h>
#include <math.h>

static GPtrArray *monitored = 0;
static GPtrArray *varied = 0;
static gboolean include_header = FALSE;
static gchar *delimiter = NULL;
static gdouble from = 0;
static gdouble step = 0.001;
static gdouble to = 1;
static gchar *output_file = NULL;
static gint64 seed = 0;

#define CDN_MONITOR_ERROR (cdn_monitor_error_quark())

static GQuark
cdn_monitor_error_quark ()
{
	return g_quark_from_static_string ("cdn-monitor-error");
}

typedef enum
{
	CDN_MONITOR_ERROR_RANGE
} CdnMonitorError;

static gboolean
parse_monitored (gchar const  *option_name,
                 gchar const  *value,
                 gpointer      data,
                 GError      **error)
{
	g_ptr_array_add (monitored, g_strdup (value));
	return TRUE;
}

typedef struct
{
	gchar *selector;
	gdouble from;
	gdouble step;
	gdouble to;
} Range;

static Range *
range_new (gchar const *sel,
           gdouble from,
           gdouble step,
           gdouble to)
{
	Range *ret;

	ret = g_slice_new0 (Range);

	ret->selector = g_strdup (sel);
	ret->from = from;
	ret->step = step;
	ret->to = to;

	return ret;
}

static void
range_free (Range *self)
{
	g_free (self->selector);

	g_slice_free (Range, self);
}

static gboolean
parse_varied (gchar const  *option_name,
              gchar const  *value,
              gpointer      data,
              GError      **error)
{
	static GRegex *regex = NULL;
	GMatchInfo *info;

	if (regex == NULL)
	{
		regex = g_regex_new ("^(.*):([^:]*):([^:]*):([^:]*)$", 0, 0, NULL);
	}

	if (!g_regex_match (regex, value, 0, &info))
	{
		if (error)
		{
			g_set_error (error,
			             CDN_MONITOR_ERROR,
			             CDN_MONITOR_ERROR_RANGE,
			             "Could not parse range varying range: %s",
			             value);
		}

		return FALSE;
	}

	gchar *s = g_match_info_fetch (info, 1);
	gchar *from = g_match_info_fetch (info, 2);
	gchar *step = g_match_info_fetch (info, 3);
	gchar *to = g_match_info_fetch (info, 4);

	g_ptr_array_add (varied, range_new (s,
	                                    g_ascii_strtod (from, NULL),
	                                    g_ascii_strtod (step, NULL),
	                                    g_ascii_strtod (to, NULL)));

	g_free (s);
	g_free (from);
	g_free (step);
	g_free (to);

	return TRUE;
}

static gboolean
parse_time (gchar const  *option_name,
            gchar const  *value,
            gpointer      data,
            GError      **error)
{
	gchar **parts = g_strsplit (value, ":", 0);
	gint len = g_strv_length (parts);

	if (len < 2)
	{
		if (error)
		{
			g_set_error (error,
			             CDN_MONITOR_ERROR,
			             CDN_MONITOR_ERROR_RANGE,
			             "Could not parse range: %s",
			             value);
		}

		g_strfreev (parts);
		return FALSE;
	}

	if (len == 2)
	{
		from = g_ascii_strtod (parts[0], NULL);
		to = g_ascii_strtod (parts[1], NULL);
	}
	else
	{
		from = g_ascii_strtod (parts[0], NULL);
		step = g_ascii_strtod (parts[1], NULL);
		to = g_ascii_strtod (parts[2], NULL);
	}

	g_strfreev (parts);

	if ((to - (from + step)) >= to - from)
	{
		if (error)
		{
			g_set_error (error,
			             CDN_MONITOR_ERROR,
			             CDN_MONITOR_ERROR_RANGE,
			             "Invalid range: %s",
			             value);
		}

		g_strfreev (parts);
		return FALSE;
	}

	return TRUE;
}

static GOptionEntry entries[] = {
	{"monitor", 'm', 0, G_OPTION_ARG_CALLBACK, parse_monitored, "Selector for variables to monitor (e.g. /state_.*/.\"{x,y}\")", "SEL"},
	{"include-header", 'i', 0, G_OPTION_ARG_NONE, &include_header, "Include header in output", NULL},
	{"delimiter", 'd', 0, G_OPTION_ARG_STRING, &delimiter, "Column delimiter (defaults to tab)", "DELIM"},
	{"time", 't', 0, G_OPTION_ARG_CALLBACK, parse_time, "Time range (from:to or from:step:to, defaults to 0:0.01:1)", "RANGE"},
	{"output", 'o', 0, G_OPTION_ARG_STRING, &output_file, "Output file (defaults to standard output)", "FILE"},
	{"seed", 's', 0, G_OPTION_ARG_INT64, &seed, "Random numbers seed (defaults to current time)", "SEED"},
	{"vary", 'v', 0, G_OPTION_ARG_CALLBACK, parse_varied, "Run integration multiple times, varying this range (e.g. /state_.*/.\"{x,y}\"(0:0.1:10))", "RANGE"},
	{NULL}
};

static void
write_monitors (CdnNetwork    *network,
                GSList        *monitors,
                GSList        *names,
                GOutputStream *stream)
{
	gint i;
	gint num;
	gint row;
	GSList *item_monitor;
	GSList *item_name;

	if (include_header)
	{
		gchar *header_start = g_strdup_printf ("#%1$st%1$s", delimiter);

		g_output_stream_write_all (stream,
		                           header_start,
		                           strlen (header_start),
		                           NULL,
		                           NULL,
		                           NULL);

		item_monitor = monitors;
		item_name = names;

		while (item_monitor)
		{
			gchar *name = item_name->data;

			g_output_stream_write_all (stream,
			                           delimiter,
			                           strlen (delimiter),
			                           NULL,
			                           NULL,
			                           NULL);

			g_output_stream_write_all (stream,
			                           name,
			                           strlen (name),
			                           NULL,
			                           NULL,
			                           NULL);

			item_monitor = g_slist_next (item_monitor);
			item_name = g_slist_next (item_name);
		}

		g_output_stream_write_all (stream,
		                           "\n",
		                           1,
		                           NULL,
		                           NULL,
		                           NULL);

		g_free (header_start);
	}

	num = g_slist_length (monitors);
	gdouble const **data = g_new (gdouble const *, num);
	guint size;

	for (i = 0; i < num; ++i)
	{
		data[i] = cdn_monitor_get_data (monitors->data, &size);
		monitors = monitors->next;
	}

	for (row = 0; row < size; ++row)
	{
		gchar value[G_ASCII_DTOSTR_BUF_SIZE];

		for (i = 0; i < num; ++i)
		{
			if (i != 0)
			{
				g_output_stream_write_all (stream,
				                           delimiter,
				                           strlen (delimiter),
				                           NULL,
				                           NULL,
				                           NULL);
			}

			g_ascii_dtostr (value,
			                G_ASCII_DTOSTR_BUF_SIZE,
			                data[i][row]);

			g_output_stream_write_all (stream,
			                           value,
			                           strlen (value),
			                           NULL,
			                           NULL,
			                           NULL);
		}

		g_output_stream_write_all (stream, "\n", 1, NULL, NULL, NULL);
	}

	g_free (data);
}

static GSList *
find_matching_variables (CdnNetwork  *network,
                          gchar const *expression)
{
	GError *err = NULL;
	CdnSelector *sel;

	sel = cdn_selector_parse (CDN_OBJECT (network), expression, &err);

	if (err)
	{
		g_printerr ("Failed to parse selector \"%s\": %s\n", expression, err->message);
		g_error_free (err);

		return NULL;
	}

	CdnEmbeddedContext *context = cdn_embedded_context_new ();
	GSList *selection = cdn_selector_select (sel,
	                                         G_OBJECT (network),
	                                         CDN_SELECTOR_TYPE_VARIABLE,
	                                         context);
	GSList *element = selection;
	GSList *variables = NULL;

	while (element)
	{
		CdnVariable *property = cdn_selection_get_object (element->data);
		variables = g_slist_prepend (variables, property);

		g_object_unref (element->data);
		element = g_slist_next (element);
	}

	g_slist_free (selection);
	g_object_unref (context);
	g_object_unref (sel);

	return variables;
}

static GOutputStream *
get_output_stream ()
{
	GOutputStream *out = NULL;
	GError *error = NULL;

	if (output_file != NULL && g_strcmp0 (output_file, "-") != 0)
	{
		GFile *output;

		output = g_file_new_for_path (output_file);

		out = G_OUTPUT_STREAM (g_file_create (output,
		                                      G_FILE_CREATE_REPLACE_DESTINATION,
		                                      NULL,
		                                      &error));

		if (!out && error->code == G_IO_ERROR_EXISTS)
		{
			g_error_free (error);
			error = NULL;

			out = G_OUTPUT_STREAM (g_file_replace (output,
			                                       NULL,
			                                       FALSE,
			                                       G_FILE_CREATE_NONE,
			                                       NULL,
			                                       &error));
		}

		if (!out)
		{
			g_printerr ("Could not create output file `%s': %s\n",
			            output_file,
			            error->message);

			g_error_free (error);
		}

		g_object_unref (output);
	}
	else
	{
		out = g_unix_output_stream_new (STDOUT_FILENO,
		                                TRUE);
	}

	return out;
}

static gint
run_simple_monitor (CdnNetwork *network)
{
	GSList *monitors = NULL;
	GSList *names = NULL;
	CdnIntegrator *integrator;
	gint i;
	gint ret;

	integrator = cdn_network_get_integrator (network);

	for (i = monitored->len - 1; i >= 0; --i)
	{
		GSList *variables = find_matching_variables (network, monitored->pdata[i]);
		GSList *prop = variables;

		while (prop)
		{
			CdnMonitor *monitor;

			monitor = cdn_monitor_new (network,
			                           prop->data);

			monitors = g_slist_prepend (monitors, monitor);
			names = g_slist_prepend (names, cdn_variable_get_full_name (prop->data));

			prop = g_slist_next (prop);
		}

		g_slist_free (variables);
	}

	monitors = g_slist_prepend (monitors,
	                            cdn_monitor_new (network,
	                                              cdn_object_get_variable (CDN_OBJECT (integrator), "t")));

	cdn_network_run (network, from, step, to);

	GOutputStream *out;

	out = get_output_stream ();

	if (out)
	{
		write_monitors (network, monitors, names, out);

		g_output_stream_flush (out, NULL, NULL);
		g_output_stream_close (out, NULL, NULL);

		ret = 0;
	}
	else
	{
		ret = 1;
	}

	g_slist_foreach (monitors, (GFunc)g_object_unref, NULL);
	g_slist_free (monitors);
	g_slist_foreach (names, (GFunc)g_free, NULL);
	g_slist_free (names);

	return ret;
}

typedef struct
{
	CdnVariable *variable;
	Range const *range;
	gdouble value;
} VariableRange;

static VariableRange *
variable_range_new (CdnVariable *variable,
                    Range const *range)
{
	VariableRange *ret;

	ret = g_slice_new0 (VariableRange);

	ret->variable = variable;
	ret->range = range;
	ret->value = 0;

	return ret;
}

static void
variable_range_free (VariableRange *self)
{
	g_slice_free (VariableRange, self);
}

static void
simulate_combinations (CdnNetwork    *network,
                       GSList        *ranges,
                       GSList        *all_ranges,
                       GSList        *vars,
                       GOutputStream *out)
{
	VariableRange *r;
	gdouble v;

	if (!ranges)
	{
		cdn_object_reset (CDN_OBJECT (network));
		gboolean first = TRUE;

		// Set all the current values
		while (all_ranges)
		{
			VariableRange *rr = all_ranges->data;
			gchar value[G_ASCII_DTOSTR_BUF_SIZE];

			cdn_variable_set_value (rr->variable, rr->value);

			if (!first)
			{
				g_output_stream_write_all (out,
				                           delimiter,
				                           strlen (delimiter),
				                           NULL,
				                           NULL,
				                           NULL);
			}
			else
			{
				first = FALSE;
			}

			g_ascii_dtostr (value,
			                G_ASCII_DTOSTR_BUF_SIZE,
			                rr->value);

			g_output_stream_write_all (out,
			                           value,
			                           strlen (value),
			                           NULL,
			                           NULL,
			                           NULL);

			all_ranges = g_slist_next (all_ranges);
		}

		// Simulate the network here
		cdn_network_run (network, from, step, to);

		// Write the output
		while (vars)
		{
			gchar value[G_ASCII_DTOSTR_BUF_SIZE];

			g_output_stream_write_all (out,
			                           delimiter,
			                           strlen (delimiter),
			                           NULL,
			                           NULL,
			                           NULL);

			g_ascii_dtostr (value,
			                G_ASCII_DTOSTR_BUF_SIZE,
			                cdn_variable_get_value (vars->data));

			vars = g_slist_next (vars);

			g_output_stream_write_all (out,
			                           value,
			                           strlen (value),
			                           NULL,
			                           NULL,
			                           NULL);
		}

		g_output_stream_write_all (out, "\n", 1, NULL, NULL, NULL);

		return;
	}

	r = ranges->data;
	v = r->range->from;

	while (fabs(r->range->to - v) > 10e-9)
	{
		// This makes sure we can just reset the network, but these
		// values stay persistent
		r->value = v;

		simulate_combinations (network, ranges->next, all_ranges, vars, out);

		v += r->range->step;
	}
}

static gint
run_varied_monitor (CdnNetwork *network)
{
	GSList *vars = NULL;
	GSList *ranges = NULL;
	gint i;
	gboolean first = TRUE;

	GOutputStream *out = get_output_stream ();

	if (!out)
	{
		return 1;
	}

	for (i = monitored->len - 1; i >= 0; --i)
	{
		GSList *variables = find_matching_variables (network, monitored->pdata[i]);
		GSList *prop = variables;

		while (prop)
		{
			if (include_header)
			{
				if (!first)
				{
					g_output_stream_write_all (out,
					                           delimiter,
					                           strlen (delimiter),
					                           NULL,
					                           NULL,
					                           NULL);
				}
				else
				{
					first = FALSE;
				}

				gchar *name = cdn_variable_get_full_name_for_display (prop->data);

				g_output_stream_write_all (out,
				                           name,
				                           strlen (name),
				                           NULL,
				                           NULL,
				                           NULL);

				g_free (name);
			}

			vars = g_slist_prepend (vars, prop->data);
			prop = g_slist_next (prop);
		}

		g_slist_free (variables);
	}

	for (i = varied->len - 1; i >= 0; --i)
	{
		Range *r = varied->pdata[i];
		GSList *variables = find_matching_variables (network, r->selector);
		GSList *prop = variables;

		while (prop)
		{
			if (include_header)
			{
				if (!first)
				{
					g_output_stream_write_all (out,
					                           delimiter,
					                           strlen (delimiter),
					                           NULL,
					                           NULL,
					                           NULL);
				}
				else
				{
					first = FALSE;
				}

				gchar *name = cdn_variable_get_full_name_for_display (prop->data);

				g_output_stream_write_all (out,
				                           name,
				                           strlen (name),
				                           NULL,
				                           NULL,
				                           NULL);

				g_free (name);
			}

			ranges = g_slist_prepend (ranges,
			                          variable_range_new (prop->data,
			                                              r));

			prop = g_slist_next (prop);
		}

		g_slist_free (variables);
	}

	if (include_header)
	{
		g_output_stream_write_all (out, "\n", 1, NULL, NULL, NULL);
	}

	// For all the combinations of values in ranges, simulate the network
	simulate_combinations (network, ranges, ranges, vars, out);

	g_slist_foreach (ranges, (GFunc)variable_range_free, NULL);
	g_slist_free (ranges);

	g_slist_free (vars);

	return 0;
}

static gint
monitor_network (gchar const *filename)
{
	CdnNetwork *network;
	GError *error = NULL;
	CdnCompileError *err;
	gint ret;

	if (g_strcmp0 (filename, "-") == 0)
	{
		GInputStream *stream = g_unix_input_stream_new (STDIN_FILENO, TRUE);
		network = cdn_network_new_from_stream (stream, &error);
		g_object_unref (stream);
	}
	else
	{
		GFile *file = g_file_new_for_commandline_arg (filename);
		network = cdn_network_new_from_file (file, &error);
		g_object_unref (file);
	}

	if (!network)
	{
		g_printerr ("Failed to load network `%s': %s\n", filename, error->message);
		g_error_free (error);

		return 1;
	}

	err = cdn_compile_error_new ();

	if (!cdn_object_compile (CDN_OBJECT (network), NULL, err))
	{
		gchar *msg;

		msg = cdn_compile_error_get_formatted_string (err);

		g_printerr ("Failed to compile network `%s'\n\n%s\n",
		            filename,
		            msg);

		g_free (msg);

		g_object_unref (network);
		g_object_unref (err);

		return 1;
	}

	g_object_unref (err);

	if (varied->len == 0)
	{
		ret = run_simple_monitor (network);
	}
	else
	{
		ret = run_varied_monitor (network);
	}

	g_object_unref (network);

	return ret;
}

static void
cleanup ()
{
	g_ptr_array_free (monitored, TRUE);
	g_free (delimiter);
	g_free (output_file);
	g_ptr_array_free (varied, TRUE);
}

int
main (int argc,
      char *argv[])
{
	GOptionContext *ctx;
	GError *error = NULL;
	gchar const *file;
	gint ret = 1;
	struct timeval tv;

	g_type_init ();

	gettimeofday (&tv, NULL);
	seed = tv.tv_sec * 1000 + tv.tv_usec / 1000;

	monitored = g_ptr_array_new ();
	varied = g_ptr_array_new_with_free_func ((GDestroyNotify)range_free);

	delimiter = g_strdup ("\t");

	ctx = g_option_context_new ("-m <SELECTOR> [-m ...] [-v RANGE...] [NETWORK] - monitor Codyn network");
	g_option_context_set_summary (ctx, "Omit the network name or use a dash '-' to read from standard input.");
	g_option_context_add_main_entries (ctx, entries, NULL);

	if (!g_option_context_parse (ctx, &argc, &argv, &error))
	{
		g_printerr ("Failed to parse options: %s\n", error->message);
		g_error_free (error);
		cleanup ();

		return 1;
	}

	if (argc == 1)
	{
		file = "-";
	}
	else if (argc == 2)
	{
		file = argv[1];
	}
	else
	{
		g_printerr ("Too many arguments. Please provide at most one network to monitor\n");
		cleanup ();

		return 1;
	}

	srand (seed);

	ret = monitor_network (file);

	cleanup ();

	return ret;
}
