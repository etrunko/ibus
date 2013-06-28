/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/* vim:set et sts=4: */
/* ibus - The Input Bus
 * Copyright (C) 2013 Intel Corporation
 * Copyright (C) 2013 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include <ibus.h>
#include <xkbcommon/xkbcommon.h>
#include <wayland-client.h>
#include "input-method-client-protocol.h"
#include "waylandeventsource.h"

struct ibus_im {
    struct wl_input_method *input_method;
    struct wl_input_method_context *context;
    struct wl_keyboard *keyboard;

    IBusInputContext *ibuscontext;
    IBusText *preedit_text;
    guint preedit_cursor_pos;
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

    uint32_t serial;
};

struct wl_display *_display = NULL;
struct wl_registry *_registry = NULL;
static IBusBus *_bus = NULL;

static void
_context_commit_text_cb (IBusInputContext *context,
                         IBusText         *text,
                         struct ibus_im   *ibus_im)
{
    wl_input_method_context_commit_string (ibus_im->context,
                                           ibus_im->serial,
                                           text->text);
}

static void
_context_forward_key_event_cb (IBusInputContext *context,
                               guint             keyval,
                               guint             keycode,
                               guint             state,
                               struct ibus_im   *ibus_im)
{
    wl_input_method_context_keysym (ibus_im->context,
                                    ibus_im->serial,
                                    0,
                                    keyval,
                                    state,
                                    ibus_im->modifiers);
}

static void
_context_show_preedit_text_cb (IBusInputContext *context,
                               struct ibus_im   *ibus_im)
{
    uint32_t cursor =
        g_utf8_offset_to_pointer (ibus_im->preedit_text->text,
                                  ibus_im->preedit_cursor_pos) -
        ibus_im->preedit_text->text;

    wl_input_method_context_preedit_cursor (ibus_im->context,
                                            cursor);
    wl_input_method_context_preedit_string (ibus_im->context,
                                            ibus_im->serial,
                                            ibus_im->preedit_text->text,
                                            ibus_im->preedit_text->text);
}

static void
_context_hide_preedit_text_cb (IBusInputContext *context,
                               struct ibus_im   *ibus_im)
{
    wl_input_method_context_preedit_string (ibus_im->context,
                                            ibus_im->serial,
                                            "",
                                            "");
}

static void
_context_update_preedit_text_cb (IBusInputContext *context,
                                 IBusText         *text,
                                 gint              cursor_pos,
                                 gboolean          visible,
                                 struct ibus_im   *ibus_im)
{
    if (ibus_im->preedit_text)
        g_object_unref (ibus_im->preedit_text);
    ibus_im->preedit_text = g_object_ref_sink (text);
    ibus_im->preedit_cursor_pos = cursor_pos;

    if (visible)
        _context_show_preedit_text_cb (context, ibus_im);
    else
        _context_hide_preedit_text_cb (context, ibus_im);
}

static void
handle_surrounding_text (void *data,
                         struct wl_input_method_context *context,
                         const char *text,
                         uint32_t cursor,
                         uint32_t anchor)
{
}

static void
handle_reset (void *data,
              struct wl_input_method_context *context)
{
}

static void
handle_content_type (void *data,
                     struct wl_input_method_context *context,
                     uint32_t hint,
                     uint32_t purpose)
{
}

static void
handle_invoke_action (void *data,
                      struct wl_input_method_context *context,
                      uint32_t button,
                      uint32_t index)
{
}

static void
handle_commit_state (void *data,
                     struct wl_input_method_context *context,
                     uint32_t serial)
{
    struct ibus_im *ibus_im = data;

    ibus_im->serial = serial;
}

static void
handle_preferred_language (void *data,
                           struct wl_input_method_context *context,
                           const char *language)
{
}

static const struct wl_input_method_context_listener context_listener = {
    handle_surrounding_text,
    handle_reset,
    handle_content_type,
    handle_invoke_action,
    handle_commit_state,
    handle_preferred_language
};

static void
input_method_keyboard_keymap (void *data,
                              struct wl_keyboard *wl_keyboard,
                              uint32_t format,
                              int32_t fd,
                              uint32_t size)
{
    struct ibus_im *ibus_im = data;
    GMappedFile *map;
    GError *error;

    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        close(fd);
        return;
    }

    error = NULL;
    map = g_mapped_file_new_from_fd (fd, FALSE, &error);
    if (map == NULL) {
        close (fd);
        return;
    }

    ibus_im->keymap =
        xkb_map_new_from_string (ibus_im->xkb_context,
                                 g_mapped_file_get_contents (map),
                                 XKB_KEYMAP_FORMAT_TEXT_V1,
                                 0);

    g_mapped_file_unref (map);
    close(fd);

    if (!ibus_im->keymap) {
        return;
    }

    ibus_im->state = xkb_state_new (ibus_im->keymap);
    if (!ibus_im->state) {
        xkb_map_unref (ibus_im->keymap);
        return;
    }

    ibus_im->shift_mask =
        1 << xkb_map_mod_get_index (ibus_im->keymap, "Shift");
    ibus_im->lock_mask =
        1 << xkb_map_mod_get_index (ibus_im->keymap, "Lock");
    ibus_im->control_mask =
        1 << xkb_map_mod_get_index (ibus_im->keymap, "Control");
    ibus_im->mod1_mask =
        1 << xkb_map_mod_get_index (ibus_im->keymap, "Mod1");
    ibus_im->mod2_mask =
        1 << xkb_map_mod_get_index (ibus_im->keymap, "Mod2");
    ibus_im->mod3_mask =
        1 << xkb_map_mod_get_index (ibus_im->keymap, "Mod3");
    ibus_im->mod4_mask =
        1 << xkb_map_mod_get_index (ibus_im->keymap, "Mod4");
    ibus_im->mod5_mask =
        1 << xkb_map_mod_get_index (ibus_im->keymap, "Mod5");
}

static void
input_method_keyboard_key (void *data,
                           struct wl_keyboard *wl_keyboard,
                           uint32_t serial,
                           uint32_t time,
                           uint32_t key,
                           uint32_t state)
{
    struct ibus_im *ibus_im = data;
    uint32_t code;
    uint32_t num_syms;
    const xkb_keysym_t *syms;
    xkb_keysym_t sym;
    guint32 modifiers;
    gboolean retval;

    if (!ibus_im->state)
        return;

    code = key + 8;
    num_syms = xkb_key_get_syms (ibus_im->state, code, &syms);

    sym = XKB_KEY_NoSymbol;
    if (num_syms == 1)
        sym = syms[0];

    modifiers = ibus_im->modifiers;
    if (state == WL_KEYBOARD_KEY_STATE_RELEASED)
        modifiers |= IBUS_RELEASE_MASK;

    retval = ibus_input_context_process_key_event (ibus_im->ibuscontext,
                                                   sym,
                                                   code,
                                                   modifiers);
    if (!retval) {
        _context_forward_key_event_cb (ibus_im->ibuscontext,
                                       sym,
                                       code,
                                       modifiers,
                                       ibus_im);
    }
}

static void
input_method_keyboard_modifiers (void *data,
                                 struct wl_keyboard *wl_keyboard,
                                 uint32_t serial,
                                 uint32_t mods_depressed,
                                 uint32_t mods_latched,
                                 uint32_t mods_locked,
                                 uint32_t group)
{
    struct ibus_im *ibus_im = data;
    struct wl_input_method_context *context = ibus_im->context;
    xkb_mod_mask_t mask;

    xkb_state_update_mask (ibus_im->state, mods_depressed,
                           mods_latched, mods_locked, 0, 0, group);
    mask = xkb_state_serialize_mods (ibus_im->state,
                                     XKB_STATE_DEPRESSED |
                                     XKB_STATE_LATCHED);

    ibus_im->modifiers = 0;
    if (mask & ibus_im->shift_mask)
        ibus_im->modifiers |= IBUS_SHIFT_MASK;
    if (mask & ibus_im->lock_mask)
        ibus_im->modifiers |= IBUS_LOCK_MASK;
    if (mask & ibus_im->control_mask)
        ibus_im->modifiers |= IBUS_CONTROL_MASK;
    if (mask & ibus_im->mod1_mask)
        ibus_im->modifiers |= IBUS_MOD1_MASK;
    if (mask & ibus_im->mod2_mask)
        ibus_im->modifiers |= IBUS_MOD2_MASK;
    if (mask & ibus_im->mod3_mask)
        ibus_im->modifiers |= IBUS_MOD3_MASK;
    if (mask & ibus_im->mod4_mask)
        ibus_im->modifiers |= IBUS_MOD4_MASK;
    if (mask & ibus_im->mod5_mask)
        ibus_im->modifiers |= IBUS_MOD5_MASK;

    wl_input_method_context_modifiers (context, serial,
                                       mods_depressed, mods_depressed,
                                       mods_latched, group);
}

static const struct wl_keyboard_listener keyboard_listener = {
    input_method_keyboard_keymap,
    NULL, /* enter */
    NULL, /* leave */
    input_method_keyboard_key,
    input_method_keyboard_modifiers
};

