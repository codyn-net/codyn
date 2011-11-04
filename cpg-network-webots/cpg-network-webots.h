/*
 * cpg-network-webots.h
 * This file is part of cpg-network
 *
 * Copyright (C) 2011 - Jesse van den Kieboom
 *
 * cpg-network is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * cpg-network is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with cpg-network; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, 
 * Boston, MA  02110-1301  USA
 */

#ifndef __CPG_NETWORK_WEBOTS_H__
#define __CPG_NETWORK_WEBOTS_H__

#include <glib-object.h>
#include <cpg-network/cpg-network.h>

G_BEGIN_DECLS

#define CPG_TYPE_NETWORK_WEBOTS			(cpg_network_webots_get_type ())
#define CPG_NETWORK_WEBOTS(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), CPG_TYPE_NETWORK_WEBOTS, CpgNetworkWebots))
#define CPG_NETWORK_WEBOTS_CONST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), CPG_TYPE_NETWORK_WEBOTS, CpgNetworkWebots const))
#define CPG_NETWORK_WEBOTS_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), CPG_TYPE_NETWORK_WEBOTS, CpgNetworkWebotsClass))
#define CPG_IS_NETWORK_WEBOTS(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), CPG_TYPE_NETWORK_WEBOTS))
#define CPG_IS_NETWORK_WEBOTS_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), CPG_TYPE_NETWORK_WEBOTS))
#define CPG_NETWORK_WEBOTS_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), CPG_TYPE_NETWORK_WEBOTS, CpgNetworkWebotsClass))

typedef struct _CpgNetworkWebots	CpgNetworkWebots;
typedef struct _CpgNetworkWebotsClass	CpgNetworkWebotsClass;
typedef struct _CpgNetworkWebotsPrivate	CpgNetworkWebotsPrivate;

struct _CpgNetworkWebots
{
	/*< private >*/
	GObject parent;

	CpgNetworkWebotsPrivate *priv;
};

struct _CpgNetworkWebotsClass
{
	/*< private >*/
	GObjectClass parent_class;
};

GType cpg_network_webots_get_type (void) G_GNUC_CONST;

CpgNetworkWebots *cpg_network_webots_new           (CpgNetwork       *network,
                                                    guint             basic_time_step);

void              cpg_network_webots_read_inputs (CpgNetworkWebots *webots);
void              cpg_network_webots_write_outputs (CpgNetworkWebots *webots);

G_END_DECLS

#endif /* __CPG_NETWORK_WEBOTS_H__ */

