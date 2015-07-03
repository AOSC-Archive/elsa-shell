/* Copyright (C) 2014 - 2015 Sian Cao <siyuan.cao@i-soft.com.cn> */

#include <math.h>

#define COGL_ENABLE_EXPERIMENTAL_API
#include <cogl/cogl.h>

#include <meta/workspace.h>
#include <meta/compositor.h>
#include <meta/compositor-mutter.h>
#include <meta/window.h>
#include <meta/keybindings.h>
#include <meta/meta-plugin.h>
#include <meta/main.h>
#include <meta/display.h>
#include <meta/meta-background.h>


#include <cairo/cairo.h>

#include "switcher.h"
#include "shell-app.h"
#include "shell-window-tracker.h"

static void meta_switcher_present_list(MetaSwitcher* self);
static gboolean on_button_press(ClutterActor *actor, ClutterEvent *event, MetaSwitcher* self);
static gboolean on_captured_event(ClutterActor *actor, ClutterEvent *event, MetaSwitcher* self);
static gboolean on_key_release(ClutterActor *actor, ClutterEvent *event, MetaSwitcher* self);
static gboolean on_key_press(ClutterActor *actor, ClutterEvent *event, MetaSwitcher* self);

static GQuark window_private_quark = 0;

// store data for MetaWindow on App Icon
typedef struct _WindowPrivate {
    guint highlight;
    MetaWindow* window;
} WindowPrivate;

static void free_window_private(WindowPrivate* priv)
{
    g_slice_free(WindowPrivate, priv);
}

static WindowPrivate * get_window_private(ClutterActor* actor)
{
    WindowPrivate *priv = g_object_get_qdata (G_OBJECT(actor), window_private_quark);

    if (G_UNLIKELY (window_private_quark == 0))
        window_private_quark = g_quark_from_static_string ("window-private-data");

    if (G_UNLIKELY (!priv)) {
        priv = g_slice_new0 (WindowPrivate);

        g_object_set_qdata_full (G_OBJECT(actor), window_private_quark, priv,
                (GDestroyNotify)free_window_private);
    }

    return priv;
}

static const int APP_ICON_SIZE = 96;
static const int APP_ACTOR_WIDTH = 96+4;
static const int APP_ACTOR_HEIGHT = 96+24;

static const int AUTOCLOSE_TIMEOUT = 1000;

struct _MetaSwitcherPrivate
{
    ClutterActor* top;
    MetaWorkspace* workspace;
    MetaPlugin* plugin;
    ClutterActor* previous_focused;
    guint autoclose_id;
    GPtrArray* apps; // cached apps
    GHashTable* icons;
    cairo_surface_t* snapshot;
    gfloat snapshot_offset;

    gint selected_id; // selected window id

    guint disposed: 1;
    guint backwards: 1;
    guint modaled: 1;
};

// {{{ GObject property and signal enums

enum MetaSwitcherProp
{
    PROP_0,
    PROP_WORKSPACE,
    PROP_PLUGIN,
    N_PROPERTIES
};

static GParamSpec* property_specs[N_PROPERTIES] = {NULL, };

enum MetaSwitcherSignal
{
    DESTROY,
    N_SIGNALS
};

static guint signals[N_SIGNALS];

G_DEFINE_TYPE_WITH_PRIVATE(MetaSwitcher, meta_switcher, G_TYPE_OBJECT);

// }}}

static void _set_highlight(MetaSwitcher* self, int id, gboolean highlight)
{
    if (id < 0) return;
    MetaSwitcherPrivate* priv = self->priv;

    MetaWindow* window = g_ptr_array_index(priv->apps, id);
    ClutterActor* win_actor = g_hash_table_lookup(priv->icons, window);
    WindowPrivate* win_priv = get_window_private(CLUTTER_ACTOR(win_actor));
    win_priv->highlight = highlight;

    ClutterActor* bg = clutter_container_find_child_by_name(CLUTTER_CONTAINER(win_actor), "bg");
    clutter_content_invalidate(clutter_actor_get_content(bg));
}

