/*
 * cdn-parser-context.h
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

#ifndef __CDN_PARSER_CONTEXT_H__
#define __CDN_PARSER_CONTEXT_H__

#include <glib-object.h>
#include <codyn/cdn-network.h>
#include <codyn/cdn-variable.h>
#include <codyn/cdn-edge.h>
#include <codyn/cdn-function.h>
#include <codyn/cdn-function-polynomial.h>
#include <codyn/cdn-import.h>
#include <codyn/cdn-import-alias.h>
#include <codyn/cdn-selector.h>
#include <codyn/cdn-layout.h>
#include <codyn/cdn-attribute.h>
#include <codyn/cdn-event.h>
#include <codyn/cdn-io.h>

G_BEGIN_DECLS

#define CDN_TYPE_PARSER_CONTEXT			(cdn_parser_context_get_type ())
#define CDN_PARSER_CONTEXT(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), CDN_TYPE_PARSER_CONTEXT, CdnParserContext))
#define CDN_PARSER_CONTEXT_CONST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), CDN_TYPE_PARSER_CONTEXT, CdnParserContext const))
#define CDN_PARSER_CONTEXT_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), CDN_TYPE_PARSER_CONTEXT, CdnParserContextClass))
#define CDN_IS_PARSER_CONTEXT(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), CDN_TYPE_PARSER_CONTEXT))
#define CDN_IS_PARSER_CONTEXT_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), CDN_TYPE_PARSER_CONTEXT))
#define CDN_PARSER_CONTEXT_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), CDN_TYPE_PARSER_CONTEXT, CdnParserContextClass))

typedef struct _CdnParserContext	CdnParserContext;
typedef struct _CdnParserContextClass	CdnParserContextClass;
typedef struct _CdnParserContextPrivate	CdnParserContextPrivate;
typedef struct _CdnParserContextClassPrivate	CdnParserContextClassPrivate;

typedef struct _CdnFunctionArgumentSpec CdnFunctionArgumentSpec;

typedef enum
{
	CDN_PARSER_CONTEXT_SELECTOR_TYPE_OBJECT,
	CDN_PARSER_CONTEXT_SELECTOR_TYPE_NODE,
	CDN_PARSER_CONTEXT_SELECTOR_TYPE_EDGE,
	CDN_PARSER_CONTEXT_SELECTOR_TYPE_VARIABLE,
	CDN_PARSER_CONTEXT_SELECTOR_TYPE_ACTION,
	CDN_PARSER_CONTEXT_SELECTOR_TYPE_FUNCTION
} CdnParserContextDeleteType;

struct _CdnParserContext
{
	/*< private >*/
	GObject parent;

	CdnParserContextPrivate *priv;
};

struct _CdnParserContextClass
{
	/*< private >*/
	GObjectClass parent_class;
};

GType                  cdn_parser_context_get_type             (void) G_GNUC_CONST;

CdnParserContext      *cdn_parser_context_new                  (CdnNetwork                 *network);

void                   cdn_parser_context_set_emit             (CdnParserContext           *context,
                                                                gboolean                    emit);

CdnEmbeddedContext    *cdn_parser_context_get_embedded         (CdnParserContext           *context);
void                   cdn_parser_context_set_embedded         (CdnParserContext           *context,
                                                                CdnEmbeddedContext         *embedded);

GFile                 *cdn_parser_context_get_file             (CdnParserContext           *context);

void                   cdn_parser_context_set_line             (CdnParserContext           *context,
                                                                gchar const                *line,
                                                                gint                        lineno);

gchar const           *cdn_parser_context_get_line             (CdnParserContext           *context,
                                                                gint                       *lineno);

gchar const           *cdn_parser_context_get_line_at          (CdnParserContext           *context,
                                                                gint                        lineno);

void                   cdn_parser_context_set_column           (CdnParserContext           *context,
                                                                gint                        start,
                                                                gint                        end);

