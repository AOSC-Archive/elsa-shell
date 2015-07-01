/* Copyright (C) 2015 AnthonOS Open Source Community */

#include "config.h"
#include "elsa-sound.h"
#include "../elsa-popup.h"

#include <pulse/pulseaudio.h>

#define SINK_DEVICE_INDEX 0
#define VOLUME_MUTE_ICON "audio-volume-muted-symbolic"
#define VOLUME_LOW_ICON "audio-volume-low-symbolic"
#define VOLUME_MEDIUM_ICON "audio-volume-medium-symbolic"
#define VOLUME_HIGH_ICON "audio-volume-high-symbolic"

#define VOLUME_SET_ICON(icon, name) \
    if (icon) { \
        gtk_image_set_from_icon_name(GTK_IMAGE(icon),   \
                                     name,  \
                                     GTK_ICON_SIZE_LARGE_TOOLBAR);  \
        gtk_image_set_pixel_size(GTK_IMAGE(icon), 24);  \
    }

static GtkWidget *icon = NULL;
static GtkWidget *scale = NULL;
static GtkAdjustment *adjust = NULL;

/* 
 * PulseAudio is inherit from Python2.x binding 
 * https://github.com/linuxdeepin/pypulseaudio 
 */
static pa_threaded_mainloop *pa_ml = NULL;
static pa_context *pa_ctx = NULL;
static pa_mainloop_api *pa_mlapi = NULL;
static int sink_channel_num = 0;

static void pa_sink_event_cb(pa_context *c,
                             const pa_sink_info *info,
                             int eol,
                             void *user_data);
static void pa_context_subscribe_cb(pa_context *c,
                                    pa_subscription_event_type_t t,
                                    uint32_t idx,
                                    void *user_data);
static void pa_sink_info_cb(pa_context *c,
                            const pa_sink_info *i,
                            int eol,
                            void *userdata);
static void context_state_cb(pa_context *c, void *user_data);
static gboolean connect_to_pulse(gpointer user_data);
static void set_volume_icon(int value);
static void set_sink_volume(int idx, int volume);
static void adjust_value_changed(GtkAdjustment *adjust, gpointer user_data);

static void pa_sink_event_cb(pa_context *c,
                             const pa_sink_info *info,
                             int eol,
                             void *user_data)
{
    if (!c || !info || eol > 0 || !user_data)
        return;

    pa_sink_info_cb(c, info, eol, NULL);
}

static void pa_context_subscribe_cb(pa_context *c,
                                    pa_subscription_event_type_t t,
                                    uint32_t idx,
                                    void *user_data)
{
    if (!c) {
        g_warning("%s, line %d, invalid arguement\n", __func__, __LINE__);
        return;
    }

    switch (t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) {
    case PA_SUBSCRIPTION_EVENT_SINK:
        if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_NEW) {
            pa_context_get_sink_info_by_index(c, idx, pa_sink_event_cb, "sink_new");
        } else if ((t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_CHANGE) {
            pa_context_get_sink_info_by_index(c, idx, pa_sink_event_cb, "sink_changed");
        }
        break;
    default:
        break;
    }
}

