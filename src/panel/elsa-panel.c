/* 
 * Copyright (C) 2015 AnthonOS Open Source Community
 * Copyright (C) 2015 Leslie Zhai <xiang.zhai@i-soft.com.cn> 
 *
 */

#include "elsa-panel.h"

#include <string.h>
#include <X11/Xatom.h>
#include <gdk/gdkx.h>

#include "elsa-launcher.h"
#include "elsa-showdesktop.h"
#include "elsa-taskbar.h"
#include "elsa-systray.h"
#include "elsa-sound.h"
#include "elsa-datetime.h"

struct _ElsaPanelPrivate {
    GtkWidget *window;

    gint x;
    gint y;
    gint width;
    gint height;

    GdkScreen *screen;

    GtkWidget *box;
    GtkWidget *left_box;
    GtkWidget *center_box;
    GtkWidget *right_box;

    GtkWidget *launcher;
    GtkWidget *showdesktop;
    GtkWidget *taskbar;
    GtkWidget *systray;
    GtkWidget *sound;
    GtkWidget *datetime;
};

/* GObject vfunc */
static void elsa_panel_dispose(GObject *object);
static void elsa_panel_finalize(GObject *object);

/* Internal vfunc */
static void elsa_panel_resize(ElsaPanel *self);
static void elsa_panel_update_strut(ElsaPanel *self);

G_DEFINE_TYPE_WITH_PRIVATE(ElsaPanel, elsa_panel, G_TYPE_OBJECT)

static gboolean elsa_panel_configure_cb(GtkWidget *widget, 
                                        GdkEvent *ev, 
                                        ElsaPanel *self) 
{
    elsa_panel_update_strut(self);

    return FALSE;
}

/* 
 * Extended Window Manager Hints 
 * http://standards.freedesktop.org/wm-spec/wm-spec-1.3.html
 *
 * Inherit from xtk/xtk/x11/xtkwidget-x11.cpp
 *
 */
static void elsa_panel_update_strut(ElsaPanel *self) 
{
    ElsaPanelPrivate *priv = self->priv;
    GdkDisplay *display = gdk_screen_get_display(priv->screen);
    gint w, h;
    GdkAtom atom_wm_strut_partial, atom_wm_strut, atom_type;

    gtk_window_get_size(GTK_WINDOW(priv->window), &w, &h);

    elsa_panel_resize(self);

    atom_wm_strut_partial = gdk_atom_intern("_NET_WM_STRUT_PARTIAL", FALSE);
    atom_wm_strut = gdk_atom_intern("_NET_WM_STRUT", FALSE); 
    atom_type = gdk_x11_xatom_to_atom_for_display(display, XA_CARDINAL);
    if (gdk_x11_screen_supports_net_wm_hint(priv->screen, atom_wm_strut_partial)) {
        long struts[12];

        memset(struts, 0, sizeof(struts));
        struts[3] = h;
        struts[10] = 0;
        struts[11] = w;

        GdkWindow *gdkwin = gtk_widget_get_window(priv->window);
        gdk_property_change(gdkwin, atom_wm_strut_partial, atom_type, 32,
                            GDK_PROP_MODE_REPLACE, (const guchar*)&struts, 12);
        gdk_property_change(gdkwin, atom_wm_strut, atom_type, 32,
                            GDK_PROP_MODE_REPLACE, (const guchar*)&struts, 4);
    }
}

static void elsa_panel_resize(ElsaPanel *self) 
{
    ElsaPanelPrivate* priv = self->priv;
    GtkRequisition req;
    gint lw = -1, rw = -1, cw = -1;

    gtk_widget_get_preferred_size(priv->left_box, NULL, &req);
    lw = req.width;

    gtk_widget_get_preferred_size(priv->right_box, NULL, &req);
    rw = req.width;

    cw = priv->width - lw - rw;
    elsa_taskbar_set_width(cw);
#if DEBUG
    g_message("%s, line %d, lw: %d, cw: %d, rw: %d, w: %d\n", 
        __func__, __LINE__, lw, cw, rw, priv->width);
#endif
    gtk_window_resize(GTK_WINDOW(priv->window), priv->width, priv->height);
}

static void elsa_panel_screen_size_changed_cb(GdkScreen *screen, ElsaPanel *self) 
{
    ElsaPanelPrivate *priv = self->priv;

    if (gdk_screen_get_n_monitors(priv->screen) <= 0) {
        gtk_widget_hide(GTK_WIDGET(priv->window));
        return;
    }

    int mon_id = gdk_screen_get_primary_monitor(priv->screen);
    GdkRectangle r;

    gdk_screen_get_monitor_geometry(priv->screen, mon_id, &r);
    priv->x = r.x;
    priv->width = r.width;
    priv->y = gdk_screen_height() - priv->height;
#if DEBUG
    g_message("%s, line %d, %dx%d (%d, %d)\n", 
        __func__, __LINE__, priv->width, priv->height, priv->x, priv->y);
#endif
    elsa_panel_resize(self);
    gtk_window_move(GTK_WINDOW(priv->window), priv->x, priv->y);
}

