/* Copyrgiht (C) 2014 - 2015 Sian Cao <siyuan.cao@i-soft.com.cn> */

#include "overview.h"
#include "elsa-wm-plugin.h"
#include "ov_header.h"

#include <math.h>

#include <clutter/clutter.h>
#include <meta/workspace.h>
#include <meta/compositor.h>
#include <meta/compositor-mutter.h>
#include <meta/window.h>
#include <meta/keybindings.h>
#include <meta/meta-plugin.h>
#include <meta/main.h>
#include <meta/display.h>
#include <meta/meta-background.h>
#include <cairo.h>

static gboolean on_bg_button_press(ClutterActor *actor, ClutterEvent *event, MosesOverview* self);
static gboolean on_key_press(ClutterActor *actor, ClutterEvent *event, MosesOverview* self);
static gboolean on_key_release(ClutterActor *actor, ClutterEvent *event, MosesOverview* self);
static void create_window_badge(MosesOverview* self, ClutterActor* parent, int order);

static const int PLACEMENT_ANIMATION_DURATION = 250;

struct _MosesOverviewPrivate
{
    MetaPlugin* plugin;
    GPtrArray* clones;
    ClutterActor* background_actor;
    ClutterActor* previous_focused;

    ClutterActor* selected_actor; // will be new focus
    MetaWorkspace* selected_workspace;

    GPtrArray* badges;
    int nr_complete; // presenting effect completion count

    OverviewHead* ov_head;

    guint ready: 1; // windows presented
    guint modaled: 1;
    guint disposed: 1;
};

// {{{ GObject property and signal enums

enum MosesOverviewProp
{
    PROP_0,
    PROP_PLUGIN,
    N_PROPERTIES
};
static GParamSpec* property_specs[N_PROPERTIES] = {NULL, };

enum MosesOverviewSignal
{
    N_SIGNALS
};

static guint signals[N_SIGNALS];

static const gint moses_overview_window_clone_order(void)
{
    return g_quark_from_static_string("moses-overview-window-clone-order-quark");
}
// }}}

/**
 * moses_overview_new:
 *
 * Create new #MosesOverview object.
 *
 * Returns: #MosesOverview object.
 */
MosesOverview* moses_overview_new(MetaPlugin* plugin)
{
    MosesOverview *overview = g_object_new(MOSES_TYPE_OVERVIEW, "plugin", plugin, NULL);

    return overview;
}

// {{{ GObject type setup

