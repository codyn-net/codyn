#include "cpg-network-writer.h"

#include <libxml/tree.h>

#include "cpg-function-polynomial.h"

extern int xmlIndentTreeOutput;

static void
properties_to_xml (xmlDocPtr   doc,
                   xmlNodePtr  parent,
                   GSList     *properties,
                   CpgObject  *template)
{
	GSList *item;
	
	for (item = properties; item; item = g_slist_next (item))
	{
		CpgProperty *property = (CpgProperty *)item->data;
		CpgExpression *expression = cpg_property_get_expression (property);
		
		// Check if property is different from template
		if (template)
		{
			CpgProperty *tprop = cpg_object_get_property (template, 
				                                          cpg_property_get_name (property));

			if (tprop && cpg_property_equal (property, tprop))
			{
				continue;
			}
		}

		xmlNodePtr node = xmlNewDocNode (doc, NULL, (xmlChar *)"property", NULL);
		xmlNewProp (node, (xmlChar *)"name", (xmlChar *)cpg_property_get_name (property));
		
		if (cpg_property_get_integrated (property))
		{
			xmlNewProp (node, (xmlChar *)"integrated", (xmlChar *)"yes");
		}
		
		if (cpg_property_get_hint (property) & CPG_PROPERTY_HINT_OUT)
		{
			xmlNewProp (node, (xmlChar *)"out", (xmlChar *)"yes");
		}
		
		if (cpg_property_get_hint (property) & CPG_PROPERTY_HINT_IN)
		{
			xmlNewProp (node, (xmlChar *)"in", (xmlChar *)"yes");
		}

		if (cpg_property_get_hint (property) & CPG_PROPERTY_HINT_ONCE)
		{
			xmlNewProp (node, (xmlChar *)"once", (xmlChar *)"yes");
		}
		
		xmlNodePtr text = xmlNewDocText (doc, (xmlChar *)cpg_expression_get_as_string (expression));
		
		xmlAddChild (node, text);
		xmlAddChild (parent, node);
	}
}

static xmlNodePtr
object_to_xml (xmlDocPtr     doc,
               xmlNodePtr    parent,
               CpgObject    *object,
               gchar const  *name)
{
	xmlNodePtr ptr = xmlNewDocNode (doc, NULL, (xmlChar *)name, NULL);
	
	xmlNewProp (ptr, (xmlChar *)"id", (xmlChar *)cpg_object_get_id (object));
	xmlAddChild (parent, ptr);
	
	CpgObject *template = NULL;
	g_object_get (G_OBJECT (object), "template", &template, NULL);
	
	if (template != NULL)
	{
		xmlNewProp (ptr, (xmlChar *)"ref", (xmlChar *)cpg_object_get_id (template));
		g_object_unref (template);
	}
	
	properties_to_xml (doc, ptr, cpg_object_get_properties (object), template);
	return ptr;
}

static xmlNodePtr
relay_to_xml (xmlDocPtr   doc,
              xmlNodePtr  parent,
              CpgRelay   *relay)
{
	return object_to_xml (doc, parent, CPG_OBJECT (relay), "relay");
}

static xmlNodePtr
state_to_xml (xmlDocPtr   doc,
              xmlNodePtr  parent,
              CpgState   *state)
{
	return object_to_xml (doc, parent, CPG_OBJECT (state), "state");
}

static gboolean
template_has_action (CpgLink       *template,
                     CpgLinkAction *action)
{
	if (!template)
	{
		return FALSE;
	}
	
	GSList *item;
	CpgProperty *p1 = cpg_link_action_get_target (action);
	CpgExpression *e1 = cpg_link_action_get_expression (action);
	
	for (item = cpg_link_get_actions (template); item; item = g_slist_next (item))
	{
		CpgLinkAction *other = (CpgLinkAction *)item->data;

		CpgProperty *p2 = cpg_link_action_get_target (other);
		CpgExpression *e2 = cpg_link_action_get_expression (other);
		
		if (g_strcmp0 (cpg_property_get_name (p1), 
		               cpg_property_get_name (p2)) == 0 &&
		    cpg_expression_equal (e1, e2))
		{
			return TRUE;
		}
	}
	
	return FALSE;
}

