/* Copyright (C) 2015 Leslie Zhai <xiang.zhai@i-soft.com.cn> */

#include "config.h"
#include "elsa-systray.h"

#include <gtk/gtk.h>

#include "na-tray-manager.h"

#define TRAY_ICON_SIZE 30

static NaTrayManager *tray_man = NULL;
static GtkWidget *flowbox = NULL;

static void na_tray_icon_added(NaTrayManager *na_manager, 
                               GtkWidget *child, 
                               gpointer data)
{
    gtk_box_pack_start(GTK_BOX(flowbox), child, FALSE, FALSE, 0);

    gtk_widget_show(child);
}

static void na_tray_icon_removed(NaTrayManager *na_manager, 
                                 GtkWidget *child, 
                                 gpointer data)
{
    gtk_container_remove(GTK_CONTAINER(flowbox), child);
}

/* 
 * My sincere thanks goes to Florian Müllner 
 * https://bugzilla.gnome.org/show_bug.cgi?id=751485 
 *
 * FIXME: bigger than 30px systray, such as pidgin, will expand the panel
 */
static gboolean na_tray_draw_icon (GtkWidget *socket,
                                   gpointer   data)
{
    cairo_t *cr = data;
    GdkWindow *window = NULL;
    GtkAllocation socket_alloc, parent_alloc;

    if (!NA_IS_TRAY_CHILD(socket))
        return FALSE;

    if (!na_tray_child_has_alpha(NA_TRAY_CHILD(socket)))
        return FALSE;

    window = gtk_widget_get_window(socket);

    gtk_widget_get_allocation(socket, &socket_alloc);
    gtk_widget_get_allocation(flowbox, &parent_alloc);

    cairo_save(cr);
#if DEBUG
    g_message("%s, line %d: (%d, %d) (%d, %d) %dx%d\n", 
              __func__, __LINE__, parent_alloc.x, parent_alloc.y, 
              socket_alloc.x, socket_alloc.y, 
              socket_alloc.width, socket_alloc.height);
#endif
    gdk_window_resize(window, TRAY_ICON_SIZE, TRAY_ICON_SIZE);
    gdk_cairo_set_source_window(cr,
                                gtk_widget_get_window (socket),
                                socket_alloc.x - parent_alloc.x,
                                socket_alloc.y - parent_alloc.y);
    cairo_rectangle(cr,
                    socket_alloc.x - parent_alloc.x, 
                    socket_alloc.y - parent_alloc.y,
                    TRAY_ICON_SIZE, TRAY_ICON_SIZE);
    cairo_clip(cr);
    cairo_paint(cr);
    cairo_restore(cr);

    return FALSE;
}

static gboolean flowbox_draw(GtkWidget *flowbox, cairo_t *cr) 
{
    gtk_container_foreach(GTK_CONTAINER(flowbox), (GtkCallback)na_tray_draw_icon, cr);
}

GtkWidget* elsa_systray_new(ElsaPanel *elsa_panel)
{
    GdkScreen *screen = elsa_panel_get_screen(elsa_panel);
    
    flowbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    g_object_connect(G_OBJECT(flowbox), 
        "signal::draw", G_CALLBACK(flowbox_draw), NULL, 
        NULL);

    tray_man = na_tray_manager_new();
    na_tray_manager_manage_screen(tray_man, screen);

    if (!na_tray_manager_check_running(screen)) {
        g_warning("tray manager does not run on screen");
    }

    g_object_connect(G_OBJECT(tray_man), 
        "signal::tray_icon_added", G_CALLBACK(na_tray_icon_added), NULL, 
        "signal::tray_icon_removed", G_CALLBACK(na_tray_icon_removed), NULL, 
        NULL);
    
    return flowbox;
}