static void moses_overview_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    MosesOverview* overview = MOSES_OVERVIEW(object);
    MosesOverviewPrivate* priv = overview->priv;

    switch (property_id)
    {
        case PROP_PLUGIN:
            priv->plugin = g_value_get_pointer(value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void moses_overview_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    MosesOverview* overview = MOSES_OVERVIEW(object);
    MosesOverviewPrivate* priv = overview->priv;

    switch (property_id)
    {
        case PROP_PLUGIN:
            g_value_set_pointer(value, priv->plugin);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

G_DEFINE_TYPE_WITH_PRIVATE(MosesOverview, moses_overview, CLUTTER_TYPE_ACTOR)

static void prepare_workspace_content(MosesOverview *self, MetaWorkspace *ws);

static void moses_overview_init(MosesOverview *overview)
{
    overview->priv = moses_overview_get_instance_private(overview);
    overview->priv->nr_complete = 0;
    overview->priv->selected_actor = NULL;
    overview->priv->selected_workspace = NULL;
    overview->priv->clones = NULL;
    overview->priv->badges = NULL;
    overview->priv->plugin = NULL;
    overview->priv->background_actor = NULL;
    overview->priv->ov_head = NULL;
}

static void moses_overview_dispose(GObject *object)
{
    MosesOverview *overview = MOSES_OVERVIEW(object);
    MosesOverviewPrivate* priv = overview->priv;

    if (priv->disposed) return;
    priv->disposed = TRUE;

    g_clear_pointer(&priv->ov_head, g_object_unref);

    MetaScreen* screen = meta_plugin_get_screen(priv->plugin);
    ClutterActor* stage = meta_get_stage_for_screen(screen);

    ClutterActor* to_focus = NULL;
    if (priv->selected_actor) {
        to_focus = clutter_clone_get_source(CLUTTER_CLONE(priv->selected_actor));
    }

    for (int i = 0; priv->clones && i < priv->clones->len; i++) {
        ClutterActor* clone = g_ptr_array_index(priv->clones, i);
        ClutterActor* orig = clutter_clone_get_source(CLUTTER_CLONE(clone));
        clutter_actor_show(orig); // FIXME: maybe some actors had not been shown.
        clutter_actor_destroy(clone);
    }

    for (int i = 0; priv->badges && i < priv->badges->len; i++) {
        clutter_actor_destroy(CLUTTER_ACTOR(g_ptr_array_index(priv->badges, i)));
    }

    if (priv->background_actor) {
        clutter_actor_show(clutter_clone_get_source(CLUTTER_CLONE(priv->background_actor)));
        g_clear_pointer(&priv->background_actor, clutter_actor_destroy);
    }

    if (priv->modaled) {
        meta_plugin_end_modal(priv->plugin, clutter_get_current_event_time());
        meta_enable_unredirect_for_screen(screen);

        if (priv->selected_workspace) {
            meta_workspace_activate(priv->selected_workspace, CLUTTER_CURRENT_TIME);
            MetaDisplay* display = meta_screen_get_display(screen);
            meta_compositor_switch_workspace(meta_display_get_compositor(display),
                    meta_screen_get_active_workspace(screen),
                    priv->selected_workspace, META_MOTION_DOWN);

        } else if (to_focus) {
            clutter_stage_set_key_focus(CLUTTER_STAGE(stage), to_focus);
            MetaWindowActor* actor = META_WINDOW_ACTOR(to_focus);
            MetaWindow* win = meta_window_actor_get_meta_window(actor);
            meta_window_raise(win);
            meta_window_focus(win, CLUTTER_CURRENT_TIME);

        } else if (priv->previous_focused) {
            if (!CLUTTER_IS_STAGE(priv->previous_focused)) {
                clutter_stage_set_key_focus(CLUTTER_STAGE(stage), priv->previous_focused);
            }
        }
    }

    G_OBJECT_CLASS(moses_overview_parent_class)->dispose(object);
}

static void moses_overview_finalize(GObject *object)
{
    MosesOverview *overview = MOSES_OVERVIEW(object);
    MosesOverviewPrivate* priv = overview->priv;
    if (priv->modaled) {
        if (priv->clones) g_ptr_array_free(priv->clones, FALSE);
        if (priv->badges) g_ptr_array_free(priv->badges, FALSE);
    }

    G_OBJECT_CLASS(moses_overview_parent_class)->finalize(object);
}

static void moses_overview_class_init(MosesOverviewClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GParamSpec *param_spec;

    gobject_class->set_property = moses_overview_set_property;
    gobject_class->get_property = moses_overview_get_property;
    gobject_class->dispose = moses_overview_dispose;
    gobject_class->finalize = moses_overview_finalize;

    /* object properties */
    property_specs[PROP_PLUGIN] = g_param_spec_pointer(
            "plugin",
            "plugin",
            "plugin",
            G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

    g_object_class_install_properties(gobject_class, N_PROPERTIES, property_specs);

    /* object properties end */

    /* object signals */

    /* object signals end */
}

gint moses_overview_error_quark(void)
{
    return g_quark_from_static_string("moses-overview-error-quark");
}

static void create_overlay_badges(MosesOverview* self)
{
    MosesOverviewPrivate* priv = self->priv;
    if (!priv->badges) {
        priv->badges = g_ptr_array_new_full(0, (GDestroyNotify)clutter_actor_destroy);

    } else if (priv->badges->len) {
        g_ptr_array_set_size(priv->badges, 0);
    }

    int order = 1;
    for (int i = 0; i < priv->clones->len; i++) {
        create_window_badge(self, g_ptr_array_index(priv->clones, i), order++);
    }
}

static void on_effect_complete(ClutterActor* actor, MosesOverview* self)
{
    MosesOverviewPrivate* priv = self->priv;
    g_debug("%s", __func__);
    priv->nr_complete++;
    if (priv->nr_complete == priv->clones->len) {
        g_debug("ready");
        priv->ready = TRUE;

        create_overlay_badges(self);
    }
}

static void on_restore_position_effect_complete(ClutterActor* actor, MosesOverview* self)
{
    MosesOverviewPrivate* priv = self->priv;
    g_debug("%s", __func__);
    clutter_actor_destroy(CLUTTER_ACTOR(self));
}

static void overview_animated_destroy(MosesOverview* self, MosesOverviewQuitReason reason, gboolean animate)
{
    MosesOverviewPrivate* priv = self->priv;

    gboolean just_destroy = !animate;
    if (reason == MOSES_OV_REASON_ACTIVATE_WINDOW && !priv->selected_actor) {
        just_destroy = TRUE;
    } else if (reason == MOSES_OV_REASON_ACTIVATE_WORKSPACE && !priv->selected_workspace) {
        just_destroy = TRUE;
    } else if (reason == MOSES_OV_REASON_NORMAL) {
        just_destroy = TRUE;
    }

    if (just_destroy) {
        clutter_actor_destroy(CLUTTER_ACTOR(self));
        return;
    }

    gfloat x, y, w, h;
    ClutterActor* target = NULL;

    if (reason == MOSES_OV_REASON_ACTIVATE_WINDOW) {
        target = self->priv->selected_actor;

        ClutterActor* orig = clutter_clone_get_source(CLUTTER_CLONE(target));
        clutter_actor_get_position(orig, &x, &y);
        clutter_actor_get_size(orig, &w, &h);
        g_signal_handlers_disconnect_by_func(target, on_effect_complete, self);

    } else if (reason == MOSES_OV_REASON_ACTIVATE_WORKSPACE) {
        g_assert(priv->selected_actor == NULL);

        MetaScreen* screen = meta_plugin_get_screen(priv->plugin);
        target = overview_head_get_actor_for_workspace(priv->ov_head, priv->selected_workspace);

        MetaRectangle geom;
        int focused_monitor = meta_screen_get_current_monitor(screen);
        meta_screen_get_monitor_geometry(screen, focused_monitor, &geom);
        x = geom.x, y = geom.y, w = geom.width, h = geom.height;
    }

    if (target) {
        clutter_actor_remove_all_transitions(target);
        clutter_actor_set_child_above_sibling(clutter_actor_get_parent(target), target, NULL);

        clutter_actor_save_easing_state(target);
        clutter_actor_set_easing_mode(target, CLUTTER_LINEAR);
        clutter_actor_set_easing_duration(target, 150);

        clutter_actor_set_position(target, x, y);
        clutter_actor_set_scale(target, w / clutter_actor_get_width(target),
                h / clutter_actor_get_height(target));
        clutter_actor_restore_easing_state(target);
        g_object_connect(target, "signal::transitions-completed",
                G_CALLBACK(on_restore_position_effect_complete), self, NULL);
    }
}

static void on_ov_workspace_activated(OverviewHead* ovhead, MetaWorkspace* ws, MosesOverview* self)
{
    g_assert(self->priv->ov_head == ovhead);
    self->priv->selected_workspace = ws;
    overview_animated_destroy(self, MOSES_OV_REASON_ACTIVATE_WORKSPACE, TRUE);
}

static gboolean on_bg_button_press(ClutterActor *actor, ClutterEvent *event, MosesOverview* self)
{
    MosesOverviewPrivate* priv = self->priv;
    g_debug("%s", __func__);

    if (priv->ready) {
        overview_animated_destroy(self, MOSES_OV_REASON_NORMAL, FALSE);
    }
    return TRUE;
}

static gboolean on_key_release(ClutterActor *actor, ClutterEvent *event, MosesOverview* self)
{
    MosesOverviewPrivate* priv = self->priv;
    MetaScreen* screen = meta_plugin_get_screen(priv->plugin);
    MetaDisplay* display = meta_screen_get_display(screen);
    ClutterModifierType state = clutter_event_get_state(event);
    g_debug("%s", __func__);

    if (priv->ready) {
        guint action = meta_display_get_keybinding_action(display, clutter_event_get_key_code(event), state);
        if (action == elsa_wm_get_action(ELSA_WM_PLUGIN(priv->plugin), "expose-windows")) {
            overview_animated_destroy(self, MOSES_OV_REASON_NORMAL, FALSE);
            return TRUE;
        }
    }

    return FALSE;
}

static gboolean on_key_press(ClutterActor *actor, ClutterEvent *event, MosesOverview* self)
{
    MosesOverviewPrivate* priv = self->priv;
    MetaScreen* screen = meta_plugin_get_screen(priv->plugin);
    g_debug("%s", __func__);

    guint keysym = clutter_event_get_key_symbol(event);

    if (priv->ready) {
        if (keysym == CLUTTER_KEY_Escape) {
            overview_animated_destroy(self, MOSES_OV_REASON_NORMAL, FALSE);
            return TRUE;

        } else if (keysym >= CLUTTER_KEY_1 && keysym <= CLUTTER_KEY_9) {
            //TODO: what if size of clones is greater than 9
            for (int i = 0; i < priv->clones->len; i++) {
                ClutterActor* clone = g_ptr_array_index(priv->clones, i);
                int order = GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(clone), moses_overview_window_clone_order()));
                if (order == keysym - CLUTTER_KEY_0) {
                    priv->selected_actor = clone;
                    overview_animated_destroy(self, MOSES_OV_REASON_ACTIVATE_WINDOW, TRUE);
                }
            }
        }
    }

    return FALSE;
}

static const int GAPS = 10;
static const int MAX_TRANSLATIONS = 100000;
static const int ACCURACY = 20;
static const int BORDER = 10;
static int TOP_GAP = 10; // adjusted by overview head
static const int BOTTOM_GAP = 20;

//some math utilities
static gboolean rect_is_overlapping_any(MetaRectangle rect, MetaRectangle* rects, gint n, MetaRectangle border)
{
    if (!meta_rectangle_contains_rect(&border, &rect))
        return TRUE;

    for (int i = 0; i < n; i++) {
        if (meta_rectangle_equal(&rects[i], &rect))
            continue;

        if (meta_rectangle_overlap(&rects[i], &rect))
            return TRUE;
    }

    return FALSE;
}

static MetaRectangle rect_adjusted(MetaRectangle rect, int dx1, int dy1, int dx2, int dy2)
{
    return (MetaRectangle){rect.x + dx1, rect.y + dy1, rect.width + (-dx1 + dx2), rect.height + (-dy1 + dy2)};
}

static GdkPoint rect_center(MetaRectangle rect)
{
    return (GdkPoint){rect.x + rect.width / 2, rect.y + rect.height / 2};
}

static gboolean on_thumb_button_press(ClutterActor *actor, ClutterEvent *event, MosesOverview* self)
{
    MosesOverviewPrivate* priv = self->priv;
    g_debug("%s", __func__);

    if (priv->ready) {
        priv->selected_actor = actor;
        overview_animated_destroy(self, MOSES_OV_REASON_ACTIVATE_WINDOW, TRUE);
    }
    return TRUE;
}

static gboolean on_thumb_leave(ClutterActor *actor, ClutterEvent *event, MosesOverview* self)
{
    MosesOverviewPrivate* priv = self->priv;
    g_debug("%s", __func__);

    clutter_actor_remove_effect_by_name(actor, "turn");
    return FALSE;
}

static gboolean on_thumb_enter(ClutterActor *actor, ClutterEvent *event, MosesOverview* self)
{
    MosesOverviewPrivate* priv = self->priv;
    g_debug("%s", __func__);

    ClutterColor clr = {0x20, 0x20, 0x20, 0xe0};
    ClutterEffect* turn = clutter_brightness_contrast_effect_new();
    clutter_brightness_contrast_effect_set_brightness(CLUTTER_BRIGHTNESS_CONTRAST_EFFECT(turn), 0.3);
    clutter_actor_add_effect_with_name(actor, "turn", turn);
    return FALSE;
}

static void place_window(MosesOverview* self, ClutterActor* actor, MetaRectangle rect)
{
    g_debug("%s: %d,%d,%d,%d", __func__, rect.x, rect.y, rect.width, rect.height);
    float fscale = rect.width / clutter_actor_get_width(actor);

    clutter_actor_save_easing_state(actor);
    clutter_actor_set_easing_mode(actor, CLUTTER_EASE_OUT_CUBIC);
    clutter_actor_set_easing_duration(actor, PLACEMENT_ANIMATION_DURATION);

    clutter_actor_set_scale(actor, fscale, fscale);
    clutter_actor_set_position(actor, rect.x, rect.y);
    /*clutter_actor_set_size(actor, rect.width, rect.height);*/
    clutter_actor_restore_easing_state(actor);

}

static void natural_placement (MosesOverview* self, MetaRectangle area)
{
    g_debug("%s: geom: %d,%d,%d,%d", __func__, area.x, area.y, area.width, area.height);
    MosesOverviewPrivate* priv = self->priv;
    GPtrArray* clones = priv->clones;

    MetaRectangle bounds = {area.x, area.y, area.width, area.height};

    int direction = 0;
    int* directions = g_malloc(sizeof(int)*clones->len);
    MetaRectangle* rects = g_malloc(sizeof(MetaRectangle)*clones->len);

    for (int i = 0; i < clones->len; i++) {
        // save rectangles into 4-dimensional arrays representing two corners of the rectangular: [left_x, top_y, right_x, bottom_y]
        MetaRectangle rect;
        ClutterActor* clone = g_ptr_array_index(clones, i);
        MetaWindowActor* actor = META_WINDOW_ACTOR(clutter_clone_get_source(CLUTTER_CLONE(clone)));
        MetaWindow* win = meta_window_actor_get_meta_window(actor);

        meta_window_get_frame_rect(win, &rect);
        rect = rect_adjusted(rect, -GAPS, -GAPS, GAPS, GAPS);
        rects[i] = rect;
        g_debug("%s: frame: %d,%d,%d,%d", __func__, rect.x, rect.y, rect.width, rect.height);

        meta_rectangle_union(&bounds, &rect, &bounds);

        // This is used when the window is on the edge of the screen to try to use as much screen real estate as possible.
        directions[i] = direction;
        direction++;
        if (direction == 4)
            direction = 0;
    }

    int loop_counter = 0;
    gboolean overlap = FALSE;
    do {
        overlap = FALSE;
        for (int i = 0; i < clones->len; i++) {
            for (int j = 0; j < clones->len; j++) {
                if (i == j)
                    continue;

                MetaRectangle rect = rects[i];
                MetaRectangle comp = rects[j];

                if (!meta_rectangle_overlap(&rect, &comp))
                    continue;

                loop_counter ++;
                overlap = TRUE;

                // Determine pushing direction
                GdkPoint i_center = rect_center (rect);
                GdkPoint j_center = rect_center (comp);
                GdkPoint diff = {j_center.x - i_center.x, j_center.y - i_center.y};

                // Prevent dividing by zero and non-movement
                if (diff.x == 0 && diff.y == 0)
                    diff.x = 1;

                // Approximate a vector of between 10px and 20px in magnitude in the same direction
                float length = sqrtf (diff.x * diff.x + diff.y * diff.y);
                diff.x = (int)floorf (diff.x * ACCURACY / length);
                diff.y = (int)floorf (diff.y * ACCURACY / length);
                // Move both windows apart
                rect.x += -diff.x;
                rect.y += -diff.y;
                comp.x += diff.x;
                comp.y += diff.y;

                // Try to keep the bounding rect the same aspect as the screen so that more
                // screen real estate is utilised. We do this by splitting the screen into nine
                // equal sections, if the window center is in any of the corner sections pull the
                // window towards the outer corner. If it is in any of the other edge sections
                // alternate between each corner on that edge. We don't want to determine it
                // randomly as it will not produce consistant locations when using the filter.
                // Only move one window so we don't cause large amounts of unnecessary zooming
                // in some situations. We need to do this even when expanding later just in case
                // all windows are the same size.
                // (We are using an old bounding rect for this, hopefully it doesn't matter)
                int x_section = (int)roundf ((rect.x - bounds.x) / (bounds.width / 3.0f));
                int y_section = (int)roundf ((comp.y - bounds.y) / (bounds.height / 3.0f));

                i_center = rect_center (rect);
                diff.x = 0;
                diff.y = 0;
                if (x_section != 1 || y_section != 1) { // Remove this if you want the center to pull as well
                    if (x_section == 1)
                        x_section = (directions[i] / 2 == 1 ? 2 : 0);
                    if (y_section == 1)
                        y_section = (directions[i] % 2 == 1 ? 2 : 0);
                }
                if (x_section == 0 && y_section == 0) {
                    diff.x = bounds.x - i_center.x;
                    diff.y = bounds.y - i_center.y;
                }
                if (x_section == 2 && y_section == 0) {
                    diff.x = bounds.x + bounds.width - i_center.x;
                    diff.y = bounds.y - i_center.y;
                }
                if (x_section == 2 && y_section == 2) {
                    diff.x = bounds.x + bounds.width - i_center.x;
                    diff.y = bounds.y + bounds.height - i_center.y;
                }
                if (x_section == 0 && y_section == 2) {
                    diff.x = bounds.x - i_center.x;
                    diff.y = bounds.y + bounds.height - i_center.y;
                }
                if (diff.x != 0 || diff.y != 0) {
                    length = sqrtf (diff.x * diff.x + diff.y * diff.y);
                    diff.x *= (int)floorf (ACCURACY / length / 2.0f);
                    diff.y *= (int)floorf (ACCURACY / length / 2.0f);
                    rect.x += diff.x;
                    rect.y += diff.y;
                }

                // Update bounding rect
                meta_rectangle_union(&bounds, &rect, &bounds);
                meta_rectangle_union(&bounds, &comp, &bounds);

                //we took copies from the rects from our list so we need to reassign them
                rects[i] = rect;
                rects[j] = comp;
            }
        }
    } while (overlap && loop_counter < MAX_TRANSLATIONS);

    // Work out scaling by getting the most top-left and most bottom-right window coords.
    float scale = fminf (fminf (area.width / (float)bounds.width, area.height / (float)bounds.height), 1.0f);

    // Make bounding rect fill the screen size for later steps
    bounds.x = (int)floorf (bounds.x - (area.width - bounds.width * scale) / 2);
    bounds.y = (int)floorf (bounds.y - (area.height - bounds.height * scale) / 2);
    bounds.width = (int)floorf (area.width / scale);
    bounds.height = (int)floorf (area.height / scale);

    // Move all windows back onto the screen and set their scale
    int index = 0;
    for (; index < clones->len; index++) {
        MetaRectangle rect = rects[index];
        rects[index] = (MetaRectangle){
            (int)floorf ((rect.x - bounds.x) * scale + area.x),
            (int)floorf ((rect.y - bounds.y) * scale + area.y),
            (int)floorf (rect.width * scale),
            (int)floorf (rect.height * scale)
        };
    }

    // fill gaps by enlarging windows
    gboolean moved = FALSE;
    MetaRectangle border = area;
    do {
        moved = FALSE;

        index = 0;
        for (; index < clones->len; index++) {
            MetaRectangle rect = rects[index];

            int width_diff = ACCURACY;
            int height_diff = (int)floorf ((((rect.width + width_diff) - rect.height) /
                        (float)rect.width) * rect.height);
            int x_diff = width_diff / 2;
            int y_diff = height_diff / 2;

            //top right
            MetaRectangle old = rect;
            rect = (MetaRectangle){ rect.x + x_diff, rect.y - y_diff - height_diff, rect.width + width_diff, rect.height + width_diff };
            if (rect_is_overlapping_any (rect, rects, clones->len, border))
                rect = old;
            else moved = TRUE;

            //bottom right
            old = rect;
            rect = (MetaRectangle){rect.x + x_diff, rect.y + y_diff, rect.width + width_diff, rect.height + width_diff};
            if (rect_is_overlapping_any (rect, rects, clones->len, border))
                rect = old;
            else moved = TRUE;

            //bottom left
            old = rect;
            rect = (MetaRectangle){rect.x - x_diff, rect.y + y_diff, rect.width + width_diff, rect.height + width_diff};
            if (rect_is_overlapping_any (rect, rects, clones->len, border))
                rect = old;
            else moved = TRUE;

            //top left
            old = rect;
            rect = (MetaRectangle){rect.x - x_diff, rect.y - y_diff - height_diff, rect.width + width_diff, rect.height + width_diff};
            if (rect_is_overlapping_any (rect, rects, clones->len, border))
                rect = old;
            else moved = TRUE;

            rects[index] = rect;
        }
    } while (moved);

    index = 0;
    for (; index < clones->len; index++) {
        MetaRectangle rect = rects[index];

        ClutterActor* clone = g_ptr_array_index(clones, index);
        MetaWindowActor* actor = META_WINDOW_ACTOR(clutter_clone_get_source(CLUTTER_CLONE(clone)));
        MetaWindow* window = meta_window_actor_get_meta_window(actor);

        MetaRectangle window_rect;
        meta_window_get_frame_rect(window, &window_rect);


        rect = rect_adjusted(rect, GAPS, GAPS, -GAPS, -GAPS);
        scale = rect.width / (float)window_rect.width;

        if (scale > 2.0 || (scale > 1.0 && (window_rect.width > 300 || window_rect.height > 300))) {
            scale = (window_rect.width > 300 || window_rect.height > 300) ? 1.0f : 2.0f;
            rect = (MetaRectangle){rect_center (rect).x - (int)floorf (window_rect.width * scale) / 2,
                rect_center (rect).y - (int)floorf (window_rect.height * scale) / 2,
                (int)floorf (window_rect.width * scale),
                (int)floorf (window_rect.height * scale)};
        }

        place_window(self, clone, rect);
    }

    g_free(directions);
    g_free(rects);
}

static gint window_compare(gconstpointer a, gconstpointer b)
{
    ClutterActor* aa = *(ClutterActor**)a;
    ClutterActor* bb = *(ClutterActor**)b;

    MetaWindowActor* a1 = META_WINDOW_ACTOR(clutter_clone_get_source(CLUTTER_CLONE(aa)));
    MetaWindowActor* b1 = META_WINDOW_ACTOR(clutter_clone_get_source(CLUTTER_CLONE(bb)));

    MetaWindow* w1 = meta_window_actor_get_meta_window(a1);
    MetaWindow* w2 = meta_window_actor_get_meta_window(b1);
    return meta_window_get_stable_sequence(w1) - meta_window_get_stable_sequence(w2);
}

static gboolean  on_ready_timeout(MosesOverview* self)
{
    self->priv->ready = TRUE;
    return G_SOURCE_REMOVE;
}

static void calculate_places(MosesOverview* self)
{
    MosesOverviewPrivate* priv = self->priv;
    GPtrArray* clones = priv->clones;
    if (priv->clones->len) {
        g_ptr_array_sort(clones, window_compare);

        // get the area used by the expo algorithms together
        MetaScreen* screen = meta_plugin_get_screen(priv->plugin);

        MetaRectangle geom;
        int focused_monitor = meta_screen_get_current_monitor(screen);
        meta_screen_get_monitor_geometry(screen, focused_monitor, &geom);

        int HEAD_SIZE = TOP_GAP;
        g_object_get(priv->ov_head, "height", &HEAD_SIZE, NULL);
        g_debug("%s ov height: %d", __func__, HEAD_SIZE);
        MetaRectangle area = {(int)floorf (geom.x + BORDER),
                              (int)floorf (geom.y + TOP_GAP + HEAD_SIZE),
                              (int)floorf (geom.width - BORDER * 2),
                              (int)floorf (geom.height - BOTTOM_GAP - TOP_GAP - HEAD_SIZE)};

        natural_placement(self, area);

    } else {
        //NOTE: I can not set ready flag here because a conflict of super-e key release
        g_timeout_add(500, (GSourceFunc)on_ready_timeout, self);
    }
    clutter_actor_show(overview_head_get_content(priv->ov_head));
}

static void moses_overview_setup(MosesOverview* self)
{
    MosesOverviewPrivate* priv = self->priv;
    MetaScreen* screen = meta_plugin_get_screen(priv->plugin);
    MetaWorkspace* ws = meta_screen_get_active_workspace(screen);

    prepare_workspace_content(self, ws);

    // setup ov head
    priv->ov_head = overview_head_new(self);
    ClutterActor* head = overview_head_get_content(priv->ov_head);
    clutter_actor_add_child(CLUTTER_ACTOR(self), head);
    clutter_actor_hide(head);

    g_signal_connect(priv->ov_head, "workspace-activated", G_CALLBACK(on_ov_workspace_activated), self);
}

static gboolean on_badge_draw(ClutterCanvas* canvas, cairo_t* cr,
        gint width, gint height, ClutterActor* badge)
{
    g_debug("%s: %d,%d, ", __func__, width, height);

    cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);
    cairo_paint_with_alpha(cr, 0.0);

    cairo_arc(cr, width/2, height/2, MIN(width, height)/2.0, 0, 2*M_PI);
    cairo_close_path(cr);

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_set_source_rgba(cr, 0.6, 0.2, 0.2, 0.8);
    cairo_fill(cr);

    char title[20];
    snprintf(title, 19, "%d", GPOINTER_TO_INT(g_object_get_qdata(G_OBJECT(badge), moses_overview_window_clone_order())));
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);

    cairo_select_font_face(cr, "fantasy", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 28.0);

    cairo_text_extents_t te;
    cairo_text_extents(cr, title, &te);
    cairo_move_to(cr, (width - te.width)/2.0 - te.x_bearing, (height - te.height)/2.0 - te.y_bearing);
    cairo_show_text(cr, title);

    return TRUE;
}

