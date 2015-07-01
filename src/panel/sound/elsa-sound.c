/* Copyright (C) 2015 AnthonOS Open Source Community */

#include "elsa-sound.h"
#include "../elsa-popup.h"

static GtkWidget *scale = NULL;
static gboolean is_shown = FALSE;

static gboolean elsa_sound_button_press(GtkWidget *eventbox,
                                        GdkEventButton *event,
                                        gpointer user_data) 
{
    GtkWidget *popup = (GtkWidget *)user_data;

    is_shown = !is_shown;

    if (is_shown) {
        gtk_window_move(GTK_WINDOW(popup), event->x_root, event->y_root);
        gtk_widget_show_all(popup);
    } else {
        gtk_widget_hide(popup);
    }

    return FALSE;
}

GtkWidget *elsa_sound_new() 
{
    GtkWidget *eventbox = gtk_event_box_new();
    GtkWidget *icon = NULL;
    GtkWidget *popup = NULL;

    icon = gtk_image_new_from_icon_name("audio-volume-muted-symbolic", 
                                        GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 24);
    gtk_container_add(GTK_CONTAINER(eventbox), icon);

    scale = gtk_scale_new_with_range(GTK_ORIENTATION_VERTICAL, 0.0, 1.0, 0.1);
    g_object_set(G_OBJECT(scale), 
        "inverted", TRUE,
        "draw_value", FALSE,
        NULL);
    gtk_scale_set_value_pos(GTK_SCALE(scale), GTK_POS_BOTTOM);
    gtk_widget_set_size_request(scale, 20, 120);
    popup = elsa_popup_new(scale);

    g_object_connect(G_OBJECT(eventbox),
        "signal::button-press-event", G_CALLBACK(elsa_sound_button_press), popup,
        NULL);

    return eventbox;
}
