/*
 * cdn-operator-df-dt.h
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

#ifndef __CDN_OPERATOR_DT_H__
#define __CDN_OPERATOR_DT_H__

#include <codyn/operators/cdn-operator.h>
#include <codyn/cdn-expression.h>

G_BEGIN_DECLS

#define CDN_TYPE_OPERATOR_DT		(cdn_operator_dt_get_type ())
#define CDN_OPERATOR_DT(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), CDN_TYPE_OPERATOR_DT, CdnOperatorDt))
#define CDN_OPERATOR_DT_CONST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), CDN_TYPE_OPERATOR_DT, CdnOperatorDt const))
#define CDN_OPERATOR_DT_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), CDN_TYPE_OPERATOR_DT, CdnOperatorDtClass))
#define CDN_IS_OPERATOR_DT(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), CDN_TYPE_OPERATOR_DT))
#define CDN_IS_OPERATOR_DT_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), CDN_TYPE_OPERATOR_DT))
#define CDN_OPERATOR_DT_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), CDN_TYPE_OPERATOR_DT, CdnOperatorDtClass))

typedef struct _CdnOperatorDt		CdnOperatorDt;
typedef struct _CdnOperatorDtClass	CdnOperatorDtClass;
typedef struct _CdnOperatorDtPrivate	CdnOperatorDtPrivate;

/**
 * CdnOperatorDt:
 *
 * Math operator for dt evaluation of an expression.
 *
 * The #CdnOperatorDt is a special operator that can be used in
 * mathematical expressions ('delay'). When evaluated, it will return the
 * dt value of its argument (which can be an arbitrary expression).
 */
struct _CdnOperatorDt
{
	/*< private >*/
	CdnOperator parent;

	CdnOperatorDtPrivate *priv;
};

struct _CdnOperatorDtClass
{
	/*< private >*/
	CdnOperatorClass parent_class;
};

GType          cdn_operator_dt_get_type       (void) G_GNUC_CONST;
CdnOperatorDt *cdn_operator_dt_new            (void);
gint           cdn_operator_dt_get_order      (CdnOperatorDt *dt);

G_END_DECLS

#endif /* __CDN_OPERATOR_DT_H__ */