static void reposition_switcher(MetaSwitcher* self)
{
    MetaSwitcherPrivate* priv = self->priv;
    MetaScreen* screen = meta_plugin_get_screen(priv->plugin);

    gint screen_width = 0, screen_height = 0;
    meta_screen_get_size(screen, &screen_width, &screen_height);

    clutter_actor_save_easing_state(priv->top);
    clutter_actor_set_easing_duration(priv->top, 400);
    clutter_actor_set_easing_mode(priv->top, CLUTTER_LINEAR);

    gfloat w = clutter_actor_get_width(priv->top), h = clutter_actor_get_height(priv->top),
            tx = (screen_width - w)/2, ty = (screen_height - h)/2;
    clutter_actor_set_position(priv->top, tx, ty);

    clutter_actor_restore_easing_state(priv->top);
}

static void on_window_added(MetaWorkspace *ws, MetaWindow *win, MetaSwitcher* self)
{
    g_debug("%s", __func__);
}

static void on_window_removed(MetaWorkspace *ws, MetaWindow *win, MetaSwitcher* self)
{
    g_debug("%s", __func__);
    MetaSwitcherPrivate* priv = self->priv;

    ClutterActor* win_actor = NULL;
    gboolean need_reselect = FALSE;
    int id = 0;
    for (id = 0; id < priv->apps->len; id++) {
        if (g_ptr_array_index(priv->apps, id) == win) {
            win_actor = g_hash_table_lookup(priv->icons, win);
            need_reselect = (id == priv->selected_id);
        }
    }

    if (win_actor) {
        if (need_reselect)
            _set_highlight(self, priv->selected_id, FALSE);

        g_hash_table_remove(priv->icons, win);
        g_ptr_array_remove(priv->apps, win);

        if (need_reselect) {
            if (id == 0) {
                clutter_actor_set_margin_left(
                        g_hash_table_lookup(priv->icons, g_ptr_array_index(priv->apps, 0)), 10);
            } else if (id >= priv->apps->len) {
                clutter_actor_set_margin_right(
                        g_hash_table_lookup(priv->icons, g_ptr_array_index(priv->apps, priv->apps->len-1)), 10);
            }
            priv->selected_id = (priv->selected_id + 1) % priv->apps->len;
            _set_highlight(self, priv->selected_id, TRUE);
        }

        clutter_actor_save_easing_state(priv->top);
        clutter_actor_set_easing_duration(priv->top, 200);
        clutter_actor_set_easing_mode(priv->top, CLUTTER_LINEAR);

        clutter_actor_remove_child(priv->top, win_actor);

        clutter_actor_restore_easing_state(priv->top);

        reposition_switcher(self);
    }
}

static void unhook_ws_event(MetaWorkspace* ws, MetaSwitcher* self)
{
    g_object_disconnect(ws, "any_signal::window-added", on_window_added, self,
            "any_signal::window-removed", on_window_removed, self, NULL);
}

static void hook_ws_event(MetaWorkspace* ws, MetaSwitcher* self)
{
    g_object_connect(ws, "signal::window-added", on_window_added, self,
            "signal::window-removed", on_window_removed, self, NULL);
}

/**
 * meta_switcher_new:
 *
 * Create new #MetaSwitcher object.
 *
 * Returns: #MetaSwitcher object.
 */
MetaSwitcher* meta_switcher_new(MetaPlugin* plugin)
{
    MetaSwitcher *switcher = g_object_new(META_TYPE_SWITCHER, "plugin", plugin, NULL);

    return switcher;
}

// {{{ GObject type setup

