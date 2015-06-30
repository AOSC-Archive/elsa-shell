/* Copyright (C) 2015 Leslie Zhai <xiang.zhai@i-soft.com.cn> */

#include "config.h"
#include "elsa-datetime.h"

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

static gboolean elsa_datetime_button_press_cb(GtkWidget *event_box, 
                                              GdkEvent *event, 
                                              gpointer user_data) 
{
#if DEBUG
    g_message("%s, line %d, %x\n", __func__, __LINE__, event);
#endif

    return FALSE;
}

GtkWidget *elsa_datetime_new() 
{
    GtkWidget *event_box = gtk_event_box_new();
    GtkWidget *time_label = gtk_label_new("");

    gtk_container_add(GTK_CONTAINER(event_box), time_label);

    update_time(time_label);
    g_timeout_add_seconds(1, update_time, time_label);

    g_object_connect(G_OBJECT(event_box), 
        "signal::button-press-event", G_CALLBACK(elsa_datetime_button_press_cb), NULL,
        NULL);

    return event_box;
}