static xmlNodePtr
link_to_xml (xmlDocPtr   doc,
             xmlNodePtr  parent,
             CpgLink    *link)
{
	CpgObject *from = cpg_link_get_from (link);
	CpgObject *to = cpg_link_get_to (link);
	
	xmlNodePtr node = object_to_xml (doc, parent, CPG_OBJECT (link), "link");
	
	if (from != NULL)
	{
		xmlNewProp (node, (xmlChar *)"from", (xmlChar *)cpg_object_get_id (from));
	}
	
	if (to != NULL)
	{
		xmlNewProp (node, (xmlChar *)"to", (xmlChar *)cpg_object_get_id (to));
	}
	
	// Link actions
	GSList *item;
	
	CpgObject *template = NULL;
	g_object_get (G_OBJECT (link), "template", &template, NULL);
	
	for (item = cpg_link_get_actions (link); item; item = g_slist_next (item))
	{
		CpgLinkAction *action = (CpgLinkAction *)(item->data);
		
		if (template_has_action (CPG_LINK (template), action))
		{
			continue;
		}
		
		xmlNodePtr ac = xmlNewDocNode (doc, NULL, (xmlChar *)"action", NULL);
		xmlNewProp (ac, (xmlChar *)"target", (xmlChar *)cpg_property_get_name (cpg_link_action_get_target (action)));
		
		xmlNodePtr text = xmlNewDocText (doc, (xmlChar *)cpg_expression_get_as_string (cpg_link_action_get_expression (action)));
		
		xmlAddChild (ac, text);
		xmlAddChild (node, ac);
	}
	
	if (template)
	{
		g_object_unref (template);
	}
	
	return node;
}

static void
write_function (CpgNetwork  *network,
                xmlDocPtr    doc,
                CpgFunction *func,
                xmlNodePtr   funcs)
{
	xmlNodePtr funcn = xmlNewDocNode (doc, NULL, (xmlChar *)"function", NULL);
	xmlNewProp (funcn,
	            (xmlChar *)"name",
	            (xmlChar *)cpg_object_get_id (CPG_OBJECT (func)));

	xmlAddChild (funcs, funcn);

	/* Create expression element */
	CpgExpression *expression = cpg_function_get_expression (func);

	if (expression)
	{
		xmlNodePtr exprn = xmlNewDocNode (doc, NULL, (xmlChar *)"expression", NULL);
		xmlNodePtr text = xmlNewDocText (doc,
		                                 (xmlChar *)cpg_expression_get_as_string (expression));

		xmlAddChild (exprn, text);
		xmlAddChild (funcn, exprn);
	}

	/* Create argument elements */
	GList *args = cpg_function_get_arguments (func);
	GList *argitem;

	for (argitem = args; argitem; argitem = g_list_next (argitem))
	{
		CpgFunctionArgument *argument = (CpgFunctionArgument *)argitem->data;

		xmlNodePtr argn = xmlNewDocNode (doc, NULL, (xmlChar *)"argument", NULL);
		xmlNodePtr text = xmlNewDocText (doc,
		                                 (xmlChar *)cpg_function_argument_get_name (argument));

		xmlAddChild (argn, text);

		if (cpg_function_argument_get_optional (argument))
		{
			gchar defPtr[G_ASCII_DTOSTR_BUF_SIZE];

			xmlNewProp (argn, (xmlChar *)"optional", (xmlChar *)"yes");

			g_ascii_dtostr (defPtr,
			                G_ASCII_DTOSTR_BUF_SIZE,
			                cpg_function_argument_get_default_value (argument));
			xmlNewProp (argn, (xmlChar *)"default", (xmlChar *)defPtr);
		}

		xmlAddChild (funcn, argn);
	}
}

static void
write_function_polynomial (CpgNetwork            *network,
                           xmlDocPtr              doc,
                           CpgFunctionPolynomial *func,
                           xmlNodePtr             funcs)
{
	xmlNodePtr funcn = xmlNewDocNode (doc, NULL, (xmlChar *)"polynomial", NULL);
	xmlNewProp (funcn,
	            (xmlChar *)"name",
	            (xmlChar *)cpg_object_get_id (CPG_OBJECT (func)));

	xmlAddChild (funcs, funcn);

	/* Create pieces */
	GSList *pieces = cpg_function_polynomial_get_pieces (func);

	while (pieces)
	{
		CpgFunctionPolynomialPiece *piece = (CpgFunctionPolynomialPiece *)pieces->data;

		gchar beginPtr[G_ASCII_DTOSTR_BUF_SIZE];
		gchar endPtr[G_ASCII_DTOSTR_BUF_SIZE];


		
		g_ascii_dtostr (beginPtr,
		                G_ASCII_DTOSTR_BUF_SIZE,
		                cpg_function_polynomial_piece_get_begin (piece));

		g_ascii_dtostr (endPtr,
		                G_ASCII_DTOSTR_BUF_SIZE,
		                cpg_function_polynomial_piece_get_end (piece));

		xmlNodePtr piecen = xmlNewDocNode (doc, NULL, (xmlChar *)"piece", NULL);
		xmlNewProp (piecen, (xmlChar *)"begin", (xmlChar *)beginPtr);
		xmlNewProp (piecen, (xmlChar *)"end", (xmlChar *)endPtr);

		GString *str = g_string_new ("");
		guint num;
		guint i;
		gdouble *coefficients = cpg_function_polynomial_piece_get_coefficients (piece,
		                                                                        &num);

		for (i = 0; i < num; ++i)
		{
			gchar coefPtr[G_ASCII_DTOSTR_BUF_SIZE];

			if (i != 0)
			{
				g_string_append (str, ", ");
			}

			g_ascii_dtostr (coefPtr, G_ASCII_DTOSTR_BUF_SIZE, coefficients[i]);
			g_string_append (str, coefPtr);
		}

		xmlNodePtr text = xmlNewDocText (doc,
		                                 (xmlChar *)str->str);

		g_string_free (str, TRUE);

		xmlAddChild (piecen, text);
		xmlAddChild (funcn, piecen);

		pieces = g_slist_next (pieces);
	}
}

