/*
 * cdn-operator.h
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

#ifndef __CDN_OPERATOR_H__
#define __CDN_OPERATOR_H__

#include <glib-object.h>
#include <codyn/cdn-stack.h>
#include <codyn/cdn-function.h>

G_BEGIN_DECLS

#define CDN_TYPE_OPERATOR		(cdn_operator_get_type ())
#define CDN_OPERATOR(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), CDN_TYPE_OPERATOR, CdnOperator))
#define CDN_OPERATOR_CONST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), CDN_TYPE_OPERATOR, CdnOperator const))
#define CDN_OPERATOR_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), CDN_TYPE_OPERATOR, CdnOperatorClass))
#define CDN_IS_OPERATOR(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), CDN_TYPE_OPERATOR))
#define CDN_IS_OPERATOR_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), CDN_TYPE_OPERATOR))
#define CDN_OPERATOR_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), CDN_TYPE_OPERATOR, CdnOperatorClass))

typedef struct _CdnOperator		CdnOperator;
typedef struct _CdnOperatorClass	CdnOperatorClass;
typedef struct _CdnOperatorPrivate	CdnOperatorPrivate;

#define CDN_OPERATOR_ERROR (cdn_operator_error_quark ())

typedef enum
{
	CDN_OPERATOR_ERROR_UNSUPPORTED,
	CDN_OPERATOR_ERROR_INVALID
} CdnOperatorError;

GQuark            cdn_operator_error_quark      (void);

typedef CDN_FORWARD_DECL (CdnIntegrator) CdnIntegratorForward;

typedef CDN_FORWARD_DECL (CdnFunction) CdnFunctionForward;

typedef void (*CdnForeachFunctionFunc)(CdnFunctionForward *func,
                                       gpointer            userdata);

struct _CdnOperator
{
	GObject parent;

	CdnOperatorPrivate *priv;
};

struct _CdnOperatorClass
{
	/*< private >*/
	GObjectClass parent_class;

	gchar *name;

	/*< public >*/
	void             (*execute)     (CdnOperator     *op,
	                                 CdnStack        *stack);

	gboolean         (*initialize) (CdnOperator   *op,
	                                GSList const **expressions,
	                                gint           num_expressions,
	                                GSList const **indices,
	                                gint           num_indices,
	                                gint           num_arguments,
	                                GError       **error);

	gchar           *(*get_name) ();

	void             (*reset_cache) (CdnOperator     *op);

	void             (*reset)           (CdnOperator *op);

	void             (*step)            (CdnOperator *op,
	                                     CdnIntegratorForward *integrator,
	                                     gdouble      t,
	                                     gdouble      timestep);

	void             (*step_prepare)    (CdnOperator *op,
	                                     CdnIntegratorForward *integrator,
	                                     gdouble      t,
	                                     gdouble      timestep);

	void             (*step_evaluate)   (CdnOperator *op,
	                                     CdnIntegratorForward *integrator,
	                                     gdouble      t,
	                                     gdouble      timestep);

	gboolean         (*equal)           (CdnOperator *op,
	                                     CdnOperator *other);

	CdnFunctionForward *(*get_function)  (CdnOperator *op,
	                                      gint        *idx,
	                                      gint         numidx);

	void             (*foreach_function) (CdnOperator            *op,
	                                      CdnForeachFunctionFunc  func,
	                                      gpointer                userdata);

	CdnOperator     *(*copy)             (CdnOperator *src);
};

GType                cdn_operator_get_type                    (void) G_GNUC_CONST;

gboolean             cdn_operator_initialize                  (CdnOperator     *op,
                                                               GSList const   **expressions,
                                                               gint             num_expressions,
                                                               GSList const   **indices,
                                                               gint             num_indices,
                                                               gint             num_arguments,
                                                               GError         **error);

void                 cdn_operator_execute                     (CdnOperator     *op,
                                                               CdnStack        *stack);

void                 cdn_operator_reset_cache                 (CdnOperator     *op);

gchar const         *cdn_operator_get_name                    (CdnOperator      *op);
gchar const         *cdn_operator_get_class_name              (CdnOperatorClass *op);

GSList const       **cdn_operator_all_expressions             (CdnOperator     *op);

GSList const        *cdn_operator_get_expressions             (CdnOperator     *op,
                                                               gint             idx);

gint                 cdn_operator_num_expressions             (CdnOperator     *op);

GSList const       **cdn_operator_all_indices                 (CdnOperator     *op);

GSList const        *cdn_operator_get_indices                 (CdnOperator     *op,
                                                               gint             idx);

gint                 cdn_operator_num_indices                 (CdnOperator     *op);

gboolean             cdn_operator_equal                       (CdnOperator     *op,
                                                               CdnOperator     *other);

void                 cdn_operator_step                        (CdnOperator     *op,
                                                               CdnIntegratorForward *integrator,
                                                               gdouble          t,
                                                               gdouble          timestep);

void                 cdn_operator_step_prepare                (CdnOperator     *op,
                                                               CdnIntegratorForward*integrator,
                                                               gdouble          t,
                                                               gdouble          timestep);

void                 cdn_operator_step_evaluate               (CdnOperator     *op,
                                                               CdnIntegratorForward *integrator,
                                                               gdouble          t,
                                                               gdouble          timestep);

void                 cdn_operator_reset                       (CdnOperator     *op);

CdnOperator         *cdn_operator_copy                        (CdnOperator     *op);

gint                 cdn_operator_get_num_arguments           (CdnOperator     *op);
void                _cdn_operator_set_num_arguments           (CdnOperator     *op,
                                                               gint             num);

CdnFunction         *cdn_operator_get_primary_function        (CdnOperator     *op);

CdnFunction         *cdn_operator_get_function                (CdnOperator     *op,
                                                               gint            *idx,
                                                               gint             numidx);

void                 cdn_operator_foreach_function           (CdnOperator            *op,
                                                              CdnForeachFunctionFunc  func,
                                                              gpointer                userdata);

G_END_DECLS

#endif /* __CDN_OPERATOR_H__ */