static void pa_sink_info_cb(pa_context *c,
                            const pa_sink_info *i,
                            int eol,
                            void *userdata)
{
    pa_sink_port_info **ports  = NULL;
    pa_sink_port_info *port = NULL;
    pa_sink_port_info *active_port = NULL;
    const char *prop_key = NULL;
    void *prop_state = NULL;
    int j;

    if (!c || !i || eol > 0)
        return;

    while ((prop_key = pa_proplist_iterate(i->proplist, &prop_state))) {
#if DEBUG
        g_message("%s, line %d, %s %s\n",
                  __func__, __LINE__, prop_key,
                  pa_proplist_gets(i->proplist, prop_key));
#endif
    }

    sink_channel_num = i->channel_map.channels;
#if DEBUG
    g_message("%s, line %d, channel map %s balance, channel map count: %d\n",
              __func__, __LINE__,
              pa_channel_map_can_balance(&i->channel_map) ? "CAN" : "CAN'T",
              i->channel_map.channels);
#endif
    for (j = 0; j < i->channel_map.channels; j++) {
#if DEBUG
        g_message("%s, line %d, channel map %d\n", 
                  __func__, __LINE__, i->channel_map.map[j]);
#endif
    }

    ports = i->ports;
    for (j = 0; j < i->n_ports; j++) {
        port = ports[j];
#if DEBUG
        g_message("%s, line %d, port %s %s %s\n", __func__, __LINE__,
                  port->name, port->description, 
                  port->available ? "AVAILABLE" : "UNAVAILABLE");
#endif
    }

    active_port = i->active_port;
    if (active_port) {
#if DEBUG
        g_message("%s, line %d active port %s %s %s\n", __func__, __LINE__,
                  active_port->name, active_port->description, 
                  active_port->available ? "AVAILABLE" : "UNAVAILABLE");
#endif
    }

    for (j = 0; j < i->volume.channels; j++) {
#if DEBUG
        g_message("%s, line %d, volume channel %d\n", 
                  __func__, __LINE__, i->volume.values[j]);
#endif
    }

    g_signal_handlers_block_by_func(adjust, adjust_value_changed, NULL);
    set_volume_icon(i->volume.values[0]);
    gtk_adjustment_set_value(GTK_ADJUSTMENT(adjust), i->volume.values[0]);
    g_signal_handlers_unblock_by_func(adjust, adjust_value_changed, NULL);

#if DEBUG
    g_message("%s, line %d, sink %s %s base volume %d %s\n", 
              __func__, __LINE__, i->name, i->description, i->base_volume, 
              i->mute ? "MUTED" : "UNMUTED");
#endif
}

static void context_state_cb(pa_context *c, void *user_data)
{
    pa_operation *pa_op = NULL;

    if (!c) {
        g_warning("%s, line %d, invalid arguement\n", __func__, __LINE__);
        return;
    }

    switch (pa_context_get_state(c)) {
    case PA_CONTEXT_UNCONNECTED:
    case PA_CONTEXT_CONNECTING:
    case PA_CONTEXT_AUTHORIZING:
    case PA_CONTEXT_SETTING_NAME:
        break;

    case PA_CONTEXT_READY:
        pa_context_set_subscribe_callback(c, pa_context_subscribe_cb, NULL);

        pa_op = pa_context_subscribe(c, (pa_subscription_mask_t)
                                     (PA_SUBSCRIPTION_MASK_SINK |
                                      PA_SUBSCRIPTION_MASK_SOURCE |
                                      PA_SUBSCRIPTION_MASK_SINK_INPUT |
                                      PA_SUBSCRIPTION_MASK_SOURCE_OUTPUT |
                                      PA_SUBSCRIPTION_MASK_CLIENT |
                                      PA_SUBSCRIPTION_MASK_SERVER |
                                      PA_SUBSCRIPTION_MASK_CARD),
                                     NULL, NULL);
        if (!pa_op) {
            g_warning("%s, line %d, fail to pa_context_subscribe\n", 
                      __func__, __LINE__);
            return;
        }
        pa_operation_unref(pa_op);

        pa_op = pa_context_get_sink_info_list(c, pa_sink_info_cb, NULL);
        if (!pa_op) {
            g_warning("%s, line %d, fail to pa_context_get_sink_info_list\n", 
                      __func__, __LINE__);
            return;
        }
        pa_operation_unref(pa_op);
        pa_op = NULL;
        break;

    case PA_CONTEXT_FAILED:
        if (pa_ctx) {
            pa_context_unref(pa_ctx);
            pa_ctx = NULL;
        }

        g_warning("%s, line %d Connection failed, attempting reconnect\n", 
                  __func__, __LINE__);
        g_timeout_add_seconds(13, connect_to_pulse, NULL);
        return;

    case PA_CONTEXT_TERMINATED:
    default:
        g_message("%s, line %d, pa_context terminated\n", __func__, __LINE__);
        return;
    }
}

