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

#include "waylandproxy.h"

#include "ibusimpl.h"
#include "waylandeventsource.h"
#include "types.h"

#include <wayland-client.h>
#include "input-method-client-protocol.h"

#include <string.h>
#include <sys/mman.h>
#include <xkbcommon/xkbcommon.h>

struct _BusWaylandProxy
{
    GObject parent;

    struct wl_input_method_context *context;
    BusInputContext *bus_context;

    struct wl_keyboard *keyboard;

    IBusModifierType modifiers;

    struct xkb_context *xkb_context;

    struct xkb_keymap *keymap;
    struct xkb_state *state;

    xkb_mod_mask_t shift_mask;
    xkb_mod_mask_t lock_mask;
    xkb_mod_mask_t control_mask;
    xkb_mod_mask_t mod1_mask;
    xkb_mod_mask_t mod2_mask;
    xkb_mod_mask_t mod3_mask;
    xkb_mod_mask_t mod4_mask;
    xkb_mod_mask_t mod5_mask;

    gboolean destroyed;

    gchar *surrounding_text;
    uint32_t cursor;
    uint32_t anchor;

    uint32_t serial;
};

struct _BusWaylandProxyClass
{
    GObjectClass parent;
};

struct _BusWaylandKeyEvent
{
    struct wl_input_method_context *context;
    uint32_t serial;
    uint32_t time;
    uint32_t key;
    uint32_t sym;
    enum wl_keyboard_key_state state;
};

G_DEFINE_TYPE (BusWaylandProxy, bus_wayland_proxy, G_TYPE_OBJECT);

static void
bus_wayland_proxy_class_init(BusWaylandProxyClass *klass)
{
}

static void
bus_wayland_proxy_init(BusWaylandProxy *self)
{
        self->xkb_context = xkb_context_new(0);
}

static void
handle_surrounding_text (void *data,
                         struct wl_input_method_context *context,
                         const char *text,
                         uint32_t cursor,
                         uint32_t anchor)
{
        BusWaylandProxy *proxy = data;

        if (proxy->surrounding_text)
                g_free (proxy->surrounding_text);

        proxy->surrounding_text = g_strdup (text);
        proxy->cursor = cursor;
        proxy->anchor = anchor;
}

static void
handle_reset (void *data,
              struct wl_input_method_context *context)
{
        BusWaylandProxy *proxy = data;

        bus_input_context_reset (proxy->bus_context);
}

static void
handle_content_type (void *data,
                     struct wl_input_method_context *context,
		     uint32_t hint,
		     uint32_t purpose)
{
}

static void
handle_invoke_action(void *data,
		     struct wl_input_method_context *context,
		     uint32_t button,
		     uint32_t index)
{
}

static void
handle_commit_state(void *data,
                    struct wl_input_method_context *context,
                    uint32_t serial)
{
        BusWaylandProxy *proxy = data;
        IBusText *text;
        glong cursor, anchor;

        if (proxy->surrounding_text) {
                text = ibus_text_new_from_string (proxy->surrounding_text);

                cursor = g_utf8_strlen(proxy->surrounding_text, proxy->cursor);
                anchor = g_utf8_strlen(proxy->surrounding_text, proxy->anchor);

                bus_input_context_set_surrounding_text (proxy->bus_context,
                                                        text,
                                                        cursor,
                                                        anchor);

                if (g_object_is_floating (text))
                        g_object_unref (text);
        }

        proxy->serial = serial;
}

static void
handle_preferred_language(void *data,
			  struct wl_input_method_context *context,
			  const char *language)
{
}

static const struct wl_input_method_context_listener input_method_context_listener = {
	handle_surrounding_text,
	handle_reset,
	handle_content_type,
	handle_invoke_action,
	handle_commit_state,
	handle_preferred_language
};