static void
input_method_activate (void *data,
                       struct wl_input_method *input_method,
                       struct wl_input_method_context *context)
{
    struct ibus_im *ibus_im = data;

    if (ibus_im->ibuscontext)
        g_object_unref (ibus_im->ibuscontext);

    ibus_im->ibuscontext = ibus_bus_create_input_context (_bus, "wayland");

    g_signal_connect (ibus_im->ibuscontext, "commit-text",
                      G_CALLBACK (_context_commit_text_cb), ibus_im);
    g_signal_connect (ibus_im->ibuscontext, "forward-key-event",
                      G_CALLBACK (_context_forward_key_event_cb), ibus_im);

    g_signal_connect (ibus_im->ibuscontext, "update-preedit-text",
                      G_CALLBACK (_context_update_preedit_text_cb), ibus_im);
    g_signal_connect (ibus_im->ibuscontext, "show-preedit-text",
                      G_CALLBACK (_context_show_preedit_text_cb), ibus_im);
    g_signal_connect (ibus_im->ibuscontext, "hide-preedit-text",
                      G_CALLBACK (_context_hide_preedit_text_cb), ibus_im);
    
    ibus_input_context_set_capabilities (ibus_im->ibuscontext,
                                         IBUS_CAP_FOCUS |
                                         IBUS_CAP_PREEDIT_TEXT);