static gboolean connect_to_pulse(gpointer user_data)
{
    pa_ctx = pa_context_new(pa_mlapi, "Elsa Sound");
    if (!pa_ctx) {
        g_warning("%s, line %d, fail to pa_context_new\n", __func__, __LINE__);
        return G_SOURCE_CONTINUE;
    }

    pa_context_set_state_callback(pa_ctx, context_state_cb, NULL);

    if (pa_context_connect(pa_ctx, NULL, PA_CONTEXT_NOFAIL, NULL) < 0) {
        if (pa_context_errno(pa_ctx) == PA_ERR_INVALID) {
            g_warning("%s, line %d Connection to PulseAudio failed. "
                      "Automatic retry in 13s\n", __func__, __LINE__);
            return G_SOURCE_CONTINUE;
        }
    }

    return G_SOURCE_CONTINUE;
}

static void set_sink_volume(int idx, int volume) 
{
    pa_operation *pa_op = NULL;
    pa_cvolume sink_volume;
    int i;

#if DEBUG
    g_message("%s, line %d, channel num %d\n", __func__, __LINE__, sink_channel_num);
#endif
    sink_volume.channels = sink_channel_num;
    for (i = 0; i < sink_channel_num; i++)
        sink_volume.values[i] = volume;

    pa_op = pa_context_set_sink_volume_by_index(pa_ctx, 
                                                idx, 
                                                &sink_volume, 
                                                NULL, NULL);
    if (!pa_op) {
        g_warning("%s, line %d, fail to pa_context_set_sink_volume_by_index\n", 
                  __func__, __LINE__);
        return;
    }
    pa_operation_unref(pa_op);
    pa_op = NULL;
}

static void set_volume_icon(int value) 
{
    if (value == 0) {
        VOLUME_SET_ICON(icon, VOLUME_MUTE_ICON);
        return;
    }
    
    if (value < 30000) {
        VOLUME_SET_ICON(icon, VOLUME_LOW_ICON);
        return;
    }
    
    if (value < 70000) {
        VOLUME_SET_ICON(icon, VOLUME_MEDIUM_ICON);
        return;
    }

    VOLUME_SET_ICON(icon, VOLUME_HIGH_ICON);
}

static void adjust_value_changed(GtkAdjustment *_adjust, gpointer user_data) 
{
    int value = (int) gtk_adjustment_get_value(_adjust);

    set_sink_volume(SINK_DEVICE_INDEX, value);

    set_volume_icon(value);
}

void elsa_sound_cleanup()
{
    if (pa_ctx) {
        pa_context_unref(pa_ctx);
        pa_ctx = NULL;
    }

    if (pa_ml) {
        pa_threaded_mainloop_stop(pa_ml);
        pa_threaded_mainloop_free(pa_ml);
        pa_ml = NULL;
    }
}

static gboolean elsa_sound_button_press(GtkWidget *eventbox,
                                        GdkEventButton *event,
                                        gpointer user_data) 
{
    GtkWidget *popup = (GtkWidget *)user_data;
    GtkAllocation alloc;

    gtk_widget_get_allocation(eventbox, &alloc);
    gtk_window_move(GTK_WINDOW(popup), alloc.x, event->y_root);
    gtk_widget_show_all(popup);

    return FALSE;
}

GtkWidget *elsa_sound_new() 
{
    GtkWidget *eventbox = gtk_event_box_new();
    GtkWidget *popup = NULL;

    pa_ml = pa_threaded_mainloop_new();
    if (!pa_ml) {
        g_warning("%s, line %d, fail to pa_threaded_mainloop_new\n", 
                  __func__, __LINE__);
        return eventbox;
    }

    pa_mlapi = pa_threaded_mainloop_get_api(pa_ml);
    if (!pa_mlapi) {
        g_warning("%s, line %d, fail to pa_threaded_mainloop_get_api\n", 
                  __func__, __LINE__);
        return eventbox;
    }

    icon = gtk_image_new_from_icon_name(VOLUME_MUTE_ICON, 
                                        GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 24);
    gtk_container_add(GTK_CONTAINER(eventbox), icon);

    adjust = gtk_adjustment_new(60000, 0, 100000, 1, 1, 0);
    g_object_connect(G_OBJECT(adjust), 
        "signal::value-changed", G_CALLBACK(adjust_value_changed), NULL, 
        NULL);

    scale = gtk_scale_new(GTK_ORIENTATION_VERTICAL, adjust);
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

    pa_threaded_mainloop_start(pa_ml);
    connect_to_pulse(NULL);

    return eventbox;
}