static void
handle_keyboard_keymap(void *data,
                       struct wl_keyboard *wl_keyboard,
                       uint32_t format,
                       int32_t fd,
                       uint32_t size)
{
	BusWaylandProxy *context = data;
	char *map_str;

	if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
		close(fd);
		return;
	}

	map_str = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
	if (map_str == MAP_FAILED) {
		close(fd);
		return;
	}

	context->keymap =
		xkb_map_new_from_string(context->xkb_context,
					map_str,
					XKB_KEYMAP_FORMAT_TEXT_V1,
					0);

	munmap(map_str, size);
	close(fd);

	if (!context->keymap) {
		return;
	}

	context->state = xkb_state_new(context->keymap);
	if (!context->state) {
		xkb_map_unref(context->keymap);
		return;
	}

        context->shift_mask =
		1 << xkb_map_mod_get_index(context->keymap, "Shift");
        context->lock_mask =
		1 << xkb_map_mod_get_index(context->keymap, "Lock");
        context->control_mask =
		1 << xkb_map_mod_get_index(context->keymap, "Control");
        context->mod1_mask =
		1 << xkb_map_mod_get_index(context->keymap, "Mod1");
        context->mod2_mask =
		1 << xkb_map_mod_get_index(context->keymap, "Mod2");
        context->mod3_mask =
		1 << xkb_map_mod_get_index(context->keymap, "Mod3");
        context->mod4_mask =
		1 << xkb_map_mod_get_index(context->keymap, "Mod4");
        context->mod5_mask =
		1 << xkb_map_mod_get_index(context->keymap, "Mod5");

}

static void
handle_keyboard_key(void *data,
                    struct wl_keyboard *wl_keyboard,
		    uint32_t serial,
		    uint32_t time,
		    uint32_t key,
		    uint32_t state_w)
{
	BusWaylandProxy *proxy = data;
	uint32_t code;
	uint32_t num_syms;
	const xkb_keysym_t *syms;
	xkb_keysym_t sym;
	enum wl_keyboard_key_state state = state_w;
        guint modifiers;
        BusWaylandKeyEvent *event;

	if (!proxy->state)
		return;

	code = key + 8;
	num_syms = xkb_key_get_syms(proxy->state, code, &syms);

	sym = XKB_KEY_NoSymbol;
	if (num_syms == 1)
		sym = syms[0];

        modifiers = proxy->modifiers;
        if (state == WL_KEYBOARD_KEY_STATE_RELEASED)
                modifiers |= IBUS_RELEASE_MASK;


        event = g_new(BusWaylandKeyEvent, 1);

        event->context = proxy->context;
        event->serial = serial;
        event->time = time;
        event->key = key;
        event->sym = sym;
        event->state = state;

        bus_input_context_process_wayland_key_event(proxy->bus_context,
                                                    sym,
                                                    code,
                                                    modifiers,
                                                    event);
}

static void
handle_keyboard_modifiers(void *data,
				struct wl_keyboard *wl_keyboard,
				uint32_t serial,
				uint32_t mods_depressed,
				uint32_t mods_latched,
				uint32_t mods_locked,
				uint32_t group)
{
        BusWaylandProxy *proxy = data;
        xkb_mod_mask_t mask;

	xkb_state_update_mask(proxy->state, mods_depressed,
			      mods_latched, mods_locked, 0, 0, group);

        mask = xkb_state_serialize_mods(proxy->state,
                        XKB_STATE_DEPRESSED |
                        XKB_STATE_LATCHED);

        proxy->modifiers = 0;
        if (mask & proxy->shift_mask)
                proxy->modifiers |= IBUS_SHIFT_MASK;
        if (mask & proxy->lock_mask)
                proxy->modifiers |= IBUS_LOCK_MASK;
        if (mask & proxy->control_mask)
                proxy->modifiers |= IBUS_CONTROL_MASK;
        if (mask & proxy->mod1_mask)
                proxy->modifiers |= IBUS_MOD1_MASK;
        if (mask & proxy->mod2_mask)
                proxy->modifiers |= IBUS_MOD2_MASK;
        if (mask & proxy->mod3_mask)
                proxy->modifiers |= IBUS_MOD3_MASK;
        if (mask & proxy->mod4_mask)
                proxy->modifiers |= IBUS_MOD4_MASK;
        if (mask & proxy->mod5_mask)
                proxy->modifiers |= IBUS_MOD5_MASK;

	wl_input_method_context_modifiers(proxy->context, serial,
                                          mods_depressed, mods_depressed,
                                          mods_latched, group);
}

static const struct wl_keyboard_listener input_method_keyboard_listener = {
	handle_keyboard_keymap,
	NULL, /* enter */
	NULL, /* leave */
	handle_keyboard_key,
	handle_keyboard_modifiers
};