    ibus_input_context_focus_in (ibus_im->ibuscontext);

    if (ibus_im->context)
        wl_input_method_context_destroy (ibus_im->context);

    ibus_im->serial = 0;
    ibus_im->context = context;
    wl_input_method_context_add_listener (context, &context_listener, ibus_im);
    ibus_im->keyboard = wl_input_method_context_grab_keyboard (context);
    wl_keyboard_add_listener(ibus_im->keyboard, &keyboard_listener, ibus_im);
}

static void
input_method_deactivate (void *data,
                         struct wl_input_method *input_method,
                         struct wl_input_method_context *context)
{
    struct ibus_im *ibus_im = data;

    if (ibus_im->ibuscontext) {
        ibus_input_context_focus_out (ibus_im->ibuscontext);
        g_object_unref (ibus_im->ibuscontext);
        ibus_im->ibuscontext = NULL;
    }

    if (ibus_im->context) {
        wl_input_method_context_destroy (ibus_im->context);
        ibus_im->context = NULL;
    }
}

static const struct wl_input_method_listener input_method_listener = {
    input_method_activate,
    input_method_deactivate
};

static void
registry_handle_global (void *data,
                        struct wl_registry *registry,
                        uint32_t name,
                        const char *interface,
                        uint32_t version)
{
    struct ibus_im *ibus_im = data;

    if (!g_strcmp0 (interface, "wl_input_method")) {
        ibus_im->input_method =
            wl_registry_bind (registry, name,
                              &wl_input_method_interface, 1);
        wl_input_method_add_listener (ibus_im->input_method,
                                      &input_method_listener, ibus_im);
    }
}

static void
registry_handle_global_remove (void *data,
                               struct wl_registry *registry,
                               uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
    registry_handle_global,
    registry_handle_global_remove
};

int
main (int argc, char *argv[])
{
    struct ibus_im ibus_im;

    ibus_init ();

    _bus = ibus_bus_new ();
    if (!ibus_bus_is_connected (_bus)) {
        g_warning ("Cannot connect to ibus-daemon");
        return 1;
    }

    memset (&ibus_im, 0, sizeof (ibus_im));

    _display = wl_display_connect (NULL);
    if (_display == NULL) {
        g_warning ("Failed to connect to Wayland server: %m");
        return 1;
    }

    _registry = wl_display_get_registry (_display);
    wl_registry_add_listener (_registry, &registry_listener, &ibus_im);
    wl_display_roundtrip (_display);

    if (ibus_im.input_method == NULL) {
        g_warning ("No input_method global");
        return 1;
    }

    ibus_im.xkb_context = xkb_context_new (0);
    if (ibus_im.xkb_context == NULL) {
        g_warning ("Failed to create XKB context");
        return 1;
    }

    wayland_event_source_new (_display);
    ibus_main ();

    return 0;
}
