/*
 * cpg-utils.h
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

#ifndef __CPG_UTILS_H__
#define __CPG_UTILS_H__

#include <stdlib.h>
#include <glib-object.h>

#define CPG_FORWARD_DECL(id)    struct _##id
#define array_resize(Ptr, Type, Num) (Ptr = (Type *)realloc(Ptr, sizeof(Type) * (Num)))

gboolean cpg_signal_accumulator_false_handled (GSignalInvocationHint *ihint,
                                               GValue                *return_accu,
                                               const GValue          *handler_return,
                                               gpointer               dummy);


#endif /* __CPG_UTILS_H__ */