//FIXME: ClutterClone seems to have problem rendering children, so badges are stay in grandpar
static void create_window_badge(MosesOverview* self, ClutterActor* parent, int order)
{
    ClutterActor* badge = clutter_actor_new();
    clutter_actor_insert_child_above(clutter_actor_get_parent(parent), badge, NULL);

    gfloat tw, th;
    clutter_actor_get_transformed_size(parent, &tw, &th);

    gfloat w = 60.0, h = 60.0,
            x = (tw - w) / 2.0 + clutter_actor_get_x(parent),
            y = (th - h) / 2.0 + clutter_actor_get_y(parent);
    clutter_actor_set_position(badge, x, y);
    clutter_actor_set_size(badge, w, h);

    g_object_set_qdata(G_OBJECT(badge), moses_overview_window_clone_order(), GINT_TO_POINTER(order));
    g_object_set_qdata(G_OBJECT(parent), moses_overview_window_clone_order(), GINT_TO_POINTER(order));

    ClutterContent* canvas = clutter_canvas_new();
    clutter_canvas_set_size(CLUTTER_CANVAS(canvas), w, h);
    clutter_actor_set_content(badge, canvas);
    g_object_unref(canvas);
    g_signal_connect(canvas, "draw", G_CALLBACK(on_badge_draw), badge);

    clutter_content_invalidate(canvas);

    clutter_actor_set_scale(badge, 0.0, 0.0);

    //do animated show
    clutter_actor_save_easing_state(badge);
    clutter_actor_set_easing_mode(badge, CLUTTER_EASE_OUT_BACK);
    clutter_actor_set_easing_duration(badge, 350);

    clutter_actor_set_pivot_point(badge, 0.5, 0.5);
    clutter_actor_set_scale(badge, 1.0, 1.0);
    clutter_actor_restore_easing_state(badge);

    g_ptr_array_add(self->priv->badges, badge);
}

