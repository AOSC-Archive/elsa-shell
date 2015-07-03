/* Copyright (C) 2015 AnthonOS Open Source Community */

#include "config.h"
#include "elsa-systray.h"
#include "na-tray.h"

struct _ElsaSystrayPrivate
{
    GtkWidget *box;

    NaTray *na_tray;
};

G_DEFINE_TYPE_WITH_PRIVATE(ElsaSystray, elsa_systray, GTK_TYPE_BIN)

static void elsa_systray_finalize(GObject *object) 
{
    ElsaSystray *self = ELSA_SYSTRAY(object);
    ElsaSystrayPrivate *priv = elsa_systray_get_instance_private(self);

    if (priv->box) {
        g_object_unref(priv->box);
        priv->box = NULL;
    }

    if (priv->na_tray) {
        g_object_unref(priv->na_tray);
        priv->na_tray = NULL;
    }

    G_OBJECT_CLASS(elsa_systray_parent_class)->finalize(object);
}

static void elsa_systray_get_preferred_height(GtkWidget *widget, 
                                              gint *minimum,
                                              gint *natural) 
{
    *minimum = *natural = TRAY_ICON_SIZE;
}

static void elsa_systray_class_init(ElsaSystrayClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    object_class->finalize = elsa_systray_finalize;

    widget_class->get_preferred_height = elsa_systray_get_preferred_height;
}

static void elsa_systray_init(ElsaSystray *self)
{
    ElsaSystrayPrivate *priv = elsa_systray_get_instance_private(self);

    priv->box = gtk_event_box_new();

    gtk_container_add(GTK_CONTAINER(self), priv->box);
}

GtkWidget *elsa_systray_new(GdkScreen *screen)
{
    ElsaSystray *self = NULL;

    self = g_object_new(ELSA_TYPE_SYSTRAY, NULL);
    self->priv = elsa_systray_get_instance_private(self);
    self->priv->na_tray = na_tray_new_for_screen(screen, GTK_ORIENTATION_HORIZONTAL);
    
    na_tray_set_icon_size(self->priv->na_tray, TRAY_ICON_SIZE);

    gtk_container_add(GTK_CONTAINER(self->priv->box), GTK_WIDGET(self->priv->na_tray));

    return (GtkWidget *)self;
}