static void meta_switcher_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    MetaSwitcher* switcher = META_SWITCHER(object);
    MetaSwitcherPrivate* priv = switcher->priv;

    switch (property_id) {
        case PROP_WORKSPACE:
            priv->workspace = g_value_get_pointer(value);
            break;

        case PROP_PLUGIN:
            priv->plugin = g_value_get_pointer(value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void meta_switcher_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    MetaSwitcher* switcher = META_SWITCHER(object);
    MetaSwitcherPrivate* priv = switcher->priv;

    switch (property_id) {
        case PROP_WORKSPACE:
            g_value_set_pointer(value, priv->workspace);
            break;

        case PROP_PLUGIN:
            g_value_set_pointer(value, priv->plugin);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void _emit_destroy(ClutterActor* actor, MetaSwitcher* self)
{
    g_debug("%s", __func__);
    g_signal_emit(self, signals[DESTROY], 0, NULL);
}

static void meta_switcher_init(MetaSwitcher *self)
{
    MetaSwitcherPrivate* priv = self->priv = meta_switcher_get_instance_private(self);
    priv->selected_id = -1;

    priv->top = clutter_actor_new();
    ClutterLayoutManager* box = clutter_box_layout_new();
    clutter_box_layout_set_use_animations(CLUTTER_BOX_LAYOUT(box), TRUE);
    clutter_box_layout_set_easing_duration(CLUTTER_BOX_LAYOUT(box), 200);
    clutter_box_layout_set_easing_mode(CLUTTER_BOX_LAYOUT(box), CLUTTER_LINEAR);
    g_object_set(box, "spacing", 10, NULL);
    clutter_actor_set_layout_manager(priv->top, box);

    g_object_connect(G_OBJECT(priv->top), "signal::destroy", _emit_destroy, self, NULL);
    g_object_connect(priv->top,
            "signal::button-press-event", on_button_press, self,
            "signal::key-press-event", on_key_press, self,
            "signal::key-release-event", on_key_release, self,
            "signal::captured-event", on_captured_event, self,
            NULL);
    clutter_actor_hide(priv->top);
}

static void meta_switcher_dispose(GObject *object)
{
    MetaSwitcher *switcher = META_SWITCHER(object);
    MetaSwitcherPrivate* priv = switcher->priv;

    if (priv->disposed) return;
    priv->disposed = TRUE;

    MetaScreen* screen = meta_plugin_get_screen(priv->plugin);
    ClutterActor* stage = meta_get_stage_for_screen(screen);
    clutter_actor_remove_child(stage, priv->top);

    if (priv->modaled) {
        meta_plugin_end_modal(priv->plugin, clutter_get_current_event_time());
        meta_enable_unredirect_for_screen(screen);

        if (priv->selected_id < 0 && priv->previous_focused)
            clutter_stage_set_key_focus(CLUTTER_STAGE(stage), priv->previous_focused);
    }

    if (CLUTTER_IS_ACTOR(priv->top)) {
        g_clear_pointer(&priv->top, clutter_actor_destroy);
    } else priv->top = NULL;

    if (priv->autoclose_id) {
        g_source_remove(priv->autoclose_id);
        priv->autoclose_id = 0;
    }

    GList* ws_list = meta_screen_get_workspaces(screen);
    g_list_foreach(ws_list, (GFunc)unhook_ws_event, switcher);

    G_OBJECT_CLASS(meta_switcher_parent_class)->dispose(object);
}

static void meta_switcher_finalize(GObject *object)
{
    MetaSwitcher *switcher = META_SWITCHER(object);
    MetaSwitcherPrivate* priv = switcher->priv;

    if (priv->apps) g_ptr_array_unref(priv->apps);
    if (priv->icons) g_hash_table_destroy(priv->icons);
    if (priv->snapshot) cairo_surface_destroy(priv->snapshot);

    G_OBJECT_CLASS(meta_switcher_parent_class)->finalize(object);
}

static void meta_switcher_class_init(MetaSwitcherClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->set_property = meta_switcher_set_property;
    gobject_class->get_property = meta_switcher_get_property;
    gobject_class->dispose = meta_switcher_dispose;
    gobject_class->finalize = meta_switcher_finalize;

    /* object properties */
    property_specs[PROP_WORKSPACE] = g_param_spec_pointer(
            "workspace",
            "active workspace",
            "active workspace",
            G_PARAM_READABLE | G_PARAM_WRITABLE);

    property_specs[PROP_PLUGIN] = g_param_spec_pointer(
            "plugin",
            "plugin",
            "plugin",
            G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

    g_object_class_install_properties(gobject_class, N_PROPERTIES, property_specs);
    /* object properties end */

    /* object signals */
    signals[DESTROY] = g_signal_new("destroy",
            G_OBJECT_CLASS_TYPE(klass),
            G_SIGNAL_RUN_FIRST,
            G_STRUCT_OFFSET(MetaSwitcherClass, destroy),
            NULL, NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);
    /* object signals end */
}

gint meta_switcher_error_quark(void)
{
    return g_quark_from_static_string("meta-switcher-error-quark");
}

static gboolean on_autoclose_timeout(gpointer data)
{
    g_debug("%s", __func__);

    MetaSwitcher* self = META_SWITCHER(data);
    g_object_unref(self);

    return G_SOURCE_REMOVE;
}

static void _capture_desktop(MetaSwitcher* self)
{
    MetaSwitcherPrivate* priv = self->priv;
    MetaScreen* screen = meta_plugin_get_screen(priv->plugin);
    ClutterActor* stage = meta_get_stage_for_screen(screen);

    gfloat tx, ty, w, h;
    clutter_actor_get_position(priv->top, &tx, &ty);
    clutter_actor_get_size(priv->top, &w, &h);

    g_debug("%s: %f, %f, %f, %f", __func__, tx, ty, w, h);
    if (priv->snapshot) {
        cairo_surface_destroy(priv->snapshot);
        priv->snapshot = NULL;
    }
    priv->snapshot_offset = 20.0;
    w += priv->snapshot_offset*3;
    clutter_stage_ensure_redraw(CLUTTER_STAGE(stage));

    guchar* data = g_malloc(w*h*4);
    cogl_framebuffer_read_pixels(cogl_get_draw_framebuffer(),
            tx-priv->snapshot_offset, ty, w, h,
            CLUTTER_CAIRO_FORMAT_ARGB32, data);

    /*guchar* data = clutter_stage_read_pixels(CLUTTER_STAGE(stage), */
            /*tx-priv->snapshot_offset, ty, w, h);*/
    priv->snapshot = cairo_image_surface_create_for_data(data,
            CAIRO_FORMAT_ARGB32, w, h, w*4);
    g_free(data);
}

static void _draw_round_box(cairo_t* cr, gint width, gint height, double radius)
{
    cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);

    double xc = radius, yc = radius;
    double angle1 = 180.0  * (M_PI/180.0);  /* angles are specified */
    double angle2 = 270.0 * (M_PI/180.0);  /* in radians           */

    cairo_arc (cr, xc, yc, radius, angle1, angle2);

    xc = width - radius;
    angle1 = 270.0 * (M_PI/180.0);
    angle2 = 360.0 * (M_PI/180.0);
    cairo_arc (cr, xc, yc, radius, angle1, angle2);

    yc = height - radius;
    angle1 = 0.0 * (M_PI/180.0);
    angle2 = 90.0 * (M_PI/180.0);
    cairo_arc (cr, xc, yc, radius, angle1, angle2);

    xc = radius;
    angle1 = 90.0 * (M_PI/180.0);
    angle2 = 180.0 * (M_PI/180.0);
    cairo_arc (cr, xc, yc, radius, angle1, angle2);

    cairo_close_path(cr);
}

/* FIXME: when there are too many windows, the round box and icons need to be scaled */
static gboolean on_switcher_background_draw(ClutterCanvas* canvas, cairo_t* cr,
        gint width, gint height, MetaSwitcher* self)
{
#if DEBUG
    g_message("%s, line %d, %dx%d\n", __func__, __LINE__, width, height);
#endif

    MetaSwitcherPrivate* priv = self->priv;

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    _draw_round_box(cr, width, height, 20.0);
    cairo_clip(cr);

    /*_capture_desktop(self);*/

    if (priv->snapshot) {
        cairo_set_source_surface(cr, priv->snapshot, -priv->snapshot_offset, 0);
        cairo_paint(cr);
    }

    return TRUE;
}

static gboolean on_icon_background_draw(ClutterCanvas* canvas, cairo_t* cr,
        gint width, gint height, ClutterActor* actor)
{
    WindowPrivate* win_priv = get_window_private(actor);
    if (!win_priv->highlight) {
        cairo_set_source_rgba(cr, 0.2, 0.2, 0.2, 0.0);
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        cairo_paint(cr);
    } else {
        cairo_set_source_rgba(cr, 0.2, 0.2, 0.2, 0.90);
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        _draw_round_box(cr, width, height, 10.0);
        cairo_fill(cr);
    }

    return TRUE;
}

static ClutterActor* load_icon_for_window(MetaSwitcher* self, MetaWindow* window)
{
    MetaSwitcherPrivate *priv = self->priv;
    MetaScreen *screen = meta_plugin_get_screen(priv->plugin);
    ShellWindowTracker* tracker = shell_window_tracker_get_default();
    ShellApp* app = shell_window_tracker_get_window_app(tracker, window);
    gint screen_width, screen_height, app_icon_size = -1;

    /* TODO: @sonald scaled app icon size at first */
    meta_screen_get_size(screen, &screen_width, &screen_height);
    if (priv->apps->len)
        app_icon_size = screen_width / priv->apps->len;

    if (app_icon_size == -1 || app_icon_size > APP_ICON_SIZE)
        app_icon_size = APP_ICON_SIZE;

    if (app)
        return shell_app_create_icon_texture(app, app_icon_size);

    return NULL;
}

static void _add_app(MetaSwitcher* self, MetaWindow* app, gint* x, gint* y)
{
    MetaSwitcherPrivate* priv = self->priv;

    if (!priv->icons)
        priv->icons = g_hash_table_new(NULL, NULL);

    // container
    ClutterActor* actor = clutter_actor_new();
    WindowPrivate* win_priv = get_window_private(actor);
    win_priv->window = app;
    win_priv->highlight = FALSE;
    g_hash_table_insert(priv->icons, app, actor);

    gint w = APP_ACTOR_WIDTH, h = APP_ACTOR_HEIGHT;

    // add children
    ClutterContent* canvas = clutter_canvas_new();
    g_signal_connect(canvas, "draw", G_CALLBACK(on_icon_background_draw), actor);
    clutter_canvas_set_size(CLUTTER_CANVAS(canvas), w, h);

    ClutterActor* bg = clutter_actor_new();
    clutter_actor_set_name(bg, "bg");
    clutter_actor_set_content(bg, canvas);
    g_object_unref(canvas);

    clutter_actor_set_size(bg, w, h);
    g_object_set(bg, "x-expand", TRUE, "y-expand", TRUE, "x-align", CLUTTER_ACTOR_ALIGN_FILL,
            "y-align", CLUTTER_ACTOR_ALIGN_FILL, NULL);
    clutter_actor_add_child(actor, bg);

    ClutterActor* icon = load_icon_for_window(self, app);
    if (icon) clutter_actor_add_child(actor, icon);

    ClutterActor* label = clutter_text_new();
    clutter_actor_add_child(actor, label);
    clutter_text_set_text(CLUTTER_TEXT(label), meta_window_get_title(app));
    clutter_text_set_font_name(CLUTTER_TEXT(label), "Sans 10");
    ClutterColor clr = {0xff, 0xff, 0xff, 0xff};
    clutter_text_set_color(CLUTTER_TEXT(label), &clr);
    clutter_text_set_ellipsize(CLUTTER_TEXT(label), PANGO_ELLIPSIZE_END);
    g_object_set(label, "x-align", CLUTTER_ACTOR_ALIGN_CENTER, "y-align", CLUTTER_ACTOR_ALIGN_CENTER, NULL);

    gfloat pref_width = clutter_actor_get_width(label);
    if (pref_width > w-10) {
        pref_width = w-10;
        clutter_actor_set_width(label, pref_width);
    }
    clutter_actor_set_position(label, (APP_ACTOR_WIDTH-pref_width)/2, APP_ICON_SIZE+2);

    g_debug("%s: size: %d, %d", __func__, w, h);
    clutter_actor_show(actor);

    clutter_actor_add_child(priv->top, actor);

    *x += w;
}

static void meta_switcher_present_list(MetaSwitcher* self)
{
    MetaSwitcherPrivate* priv = self->priv;
    MetaScreen* screen = meta_plugin_get_screen(priv->plugin);
    MetaDisplay* display = meta_screen_get_display(screen);

    // windows on all workspaces
    GList* ls = meta_display_get_tab_list(display, META_TAB_LIST_NORMAL, NULL);
    if (!ls) return;

    GList* ws_list = meta_screen_get_workspaces(screen);
    g_list_foreach(ws_list, (GFunc)hook_ws_event, self);

    if (!priv->apps) {
        priv->apps = g_ptr_array_new();
        GList* orig = ls;
        while (ls) {
            g_ptr_array_add(priv->apps, ls->data);
            ls = ls->next;
        }
        g_list_free(orig);
    }

    // do not set width, so layout will give a reasonable width dynamically
    clutter_actor_set_height(priv->top, APP_ACTOR_HEIGHT + 20);

    gint x = 0, y = 0;
    for (int i = 0; i < priv->apps->len; i++) {
        _add_app(self, g_ptr_array_index(priv->apps, i), &x, &y);
    }

    clutter_actor_set_margin_left(g_hash_table_lookup(priv->icons, g_ptr_array_index(priv->apps, 0)), 10);
    clutter_actor_set_margin_right(g_hash_table_lookup(priv->icons, g_ptr_array_index(priv->apps, priv->apps->len-1)), 10);

    gint screen_width = 0, screen_height = 0;
    meta_screen_get_size(screen, &screen_width, &screen_height);

    gfloat w = clutter_actor_get_width(priv->top), h = clutter_actor_get_height(priv->top),
           tx = (screen_width - w)/2, ty = (screen_height - h)/2;
    clutter_actor_set_position(priv->top, tx, ty);

    ClutterContent* canvas = clutter_canvas_new();
    clutter_canvas_set_size(CLUTTER_CANVAS(canvas), w, h);
    clutter_actor_set_content(priv->top, canvas);
    g_object_unref(canvas);
    g_signal_connect(canvas, "draw", G_CALLBACK(on_switcher_background_draw), self);

    clutter_content_invalidate(canvas);

    priv->selected_id = 0;
}

static gboolean on_captured_event(ClutterActor *actor, ClutterEvent *event, MetaSwitcher* self)
{
    if (event->type == CLUTTER_KEY_PRESS) {
        g_debug("%s: key press", __func__);
    }
    return FALSE;
}

static gboolean on_button_press(ClutterActor *actor, ClutterEvent *event, MetaSwitcher* self)
{
    g_debug("%s", __func__);
    return FALSE;
}

//FIXME: actually key_release is activated even when key press (WTF). does it because I'm hold alt/super?
static gboolean on_key_release(ClutterActor *actor, ClutterEvent *event, MetaSwitcher* self)
{
    MetaSwitcherPrivate* priv = self->priv;
    MetaScreen* screen = meta_plugin_get_screen(priv->plugin);
    MetaDisplay* display = meta_screen_get_display(screen);

    ClutterModifierType state = clutter_event_get_state(event);
    guint keysym = clutter_event_get_key_symbol(event);
    guint action = meta_display_get_keybinding_action(display, clutter_event_get_key_code(event), state);

    int id = priv->selected_id;
    if (action == META_KEYBINDING_ACTION_SWITCH_WINDOWS) {
        id = (id + 1) % priv->apps->len;
    } else if (action == META_KEYBINDING_ACTION_SWITCH_WINDOWS_BACKWARD) {
        id = (priv->apps->len + id - 1) % priv->apps->len;
    } else if (action == META_KEYBINDING_ACTION_SWITCH_APPLICATIONS) {
        id = (id + 1) % priv->apps->len;
    } else if (action == META_KEYBINDING_ACTION_SWITCH_APPLICATIONS_BACKWARD) {
        id = (priv->apps->len + id - 1) % priv->apps->len;
    }
    _set_highlight(self, priv->selected_id, FALSE);
    _set_highlight(self, id, TRUE);
    g_debug("%s, key: 0x%x, action: %d, previd: %d, now: %d", __func__, keysym, action,
            priv->selected_id, id);
    priv->selected_id = id;

    switch(keysym) {
        //FIXME: do not hardcode keysyms, use action instead
        case CLUTTER_KEY_Super_L:
        case CLUTTER_KEY_Super_R:
        case CLUTTER_KEY_Alt_L:
        case CLUTTER_KEY_Alt_R:
        {
            if (priv->selected_id >= 0) {
                meta_window_activate(g_ptr_array_index(priv->apps, priv->selected_id), clutter_get_current_event_time());
            }
            g_object_unref(self);
        }
        default: break;
    }

    return FALSE;
}

static gboolean on_key_press(ClutterActor *actor, ClutterEvent *event, MetaSwitcher* self)
{
    MetaSwitcherPrivate* priv = self->priv;

    g_debug("%s", __func__);

    guint keysym = clutter_event_get_key_symbol(event);

    if (keysym == CLUTTER_KEY_Escape) {
        if (!priv->autoclose_id) {
            priv->autoclose_id = g_timeout_add(AUTOCLOSE_TIMEOUT, on_autoclose_timeout, self);
        }
        return TRUE;
    }

    if (priv->autoclose_id) {
        g_source_remove(priv->autoclose_id);
        priv->autoclose_id = 0;
    }
    return FALSE;
}

gboolean meta_switcher_show(MetaSwitcher* self)
{
    MetaSwitcherPrivate* priv = self->priv;
    int screen_width, screen_height;

    MetaScreen* screen = meta_plugin_get_screen(priv->plugin);
    priv->workspace = meta_screen_get_active_workspace(screen);

    meta_screen_get_size(screen, &screen_width, &screen_height);

    meta_switcher_present_list(self);
    if (priv->apps == NULL || priv->apps->len == 0) goto _end;

    _capture_desktop(self);
    clutter_content_invalidate(clutter_actor_get_content(priv->top));

    ClutterActor* stage = meta_get_stage_for_screen(screen);
    clutter_actor_insert_child_above(stage, priv->top, NULL);
    clutter_actor_show(priv->top);

    if (!meta_plugin_begin_modal(priv->plugin, 0, clutter_get_current_event_time())) {
        if (!meta_plugin_begin_modal(priv->plugin, META_MODAL_POINTER_ALREADY_GRABBED,
                    clutter_get_current_event_time())) {
            g_warning("can not be modal");
            goto _end;
        }
    }

    meta_disable_unredirect_for_screen(screen);
    priv->modaled = TRUE;

    priv->previous_focused = clutter_stage_get_key_focus(CLUTTER_STAGE(stage));
    if (priv->previous_focused == stage) priv->previous_focused = NULL;
    clutter_stage_set_key_focus(CLUTTER_STAGE(stage), priv->top);
    clutter_actor_grab_key_focus(priv->top);

    return TRUE;

_end:
    clutter_actor_hide(priv->top);
    return FALSE;
}


// }}}