static void prepare_workspace_content(MosesOverview *self, MetaWorkspace *ws)
{
    MosesOverviewPrivate* priv = self->priv;
    GList* l = meta_workspace_list_windows(ws);
    if (!priv->clones) { priv->clones = g_ptr_array_new(); }

    while (l) {
        MetaWindow* win = l->data;
        MetaWindowActor* win_actor = META_WINDOW_ACTOR(meta_window_get_compositor_private(win));

        if (meta_window_get_window_type(win) == META_WINDOW_DESKTOP) {
            g_debug("%s: got desktop actor", __func__);
            priv->background_actor = clutter_clone_new(CLUTTER_ACTOR(win_actor));

        } else if (meta_window_get_window_type(win) == META_WINDOW_NORMAL &&
                !meta_window_is_hidden(win)) {
            ClutterActor* clone = clutter_clone_new(CLUTTER_ACTOR(win_actor));
            clutter_actor_set_reactive(clone, TRUE);

            float x = 0.0, y = 0.0;
            clutter_actor_get_position(CLUTTER_ACTOR(win_actor), &x, &y);
            clutter_actor_set_position(clone, x, y);

            clutter_actor_hide(CLUTTER_ACTOR(win_actor));

            g_ptr_array_add(priv->clones, clone);
            clutter_actor_add_child(CLUTTER_ACTOR(self), clone);

            g_object_connect(clone,
                    "signal::transitions-completed", G_CALLBACK(on_effect_complete), self,
                    "signal::button-press-event", on_thumb_button_press, self,
                    "signal::enter-event", on_thumb_enter, self,
                    "signal::leave-event", on_thumb_leave, self,
                    NULL);
        }

        l = l->next;
    }

    ClutterColor clr = CLUTTER_COLOR_INIT(0xff, 0xff, 0xff, 0xff);
    clutter_actor_set_background_color(CLUTTER_ACTOR(self), &clr);

    if (priv->background_actor) {
#if 0
        ClutterEffect* blur = moses_blur_effect_new();
        clutter_actor_add_effect_with_name(priv->background_actor, "blur", blur);
        clutter_actor_insert_child_below(CLUTTER_ACTOR(self), priv->background_actor, NULL);
        clutter_actor_hide(clutter_clone_get_source(CLUTTER_CLONE(priv->background_actor)));
        clutter_actor_set_reactive(priv->background_actor, TRUE);
#endif
    }

    g_object_connect(priv->background_actor ? priv->background_actor: CLUTTER_ACTOR(self),
            "signal::button-press-event", on_bg_button_press, self,
            NULL);
}

