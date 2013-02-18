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

#include "config.h"

#include "waylandeventsource.h"

typedef struct _WaylandEventSource {
  GSource source;
  GPollFD pfd;
  uint32_t mask;
  struct wl_display *display;
} WaylandEventSource;

static gboolean
wayland_event_source_prepare(GSource *base, gint *timeout)
{
  WaylandEventSource *source = (WaylandEventSource *) base;

  *timeout = -1;

  wl_display_flush(source->display);

  return FALSE;
}

static gboolean
wayland_event_source_check(GSource *base)
{
  WaylandEventSource *source = (WaylandEventSource *) base;

  if (source->pfd.revents & (G_IO_ERR | G_IO_HUP))
    g_error ("Lost connection to wayland compositor");

  return source->pfd.revents;
}

static gboolean
wayland_event_source_dispatch(GSource *base,
			      GSourceFunc callback,
			      gpointer data)
{
  WaylandEventSource *source = (WaylandEventSource *) base;

  if (source->pfd.revents)
    {
	wl_display_dispatch(source->display);
	source->pfd.revents = 0;
    }
  
  return TRUE;
}

static void
wayland_event_source_finalize (GSource *source)
{
}

static GSourceFuncs wl_source_funcs = {
  wayland_event_source_prepare,
  wayland_event_source_check,
  wayland_event_source_dispatch,
  wayland_event_source_finalize
};

GSource *
wayland_event_source_new (struct wl_display *display)
{
  GSource *source;
  WaylandEventSource *wl_source;

  source = g_source_new (&wl_source_funcs,
			 sizeof (WaylandEventSource));
  wl_source = (WaylandEventSource *) source;

  wl_source->display = display;
  wl_source->pfd.fd = wl_display_get_fd(display);
  wl_source->pfd.events = G_IO_IN | G_IO_ERR | G_IO_HUP;
  g_source_add_poll(source, &wl_source->pfd);

  g_source_set_priority (source, G_PRIORITY_DEFAULT);
  g_source_set_can_recurse (source, TRUE);
  g_source_attach (source, NULL);

  return source;
}