static void elsa_panel_init(ElsaPanel *self) 
{
    ElsaPanelPrivate *priv = self->priv = elsa_panel_get_instance_private(self);

    priv->height = 30;

    /* window */
    priv->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_name(GTK_WIDGET(priv->window), "elsa-panel");
    gtk_window_set_decorated(GTK_WINDOW(priv->window), FALSE);
    gtk_window_set_type_hint(GTK_WINDOW(priv->window), GDK_WINDOW_TYPE_HINT_DOCK);
    gtk_window_set_position(GTK_WINDOW(priv->window), GTK_WIN_POS_NONE);
    gtk_window_set_keep_above(GTK_WINDOW(priv->window), TRUE);
    gtk_window_set_accept_focus(GTK_WINDOW(priv->window), FALSE);
    gtk_window_stick(GTK_WINDOW(priv->window));

    /* screen */
    priv->screen = gtk_window_get_screen(GTK_WINDOW(priv->window));
    gtk_window_set_screen(GTK_WINDOW(priv->window), priv->screen);
    gtk_widget_set_size_request(priv->window, 1, 1);

    /* box */
    priv->box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    g_object_set(G_OBJECT(priv->box), "border-width", 0, NULL);
    gtk_container_add(GTK_CONTAINER(priv->window), priv->box);

    /* left box */
    priv->left_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    g_object_set(G_OBJECT(priv->left_box), "border-width", 0, NULL);
    gtk_box_pack_start(GTK_BOX(priv->box), priv->left_box, FALSE, FALSE, 0);

    /* launcher */
    priv->launcher = elsa_launcher_new();
    gtk_box_pack_start(GTK_BOX(priv->left_box), priv->launcher, FALSE, FALSE, 0);

    /* showdesktop */
    priv->showdesktop = elsa_showdesktop_new(priv->screen);
    gtk_box_pack_start(GTK_BOX(priv->left_box), priv->showdesktop, FALSE, FALSE, 0);

    /* center box */
    priv->center_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    g_object_set(G_OBJECT(priv->center_box), "border-width", 0, NULL);
    gtk_box_pack_start(GTK_BOX(priv->box), priv->center_box, FALSE, FALSE, 0);

    /* taskbar */
    priv->taskbar = elsa_taskbar_new();
    gtk_box_pack_start(GTK_BOX(priv->center_box), priv->taskbar, FALSE, FALSE, 0);

    /* right box */
    priv->right_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    g_object_set(G_OBJECT(priv->right_box), "border-width", 0, NULL);
    gtk_box_pack_end(GTK_BOX(priv->box), priv->right_box, FALSE, FALSE, 0);

    /* systray */
    priv->systray = elsa_systray_new(self);
    gtk_box_pack_start(GTK_BOX(priv->right_box), priv->systray, FALSE, FALSE, 0);

    /* sound */
    priv->sound = elsa_sound_new();
    gtk_box_pack_start(GTK_BOX(priv->right_box), priv->sound, FALSE, FALSE, 0);

    /* datetime */
    priv->datetime = elsa_datetime_new();
    gtk_box_pack_start(GTK_BOX(priv->right_box), priv->datetime, FALSE, FALSE, 0);

    g_object_connect(G_OBJECT(priv->screen),
        "signal::size-changed", G_CALLBACK(elsa_panel_screen_size_changed_cb), self,
        "signal::monitors-changed", G_CALLBACK(elsa_panel_screen_size_changed_cb), self,
        NULL);

    g_object_connect(G_OBJECT(priv->window),
        "signal::configure-event", G_CALLBACK(elsa_panel_configure_cb), self,
        NULL);

    elsa_panel_screen_size_changed_cb(priv->screen, self);
}

static void elsa_panel_dispose(GObject *object) 
{
    ElsaPanel *self = ELSA_PANEL(object);

    if (self->priv->window) {
        g_object_unref(self->priv->window);
        self->priv->window = NULL;
    }

    G_OBJECT_CLASS(elsa_panel_parent_class)->dispose(object);
}

static void elsa_panel_finalize(GObject *object) 
{
    G_OBJECT_CLASS(elsa_panel_parent_class)->finalize(object);
}

static void elsa_panel_class_init(ElsaPanelClass *kclass) 
{
    GObjectClass* object_class = G_OBJECT_CLASS(kclass);

    object_class->dispose = elsa_panel_dispose;
    object_class->finalize = elsa_panel_finalize;
}

ElsaPanel *elsa_panel_new() 
{
    return g_object_new(ELSA_TYPE_PANEL, NULL);
}

GdkScreen *elsa_panel_get_screen(ElsaPanel *self) 
{
    return self->priv->screen;
}

gint elsa_panel_get_height(ElsaPanel *self) 
{
    return self->priv->height;
}

void elsa_panel_show(ElsaPanel *self) 
{
    gtk_widget_show_all(self->priv->window);
}