void
bus_wayland_proxy_update_preedit_text (BusWaylandProxy *proxy,
                                       IBusText        *text,
                                       gint             cursor_pos,
                                       gboolean         visible)
{
    const gchar *string;
    int32_t cursor;

    if (proxy->destroyed)
            return;

    string = ibus_text_get_text (text);

    cursor = g_utf8_offset_to_pointer (string, cursor_pos) - string;

    wl_input_method_context_preedit_cursor (proxy->context, cursor);
    wl_input_method_context_preedit_string (proxy->context, proxy->serial, string, string);
}

void
bus_wayland_proxy_commit_text (BusWaylandProxy *proxy, IBusText *text)
{
    if (proxy->destroyed)
            return;

   wl_input_method_context_commit_string (proxy->context, proxy->serial, ibus_text_get_text (text));
}

static BusWaylandProxy *
bus_wayland_proxy_new (struct wl_input_method_context *context,
                       BusInputContext *bus_context)
{
        BusWaylandProxy *c;

        c = g_object_new (BUS_TYPE_WAYLAND_PROXY, NULL);
        c->context = context;
        c->bus_context = bus_context;

        wl_input_method_context_add_listener(c->context,
					  &input_method_context_listener,
					  c);

        c->keyboard = wl_input_method_context_grab_keyboard(c->context);
	wl_keyboard_add_listener(c->keyboard,
				 &input_method_keyboard_listener,
				 c);
        
        bus_input_context_set_capabilities (c->bus_context,
                        IBUS_CAP_PREEDIT_TEXT | IBUS_CAP_FOCUS | IBUS_CAP_SURROUNDING_TEXT);

        return c;
}

void
bus_wayland_key_event_processed (BusWaylandKeyEvent *event,
                                 gboolean            processed)
{
    if (!processed)
        wl_input_method_context_key(event->context,
                                    event->serial,
                                    event->time,
                                    event->key,
                                    event->state);

    g_free (event);
}

void
bus_wayland_key_event_error (BusWaylandKeyEvent *event,
                             GError             *error)
{
    wl_input_method_context_key(event->context,
                                event->serial,
                                event->time,
                                event->key,
                                event->state);

    g_free (event);
}

static void
bus_wayland_proxy_deactivate (BusWaylandProxy *proxy)
{
    proxy->destroyed = TRUE;
    ibus_object_destroy (IBUS_OBJECT (proxy->bus_context));
}

static void
input_method_activate(void *data,
		      struct wl_input_method *input_method,
		      struct wl_input_method_context *context)
{
	BusInputContext *bus_context;
        BusWaylandProxy *proxy;

	bus_context = bus_ibus_impl_create_wayland_input_context (bus_ibus_impl_get_default ());

	proxy = bus_wayland_proxy_new (context,
                                       bus_context);

        bus_input_context_set_wayland_proxy (bus_context,
                                             proxy);

	bus_input_context_focus_in (bus_context);
}

static void
input_method_deactivate(void *data,
			struct wl_input_method *input_method,
			struct wl_input_method_context *context)
{
        BusWaylandProxy *proxy = wl_input_method_context_get_user_data (context);

        bus_wayland_proxy_deactivate (proxy);
        
        wl_input_method_context_destroy (context);
}

static const struct wl_input_method_listener input_method_listener = {
	input_method_activate,
	input_method_deactivate
};

static void
registry_handle_global(void *data, struct wl_registry *registry, uint32_t id,
		       const char *interface, uint32_t version)
{
	if (strcmp (interface, "wl_input_method") == 0) {
		struct wl_input_method *input_method = wl_registry_bind(registry, id, &wl_input_method_interface, 1);
		wl_input_method_add_listener(input_method, &input_method_listener, NULL);
	}
}

static void
registry_handle_global_remove(void *data, struct wl_registry *registry,
			      uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
	registry_handle_global,
	registry_handle_global_remove
};

void
bus_wayland_support_run (void)
{
	struct wl_display *display;
	struct wl_registry *registry;

	display = wl_display_connect (NULL);
	if (display == NULL)
		return;

	registry = wl_display_get_registry (display);
	wl_registry_add_listener(registry, &registry_listener, NULL);

	wayland_event_source_new (display);
}
