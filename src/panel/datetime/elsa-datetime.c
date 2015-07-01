/* Copyright (C) 2015 Leslie Zhai <xiang.zhai@i-soft.com.cn> */

#include "config.h"
#include "elsa-datetime.h"
#include "../elsa-popup.h"

static gboolean update_time(gpointer user_data)
{
    GtkWidget *label = (GtkWidget*) user_data;

    GDateTime *dt = g_date_time_new_now_local();
    gchar *buf = g_date_time_format(dt, "%T");
    gtk_label_set_text(GTK_LABEL(label), buf);
    g_free(buf);
    buf = NULL;
    g_date_time_unref(dt);
    dt = NULL;

    return G_SOURCE_CONTINUE;
}

static gboolean elsa_datetime_button_press(GtkWidget *eventbox, 
                                           GdkEventButton *event, 
                                           gpointer user_data) 
{
    GtkWidget *popup = (GtkWidget *)user_data;

    gtk_window_move(GTK_WINDOW(popup), event->x_root, event->y_root);
    gtk_widget_show_all(popup);

    return FALSE;
}

GtkWidget *elsa_datetime_new() 
{
    GtkWidget *eventbox = gtk_event_box_new();
    GtkWidget *time_label = gtk_label_new("");
    GtkWidget *popup = elsa_popup_new(gtk_calendar_new());

    gtk_container_add(GTK_CONTAINER(eventbox), time_label);

    update_time(time_label);
    g_timeout_add_seconds(1, update_time, time_label);

    g_object_connect(G_OBJECT(eventbox), 
        "signal::button-press-event", G_CALLBACK(elsa_datetime_button_press), popup,
        NULL);

    return eventbox;
}