void                   cdn_parser_context_get_column           (CdnParserContext           *context,
                                                                gint                       *start,
                                                                gint                       *end);

void                   cdn_parser_context_get_error_location   (CdnParserContext           *context,
                                                                gint                       *lstart,
                                                                gint                       *lend,
                                                                gint                       *cstart,
                                                                gint                       *cend);

gchar                 *cdn_parser_context_get_error_lines      (CdnParserContext           *context);

void                   cdn_parser_context_get_last_selector_item_line (CdnParserContext *context,
                                                                       gint             *line_start,
                                                                       gint             *line_end);

void                   cdn_parser_context_get_last_selector_item_column (CdnParserContext *context,
                                                                         gint             *start,
                                                                         gint             *end);

void                   cdn_parser_context_set_token            (CdnParserContext           *context,
                                                                gchar const                *token);

gchar const           *cdn_parser_context_get_token            (CdnParserContext           *context);

gboolean               cdn_parser_context_parse                (CdnParserContext           *context,
                                                                gboolean                    push_network,
                                                                GError                    **error);

void                   cdn_parser_context_begin_selector_item  (CdnParserContext           *context);

void                   cdn_parser_context_add_variable         (CdnParserContext           *context,
                                                                CdnEmbeddedString          *name,
                                                                CdnEmbeddedString          *count_name,
                                                                CdnEmbeddedString          *expression,
                                                                CdnVariableFlags            add_flags,
                                                                CdnVariableFlags            remove_flags,
                                                                GSList                     *attributes,
                                                                gboolean                    assign_optional,
                                                                CdnEmbeddedString          *constraint);

void                   cdn_parser_context_set_variable         (CdnParserContext           *context,
                                                                CdnSelector                *selector,
                                                                CdnEmbeddedString          *expression,
                                                                CdnVariableFlags            add_flags,
                                                                CdnVariableFlags            remove_flags,
                                                                GSList                     *attributes);

void                   cdn_parser_context_add_action           (CdnParserContext           *context,
                                                                CdnEmbeddedString          *target,
                                                                CdnEmbeddedString          *expression,
                                                                GSList                     *attributes,
                                                                CdnEmbeddedString          *phases);

void                   cdn_parser_context_add_polynomial       (CdnParserContext           *context,
                                                                CdnEmbeddedString          *name,
                                                                GSList                     *pieces,
                                                                GSList                     *attributes);

void                   cdn_parser_context_add_interface        (CdnParserContext           *context,
                                                                CdnEmbeddedString          *name,
                                                                CdnEmbeddedString          *child_name,
                                                                CdnEmbeddedString          *property_name,
                                                                gboolean                    is_optional,
                                                                GSList                     *attributes);

void                   cdn_parser_context_import               (CdnParserContext           *context,
                                                                CdnEmbeddedString          *id,
                                                                CdnEmbeddedString          *path,
                                                                GSList                     *attributes);

void                   cdn_parser_context_set_error            (CdnParserContext           *context,
                                                                gchar const                *message);

GError                *cdn_parser_context_get_error            (CdnParserContext           *context);

void                   cdn_parser_context_push_selection       (CdnParserContext           *context,
                                                                CdnSelector                *selector,
                                                                CdnSelectorType             type,
                                                                GSList                     *templates,
                                                                GSList                     *attributes);

void                   cdn_parser_context_push_objects         (CdnParserContext           *context,
                                                                GSList                     *objects,
                                                                GSList                     *attributes);

void                   cdn_parser_context_push_node            (CdnParserContext           *context,
                                                                CdnEmbeddedString          *id,
                                                                GSList                     *templates,
                                                                GSList                     *attributes);

void                   cdn_parser_context_push_edge            (CdnParserContext           *context,
                                                                CdnEmbeddedString          *id,
                                                                GSList                     *templates,
                                                                GSList                     *attributes,
                                                                GSList                     *fromto,
                                                                CdnEmbeddedString          *phase);