static void
write_functions (CpgNetwork *network,
                 xmlDocPtr   doc,
                 xmlNodePtr  nnetwork)
{
	GSList *functions = cpg_network_get_functions (network);
	GSList *item;

	if (functions == NULL)
	{
		return;
	}

	xmlNodePtr funcs = xmlNewDocNode (doc, NULL, (xmlChar *)"functions", NULL);
	xmlAddChild (nnetwork, funcs);

	for (item = functions; item; item = g_slist_next (item))
	{
		CpgFunction *func = CPG_FUNCTION (item->data);

		if (CPG_IS_FUNCTION_POLYNOMIAL (func))
		{
			write_function_polynomial (network, doc, CPG_FUNCTION_POLYNOMIAL (func), funcs);
		}
		else
		{
			write_function (network, doc, func, funcs);
		}
	}
}

static void
write_config (xmlDocPtr   doc,
              CpgNetwork *network,
              xmlNodePtr  nnetwork)
{
	CpgIntegrator *integrator = cpg_network_get_integrator (network);

	if (integrator != NULL)
	{
		xmlNewProp (nnetwork,
		            (xmlChar *)"integrator",
		            (xmlChar *)cpg_object_get_id (CPG_OBJECT (integrator)));
	}
}

gchar * 
cpg_network_writer_xml_string (CpgNetwork *network)
{
	xmlDocPtr doc = xmlNewDoc ((xmlChar *)"1.0");
	xmlNodePtr root = xmlNewDocNode (doc, NULL, (xmlChar *)"cpg", NULL);

	xmlDocSetRootElement (doc, root);
	
	xmlNodePtr nnetwork = xmlNewDocNode (doc, NULL, (xmlChar *)"network", NULL);
	xmlAddChild (root, nnetwork);

	write_config (doc, network, nnetwork);

	// Globals
	GSList *properties = cpg_object_get_properties (cpg_network_get_globals (network));

	if (properties)
	{	
		xmlNodePtr gbl = xmlNewDocNode (doc, NULL, (xmlChar *)"globals", NULL);
		xmlAddChild (nnetwork, gbl);
		
		properties_to_xml (doc, gbl, properties, NULL);
	}

	// Generate templates
	GSList *list = cpg_network_get_templates (network);
	xmlNodePtr templates;
	
	if (list)
	{
		templates = xmlNewDocNode (doc, NULL, (xmlChar *)"templates", NULL);
		xmlAddChild (nnetwork, templates);
	}

	GSList *item;

	for (item = list; item; item = g_slist_next (item))
	{
		gchar const *name = (gchar const *)item->data;
		CpgObject *template = cpg_network_get_template (network, name);
		
		if (CPG_IS_RELAY (template))
		{
			relay_to_xml (doc, templates, CPG_RELAY (template));
		}
		else if (CPG_IS_STATE (template))
		{
			state_to_xml (doc, templates, CPG_STATE (template));
		}
		else if (CPG_IS_LINK (template))
		{
			link_to_xml (doc, templates, CPG_LINK (template));
		}
	}
	
	g_slist_foreach (list, (GFunc)g_free, NULL);
	g_slist_free (list);

	// Generate state, relay and link nodes
	for (item = cpg_network_get_states (network); item; item = g_slist_next (item))
	{
		if (CPG_IS_RELAY (item->data))
		{
			relay_to_xml (doc, nnetwork, CPG_RELAY (item->data));
		}
		else if (CPG_IS_STATE (item->data))
		{
			state_to_xml (doc, nnetwork, CPG_STATE (item->data));
		}
	}
	
	for (item = cpg_network_get_links (network); item; item = g_slist_next (item))
	{
		link_to_xml (doc, nnetwork, CPG_LINK (item->data));
	}

	write_functions (network, doc, nnetwork);

	xmlIndentTreeOutput = 1;
	
	xmlChar *mem;
	int size;
	
	xmlDocDumpFormatMemoryEnc (doc,
	                           &mem,
	                           &size,
	                           xmlGetCharEncodingName (XML_CHAR_ENCODING_UTF8),
	                           1);
	
	gchar *ret = g_strndup ((gchar const *)mem, size);
	xmlFree (mem);
	xmlFreeDoc (doc);
	
	return ret;
}

gboolean 
cpg_network_writer_xml (CpgNetwork  *network,
                        gchar const *filename)
{
	gchar *contents = cpg_network_writer_xml_string (network);

	if (contents == NULL)
	{
		return FALSE;
	}

	gboolean ret = g_file_set_contents (filename, contents, -1, NULL);
	g_free (contents);
	
	return ret;
}

