/* Copyright (C) 2015 Leslie Zhai <xiang.zhai@i-soft.com.cn> */

#include "elsa-popup.h"

static gboolean on_popup_focus_out(GtkWidget *widget,
                                   GdkEventFocus *event,
                                   gpointer user_data)
{
    gtk_widget_hide(widget);
    
    return TRUE;
}

GtkWidget *elsa_popup_new(GtkWidget *child) 
{ 
    GtkWidget *popup = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    gtk_window_set_decorated(GTK_WINDOW(popup), FALSE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(popup), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(popup), TRUE);
    gtk_window_set_position(GTK_WINDOW(popup), GTK_WIN_POS_NONE);
    gtk_widget_set_events(popup, GDK_FOCUS_CHANGE_MASK);
    gtk_container_add(GTK_CONTAINER(popup), child);
    
    g_object_connect(G_OBJECT(popup),
        "signal::focus-out-event", G_CALLBACK(on_popup_focus_out), NULL,
        NULL);

    gtk_widget_grab_focus(popup);

    return popup;
}
