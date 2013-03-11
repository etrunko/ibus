
#include <gtk/gtk.h>
#include <gdk/gdkwayland.h>

#include <string.h>

#include "input-method-client-protocol.h"

static struct wl_input_panel *input_panel = 0;

static void
registry_handle_global(void *data, struct wl_registry *registry,
		       uint32_t name, const char *interface, uint32_t version)
{
	if (strcmp(interface, "wl_input_panel") == 0) {
		input_panel = wl_registry_bind(registry, name,
					       &wl_input_panel_interface, 1);
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

static void
ensure_input_panel(void)
{
	if (input_panel)
		return;

	GdkDisplay *gdk_display = gdk_display_get_default();
	struct wl_display *wl_display =	gdk_wayland_display_get_wl_display(gdk_display);

	struct wl_registry *wl_registry = wl_display_get_registry(wl_display);
	wl_registry_add_listener(wl_registry, 
				 &registry_listener, NULL);

	wl_display_roundtrip(wl_display);

	g_assert (input_panel);
}

static void
input_panel_realize_cb (GtkWidget *widget, gpointer data)
{
	GdkWindow *window;
	struct wl_surface *surface;
	struct wl_input_panel_surface *ip_surface;

	window = gtk_widget_get_window (widget);
	gdk_wayland_window_set_use_custom_surface (window);

	surface = gdk_wayland_window_get_wl_surface (window);
	ip_surface = wl_input_panel_get_input_panel_surface (input_panel, surface);
	wl_input_panel_surface_set_overlay_panel (ip_surface);
}

void setup_input_panel(GtkWidget *widget)
{
	ensure_input_panel ();

	g_signal_connect (widget, "realize",
			  G_CALLBACK (input_panel_realize_cb), NULL);
}
