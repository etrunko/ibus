/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/* vim:set et sts=4: */
/* ibus - The Input Bus
 * Copyright (C) 2013 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __BUS_WAYLAND_PROXY_H
#define __BUS_WAYLAND_PROXY_H

#include <glib-object.h>

#include <ibus.h>

#define BUS_TYPE_WAYLAND_PROXY             \
	(bus_wayland_proxy_get_type ())
#define BUS_WAYLAND_PROXY(obj)             \
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), BUS_TYPE_WAYLAND_PROXY, BusWaylandProxy))
#define BUS_WAYLAND_PROXY_CLASS(klass)     \
	(G_TYPE_CHECK_CLASS_CAST ((klass), BUS_TYPE_WAYLAND_PROXY, BusWaylandProxyClass))
#define BUS_IS_WAYLAND_PROXY(obj)          \
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), BUS_TYPE_WAYLAND_PROXY))
#define BUS_IS_WAYLAND_PROXY_CLASS(klass)  \
	(G_TYPE_CHECK_CLASS_TYPE ((klass), BUS_TYPE_WAYLAND_PROXY))
#define BUS_WAYLAND_PROXY_GET_CLASS(obj)   \
	(G_TYPE_INSTANCE_GET_CLASS ((obj), BUS_TYPE_WAYLAND_PROXY, BusWaylandProxyClass))

G_BEGIN_DECLS

typedef struct _BusWaylandProxy BusWaylandProxy;
typedef struct _BusWaylandProxyClass BusWaylandProxyClass;

typedef struct _BusWaylandKeyEvent BusWaylandKeyEvent;

GType            bus_wayland_proxy_get_type (void);

void bus_wayland_proxy_update_preedit_text (BusWaylandProxy *proxy,
                                            IBusText        *text,
                                            gint             cursor_pos,
                                            gboolean         visible);
void bus_wayland_proxy_commit_text         (BusWaylandProxy *proxy,
                                            IBusText        *text);

void bus_wayland_key_event_processed (BusWaylandKeyEvent *event,
                                      gboolean            processed);
void bus_wayland_key_event_error     (BusWaylandKeyEvent *event,
                                      GError             *error);

void bus_wayland_support_run (void);

G_END_DECLS



#endif