static gboolean on_idle(MosesOverview* self)
{
    calculate_places(self);

    clutter_actor_set_reactive(CLUTTER_ACTOR(self), TRUE);
    g_object_connect(self,
            "signal::key-press-event", on_key_press, self,
            "signal::key-release-event", on_key_release, self,
            NULL);

    return FALSE;
}

void moses_overview_show(MosesOverview* self, gboolean all_windows)
{
    MosesOverviewPrivate* priv = self->priv;

    MetaRectangle geom;
    MetaScreen* screen = meta_plugin_get_screen(priv->plugin);
    int focused_monitor = meta_screen_get_current_monitor(screen);
    meta_screen_get_monitor_geometry(screen, focused_monitor, &geom);

    // FIXME: overview is as big as the current monitor,
    // need to take care multiple monitors
    ClutterActor* stage = meta_get_stage_for_screen(screen);
    ClutterActor* top = CLUTTER_ACTOR(self);
    clutter_actor_set_size(top, geom.width, geom.height);
    clutter_actor_insert_child_above(stage, top, NULL);

    moses_overview_setup(self);
    priv->previous_focused = clutter_stage_get_key_focus(CLUTTER_STAGE(stage));

    if (!meta_plugin_begin_modal(priv->plugin, 0, clutter_get_current_event_time())) {
        g_warning("can not be modal");
        goto _end;
    }

    meta_disable_unredirect_for_screen(screen);

    clutter_actor_show(top);
    clutter_stage_set_key_focus(CLUTTER_STAGE(stage), top);
    clutter_actor_grab_key_focus(top);

    priv->modaled = TRUE;
    g_idle_add((GSourceFunc)on_idle, self);

    return;

_end:
    clutter_actor_destroy(CLUTTER_ACTOR(self));
}

MetaPlugin* overview_get_plugin(MosesOverview* self)
{
    return self->priv->plugin;
}
// }}}
