/*
 * cdn-io-wii.h
 * This file is part of codyn
 *
 * Copyright (C) 2012 - Jesse van den Kieboom
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef __CDN_IO_WII_REGISTER_H__
#define __CDN_IO_WII_REGISTER_H__

#include <glib-object.h>
#include <gmodule.h>

G_BEGIN_DECLS

G_MODULE_EXPORT void cdn_io_register_types (GTypeModule *module);

G_END_DECLS

#endif /* __CDN_IO_WII_REGISTER_H__ */

