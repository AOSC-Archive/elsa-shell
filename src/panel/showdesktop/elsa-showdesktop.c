/* Copyright (C) 2015 AnthonOS Open Source Community */

#include "config.h"
#include "elsa-showdesktop.h"

#include <gdk/gdkx.h>
#include <X11/Xlib.h>

static Display *s_XDisplay = NULL;

static gboolean elsa_showdesktop_button_press(GtkWidget *eventbox, 
                                              GdkEventButton *event, 
                                              gpointer user_data) 
{
    /*
     * This inherit from cairo-dock-core/src/implementations/cairo-dock-X-utilities.c
     */
    XEvent xClientMessage;
    Window root = DefaultRootWindow(s_XDisplay);

    xClientMessage.xclient.type = ClientMessage;
    xClientMessage.xclient.serial = 0;
    xClientMessage.xclient.send_event = True;
    xClientMessage.xclient.display = s_XDisplay;
    xClientMessage.xclient.window = root;
    xClientMessage.xclient.message_type = XInternAtom(s_XDisplay, "_NET_SHOWING_DESKTOP", False);
    xClientMessage.xclient.format = 32;
    xClientMessage.xclient.data.l[0] = 1;
    xClientMessage.xclient.data.l[1] = 0;
    xClientMessage.xclient.data.l[2] = 0;
    xClientMessage.xclient.data.l[3] = 2;
    xClientMessage.xclient.data.l[4] = 0;
    
    XSendEvent(s_XDisplay,
        root,
        False,
        SubstructureRedirectMask | SubstructureNotifyMask,
        &xClientMessage);
    XFlush(s_XDisplay);

    return FALSE;
}

GtkWidget *elsa_showdesktop_new(GdkScreen *screen) 
{
    GtkWidget *eventbox = gtk_event_box_new();
    GtkWidget *icon = gtk_image_new_from_icon_name("desktop", GTK_ICON_SIZE_LARGE_TOOLBAR);

    s_XDisplay = GDK_SCREEN_XDISPLAY(screen);

    gtk_container_add(GTK_CONTAINER(eventbox), icon);

    g_object_connect(G_OBJECT(eventbox), 
        "signal::button-press-event", G_CALLBACK(elsa_showdesktop_button_press), NULL,
        NULL);

    return eventbox;
}