void                   cdn_parser_context_push_network         (CdnParserContext           *context,
                                                                GSList                     *attributes);
void                   cdn_parser_context_push_templates       (CdnParserContext           *context,
                                                                GSList                     *attributes);
void                   cdn_parser_context_push_integrator      (CdnParserContext           *context,
                                                                GSList                     *attributes);

void                   cdn_parser_context_push_function        (CdnParserContext           *context,
                                                                CdnEmbeddedString          *id,
                                                                GSList                     *args,
                                                                CdnEmbeddedString          *expression,
                                                                gboolean                    optional,
                                                                GSList                     *attributes);

void                   cdn_parser_context_push_event           (CdnParserContext  *context,
                                                                CdnEmbeddedString *from_phase,
                                                                CdnEmbeddedString *to_phase,
                                                                CdnEmbeddedString *condition,
                                                                gboolean           terminal,
                                                                CdnEmbeddedString *approximation,
                                                                GSList            *templates,
                                                                GSList            *attributes);

void                   cdn_parser_context_push_io_type         (CdnParserContext  *context,
                                                                CdnIoMode          mode,
                                                                CdnEmbeddedString *id,
                                                                CdnEmbeddedString *type,
                                                                GSList            *attributes);

void                   cdn_parser_context_set_io_setting       (CdnParserContext  *context,
                                                                CdnEmbeddedString *name,
                                                                CdnEmbeddedString *value);

void                   cdn_parser_context_add_event_set_variable (CdnParserContext  *context,
                                                                  CdnSelector       *selector,
                                                                  CdnEmbeddedString *value);

void                   cdn_parser_context_set_proxy            (CdnParserContext           *context,
                                                                GSList                     *objects);

void                   cdn_parser_context_pop                  (CdnParserContext           *context);
GSList const          *cdn_parser_context_current_selections   (CdnParserContext           *context);
GSList const          *cdn_parser_context_previous_selections  (CdnParserContext           *context);

void                   cdn_parser_context_push_selector_identifier (CdnParserContext           *context,
                                                                    CdnEmbeddedString          *identifier);
void                   cdn_parser_context_push_selector_regex  (CdnParserContext           *context,
                                                                CdnEmbeddedString          *regex);
void                   cdn_parser_context_push_selector_pseudo (CdnParserContext           *context,
                                                                CdnSelectorPseudoType       type,
                                                                GSList                     *arguments);

CdnSelector           *cdn_parser_context_pop_selector         (CdnParserContext           *context);
CdnSelector           *cdn_parser_context_peek_selector        (CdnParserContext           *context);
void                   cdn_parser_context_push_selector        (CdnParserContext           *context,
                                                                gboolean                    with);

gssize                 cdn_parser_context_read                 (CdnParserContext           *context,
                                                                gchar                      *buffer,
                                                                gsize                       max_size);

gpointer               cdn_parser_context_get_scanner          (CdnParserContext           *context);

void                   cdn_parser_context_define               (CdnParserContext           *context,
                                                                CdnEmbeddedString          *name,
                                                                GObject                    *value,
                                                                gboolean                    optional,
                                                                CdnEmbeddedString          *count_name);

void                   cdn_parser_context_remove               (CdnParserContext           *context,
                                                                GSList                     *selectors);

void                   cdn_parser_context_set_integrator       (CdnParserContext           *context,
                                                                CdnEmbeddedString          *value);

void                   cdn_parser_context_push_input_from_path (CdnParserContext           *context,
                                                                CdnEmbeddedString          *filename,
                                                                GSList                     *attributes,
                                                                gboolean                    only_in_context);

void                   cdn_parser_context_push_input_from_string (CdnParserContext         *context,
                                                                  gchar const              *s,
                                                                  GSList                   *attributes,
                                                                  gboolean                  only_in_context);

void                   cdn_parser_context_push_input           (CdnParserContext           *context,
                                                                GFile                      *file,
                                                                GInputStream               *stream,
                                                                GSList                     *attributes);

