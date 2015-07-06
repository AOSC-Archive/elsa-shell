/* Copyrgiht (C) 2014 - 2015 Sian Cao <siyuan.cao@i-soft.com.cn> */

/**
 * SECTION:overview-head
 * @title
 * @short_description:
 *
 * Description...
 */

#include "ov_header.h"
#include "overview.h"
#include <clutter/clutter.h>
#include <math.h>
#include <meta/display.h>

struct _OverviewHeadPrivate
{
    ClutterActor* content;
    MosesOverview* overview;
    int weight;
    int height;
    GPtrArray* ws_sprites;
};

// {{{ GObject property and signal enums

enum OverviewHeadProp
{
    PROP_0,
    PROP_HEIGHT,
    PROP_WEIGHT,
    PROP_OVERVIEW,
    N_PROPERTIES
};

enum OverviewHeadSignal
{
    WORKSPACE_ACTIVATED,
    N_SIGNALS
};

static guint signals[N_SIGNALS];

static GParamSpec* property_specs[N_PROPERTIES] = {NULL, };
static gfloat HEAD_SIZE = 120;

// }}}

static GQuark overview_head_ws_quark(void)
{
    return g_quark_from_static_string("overview-head-ws-quark");
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

static gboolean on_ws_background_draw(ClutterCanvas* canvas, cairo_t* cr,
                                      gint width, gint height, gpointer data)
{
    ClutterActor* ws_actor = CLUTTER_ACTOR(data);
    MetaWorkspace* ws = META_WORKSPACE(g_object_get_qdata(G_OBJECT(ws_actor), overview_head_ws_quark()));

    MetaScreen* screen = meta_workspace_get_screen(ws);
    gboolean is_active = meta_screen_get_active_workspace_index(screen) == meta_workspace_index(ws);

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    _draw_round_box(cr, width, height, 5.0);
    cairo_clip(cr);

    cairo_set_source_rgba(cr, 0.5, 0.4, 0.2, is_active ? 1.0 : 0.0);
    cairo_paint(cr);
    return TRUE;
}

static gboolean on_ws_button_pressed(ClutterActor* actor, ClutterEvent* ev, gpointer data)
{
    OverviewHead* self = OVERVIEW_HEAD(data);
    MetaWorkspace* ws = g_object_get_qdata(G_OBJECT(actor), overview_head_ws_quark());
    g_signal_emit(self, signals[WORKSPACE_ACTIVATED], 0, ws);

    return TRUE;
}

static GSList* gslist_from_glist(GList* l, gboolean free_orig)
{
    GSList* sl = NULL;
    GList* l2 = l;
    while (l2) {
        sl = g_slist_append(sl, l2->data);
        l2 = l2->next;
    }

    if (free_orig) g_list_free(l);
    return sl;
}

static ClutterActor *prepare_workspace(OverviewHead* self, const GList *ws_list, ClutterSize ref_size, gfloat x, gfloat y)
{
    MetaWorkspace* ws = META_WORKSPACE(ws_list->data);
    ClutterActor* ws_actor =  clutter_actor_new();
    clutter_actor_set_position(ws_actor, x, y);
    clutter_actor_set_size(ws_actor, ref_size.width, ref_size.height);

    g_object_set_qdata(G_OBJECT(ws_actor), overview_head_ws_quark(), ws);

    ClutterContent* canvas = clutter_canvas_new();
    clutter_canvas_set_size(CLUTTER_CANVAS(canvas), ref_size.width, ref_size.height);
    clutter_actor_set_content(ws_actor, canvas);
    g_object_unref(canvas);

    g_signal_connect(canvas, "draw", G_CALLBACK(on_ws_background_draw), ws_actor);
    g_signal_connect(ws_actor, "button-press-event", G_CALLBACK(on_ws_button_pressed), self);
    clutter_actor_set_reactive(ws_actor, TRUE);
    clutter_content_invalidate(canvas);

    MetaRectangle geom;
    MetaScreen* screen = meta_plugin_get_screen(overview_get_plugin(self->priv->overview));
    int focused_monitor = meta_screen_get_current_monitor(screen);

    meta_screen_get_monitor_geometry(screen, focused_monitor, &geom);
    gfloat scalex = 0.96 * ref_size.width / geom.width,
            scaley = 0.96 * ref_size.height / geom.height,
            off_x = ref_size.width * 0.02,
            off_y = ref_size.height * 0.02;

    MetaDisplay* display = meta_screen_get_display(screen);
    GSList* l = meta_display_sort_windows_by_stacking(display, gslist_from_glist(meta_workspace_list_windows(ws), TRUE));
    GSList* sl = l;
    while (sl) {
        MetaWindow* win = sl->data;
        MetaWindowActor* win_actor = META_WINDOW_ACTOR(meta_window_get_compositor_private(win));

        if ((meta_window_get_window_type(win) == META_WINDOW_NORMAL) ||
            meta_window_get_window_type(win) == META_WINDOW_DESKTOP) {
            gboolean minimized = FALSE;
            g_object_get(G_OBJECT(win), "minimized", &minimized, NULL);
            if (!minimized) {
                ClutterActor* clone = clutter_clone_new(CLUTTER_ACTOR(win_actor));

                gfloat x = 0, y = 0;
                clutter_actor_get_position(CLUTTER_ACTOR(win_actor), &x, &y);
                clutter_actor_set_scale(clone, scalex, scaley);
                clutter_actor_set_position(clone, x * scalex + off_x, y * scaley + off_y);

                clutter_actor_insert_child_above(ws_actor, clone, NULL);
                g_debug("%s: add clone %s", __func__, meta_window_get_description(win));
            }
        }
        sl = sl->next;
    }
    g_slist_free(l);

    ClutterColor title_clr = CLUTTER_COLOR_INIT(0xff, 0x80, 0x0, 0xee);
    char title[20] = "";
    snprintf(title, sizeof title - 1, "desktop %d", meta_workspace_index(ws));
    ClutterActor* mark = clutter_text_new_full("Source Han Sans J", title, &title_clr);
    clutter_actor_set_position(mark, (ref_size.width - clutter_actor_get_width(mark))/2,
                               (ref_size.height - clutter_actor_get_height(mark) /2));
    clutter_actor_add_child(ws_actor, mark);
    return ws_actor;
}

static void overview_head_prepare_content(OverviewHead* self, MetaRectangle *geom, MetaScreen *screen, OverviewHeadPrivate *priv)
{
    GList* ws_list = meta_screen_get_workspaces(screen);
    gfloat ws_size = meta_screen_get_n_workspaces(screen);

    ClutterSize ref_size;
    ref_size.height = HEAD_SIZE * 0.8;
    ref_size.width = ref_size.height / 0.618;

    gfloat x = ((*geom).width - (ws_size * ref_size.width + (ws_size-1) * 20)) / 2.0;
    gfloat y = (HEAD_SIZE - ref_size.height) / 2.0;

    priv->ws_sprites = g_ptr_array_new();
    while (ws_list) {
        ClutterActor *ws_actor = prepare_workspace(self, ws_list, ref_size, x, y);
        g_ptr_array_add(priv->ws_sprites, ws_actor);

        clutter_actor_add_child(priv->content, ws_actor);
        x += ref_size.width + 20;
        ws_list = ws_list->next;
    }
}

/**
 * overview_head_new:
 *
 * Create new #OverviewHead object.
 *
 * Returns: #OverviewHead object.
 */
OverviewHead* overview_head_new(MosesOverview* ov)
{
    OverviewHead *self = g_object_new(OVERVIEW_TYPE_HEAD, "overview", ov, NULL);

    MetaRectangle geom;
    MetaScreen* screen = meta_plugin_get_screen(overview_get_plugin(ov));
    int focused_monitor = meta_screen_get_current_monitor(screen);
    meta_screen_get_monitor_geometry(screen, focused_monitor, &geom);

    OverviewHeadPrivate* priv = self->priv;
    ClutterActor* head = priv->content;
    clutter_actor_set_position(head, geom.x, geom.y);
    if (HEAD_SIZE > geom.height / 5) HEAD_SIZE = geom.height / 5;
    clutter_actor_set_size(head, geom.width, HEAD_SIZE);
    //FIXME: why this crash?
    // g_object_set(self, "height", HEAD_SIZE, "weight", geom.width, NULL);
    priv->height = HEAD_SIZE, priv->weight = geom.width;

    overview_head_prepare_content(self, &geom, screen, priv);

    return self;
}

// {{{ GObject type setup

static void overview_head_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    OverviewHead* head = OVERVIEW_HEAD(object);
    OverviewHeadPrivate* priv = head->priv;

    switch (property_id)
    {
        case PROP_HEIGHT:
            priv->height = g_value_get_int(value); break;

        case PROP_WEIGHT:
            priv->weight = g_value_get_int(value); break;

        case PROP_OVERVIEW:
            priv->overview = g_value_get_pointer(value); break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void overview_head_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    OverviewHead* head = OVERVIEW_HEAD(object);
    OverviewHeadPrivate* priv = head->priv;

    switch (property_id)
    {
        case PROP_HEIGHT:
            g_value_set_int(value, priv->height); break;

        case PROP_WEIGHT:
            g_value_set_int(value, priv->weight); break;

        case PROP_OVERVIEW:
            g_value_set_pointer(value, priv->overview); break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

G_DEFINE_TYPE(OverviewHead, overview_head, G_TYPE_OBJECT);

static void overview_head_init(OverviewHead *head)
{
    head->priv = G_TYPE_INSTANCE_GET_PRIVATE(head, OVERVIEW_TYPE_HEAD, OverviewHeadPrivate);
    head->priv->content = clutter_actor_new();

    ClutterColor clr = CLUTTER_COLOR_INIT(0, 0, 0, 0x80);
    clutter_actor_set_background_color(CLUTTER_ACTOR(head->priv->content), &clr);
}

static void overview_head_dispose(GObject *object)
{
    OverviewHead *head = OVERVIEW_HEAD(object);
    G_GNUC_UNUSED OverviewHeadPrivate* priv = head->priv;

    // Free everything that may hold reference to OverviewHead
    if (priv->overview) {
        priv->overview = NULL;
        g_clear_pointer(&priv->ws_sprites, g_ptr_array_unref);
        g_clear_pointer(&priv->content, clutter_actor_destroy);
    }

    G_OBJECT_CLASS(overview_head_parent_class)->dispose(object);
}

static void overview_head_finalize(GObject *object)
{
    OverviewHead *head = OVERVIEW_HEAD(object);
    G_GNUC_UNUSED OverviewHeadPrivate* priv = head->priv;


    G_OBJECT_CLASS(overview_head_parent_class)->finalize(object);
}

static void overview_head_class_init(OverviewHeadClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GParamSpec *param_spec;

    gobject_class->set_property = overview_head_set_property;
    gobject_class->get_property = overview_head_get_property;
    gobject_class->dispose = overview_head_dispose;
    gobject_class->finalize = overview_head_finalize;

    g_type_class_add_private(klass, sizeof(OverviewHeadPrivate));

    /* object properties */
    property_specs[PROP_HEIGHT] = g_param_spec_int(
            "height",
            "height",
            "height",
            0,
            INT_MAX,
            0,
            G_PARAM_READABLE | G_PARAM_WRITABLE);
    property_specs[PROP_WEIGHT] = g_param_spec_int(
            "weight",
            "weight",
            "weight",
            0,
            INT_MAX,
            0,
            G_PARAM_READABLE | G_PARAM_WRITABLE);
    property_specs[PROP_OVERVIEW] = g_param_spec_pointer(
            "overview",
            "overview",
            "overview",
            G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

    g_object_class_install_properties(gobject_class, N_PROPERTIES, property_specs);

    /* object properties end */

    /* object signals */

    signals[WORKSPACE_ACTIVATED] = g_signal_new ("workspace-activated",
                                                 OVERVIEW_TYPE_HEAD,
                                                 G_SIGNAL_RUN_LAST,
                                                 0,
                                                 NULL, NULL, NULL,
                                                 G_TYPE_NONE, 1, META_TYPE_WORKSPACE);

    /* object signals end */
}

ClutterActor* overview_head_get_content(OverviewHead* self)
{
    return self->priv->content;
}

GQuark overview_head_error_quark(void)
{
    return g_quark_from_static_string("overview-head-error-quark");
}

ClutterActor* overview_head_get_actor_for_workspace(OverviewHead* self, MetaWorkspace* ws)
{
    OverviewHeadPrivate* priv = self->priv;
    for (int i = 0; priv->ws_sprites->len; i++) {
        ClutterActor* actor = g_ptr_array_index(priv->ws_sprites, i);
        if (g_object_get_qdata(G_OBJECT(actor), overview_head_ws_quark()) == ws) {
            return actor;
        }
    }
    return NULL;
}
// }}}