void                   cdn_parser_context_include              (CdnParserContext           *context,
                                                                CdnEmbeddedString          *filename,
                                                                GSList                     *attributes);

void                   cdn_parser_context_link_library         (CdnParserContext           *context,
                                                                CdnEmbeddedString          *filename);

void                   cdn_parser_context_pop_input            (CdnParserContext           *context);

gint                   cdn_parser_context_steal_start_token    (CdnParserContext           *context);
gint                   cdn_parser_context_get_start_token      (CdnParserContext           *context);

void                   cdn_parser_context_set_start_token      (CdnParserContext           *context,
                                                                gint                        token);

void                   cdn_parser_context_push_annotation      (CdnParserContext           *context,
                                                                CdnEmbeddedString          *annotation);

void                   cdn_parser_context_push_scope           (CdnParserContext           *context,
                                                                GSList                     *attributes);

void                   cdn_parser_context_push_define          (CdnParserContext           *context,
                                                                GSList                     *attributes);

void                   cdn_parser_context_push_layout          (CdnParserContext           *context,
                                                                GSList                     *attributes);
void                   cdn_parser_context_pop_layout           (CdnParserContext           *context);

void                   cdn_parser_context_add_layout           (CdnParserContext           *context,
                                                                CdnLayoutRelation           relation,
                                                                CdnSelector                *left,
                                                                CdnSelector                *right);

void                   cdn_parser_context_add_layout_position  (CdnParserContext           *context,
                                                                CdnSelector                *selector,
                                                                CdnEmbeddedString          *x,
                                                                CdnEmbeddedString          *y,
                                                                CdnSelector                *of,
                                                                gboolean                    cartesian);

void                   cdn_parser_context_add_integrator_variable (CdnParserContext        *context,
                                                                   CdnEmbeddedString       *name,
                                                                   CdnEmbeddedString       *value);

CdnEmbeddedString     *cdn_parser_context_push_string           (CdnParserContext *context);
CdnEmbeddedString     *cdn_parser_context_peek_string           (CdnParserContext *context);
CdnEmbeddedString     *cdn_parser_context_pop_string            (CdnParserContext *context);

gboolean               cdn_parser_context_pop_equation_depth    (CdnParserContext *context);
void                   cdn_parser_context_push_equation_depth   (CdnParserContext *context);
gint                   cdn_parser_context_peek_equation_depth   (CdnParserContext *context);
void                   cdn_parser_context_push_equation         (CdnParserContext *context);

void                   cdn_parser_context_delete_selector       (CdnParserContext *context,
                                                                 CdnSelector      *selector);

void                   cdn_parser_context_debug_selector        (CdnParserContext *context,
                                                                 CdnSelector      *selector);

void                   cdn_parser_context_debug_string          (CdnParserContext  *context,
                                                                 CdnEmbeddedString *s);

void                   cdn_parser_context_debug_context         (CdnParserContext  *context);

void                   cdn_parser_context_apply_template        (CdnParserContext  *context,
                                                                 CdnSelector       *templates,
                                                                 CdnSelector       *targets);

void                   cdn_parser_context_unapply_template      (CdnParserContext  *context,
                                                                 CdnSelector       *templates,
                                                                 CdnSelector       *targets);

void                   cdn_parser_context_remove_record         (CdnParserContext  *context,
                                                                 gint               len,
                                                                 gint               offset);

gboolean               cdn_parser_context_get_first_eof         (CdnParserContext  *context);
void                   cdn_parser_context_set_first_eof         (CdnParserContext  *context,
                                                                 gboolean           firsteof);

CdnFunctionArgumentSpec *cdn_function_argument_spec_new        (CdnEmbeddedString *name,
                                                                gboolean           isexplicit,
                                                                CdnEmbeddedString *default_value);

void cdn_function_argument_spec_free (CdnFunctionArgumentSpec *spec);

G_END_DECLS

#endif /* __CDN_PARSER_CONTEXT_H__ */
