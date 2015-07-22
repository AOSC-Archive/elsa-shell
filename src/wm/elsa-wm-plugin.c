/*
 * This file inherit from mutter/src/compositor/plugins/default.c
 *
 * Author: Tomas Frydrych <tf@linux.intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "elsa-wm-plugin.h"
#include "background.h"
#include "switcher.h"
#include "overview.h"

#include <meta/window.h>
#include <meta/meta-background-group.h>
#include <meta/meta-background-actor.h>
#include <meta/util.h>
#include <glib/gi18n-lib.h>

#include <clutter/clutter.h>
#include <gmodule.h>
#include <string.h>

#define DESTROY_TIMEOUT   100
#define MINIMIZE_TIMEOUT  250
#define MAP_TIMEOUT       250
#define SWITCH_TIMEOUT    500

#define ACTOR_DATA_KEY "MCCP-Default-actor-data"
#define SCREEN_TILE_PREVIEW_DATA_KEY "MCCP-Default-screen-tile-preview-data"

static ElsaWmPlugin *m_plugin = NULL;
static GQuark actor_data_quark = 0;
static GQuark screen_tile_preview_data_quark = 0;

static void start      (MetaPlugin      *plugin);
static void minimize   (MetaPlugin      *plugin,
                        MetaWindowActor *actor);
static void map        (MetaPlugin      *plugin,
                        MetaWindowActor *actor);
static void destroy    (MetaPlugin      *plugin,
                        MetaWindowActor *actor);

static void switch_workspace (MetaPlugin          *plugin,
                              gint                 from,
                              gint                 to,
                              MetaMotionDirection  direction);

static void kill_window_effects   (MetaPlugin      *plugin,
                                   MetaWindowActor *actor);
static void kill_switch_workspace (MetaPlugin      *plugin);

static void show_tile_preview (MetaPlugin      *plugin,
                               MetaWindow      *window,
                               MetaRectangle   *tile_rect,
                               int              tile_monitor_number);
static void hide_tile_preview (MetaPlugin      *plugin);

static void confirm_display_change (MetaPlugin *plugin);

static const MetaPluginInfo * plugin_info (MetaPlugin *plugin);

G_DEFINE_TYPE(ElsaWmPlugin, elsa_wm_plugin, META_TYPE_PLUGIN)

/*
 * Plugin private data that we store in the .plugin_private member.
 */
struct _ElsaWmPluginPrivate
{
    /* Valid only when switch_workspace effect is in progress */
    ClutterTimeline         *tml_switch_workspace1;
    ClutterTimeline         *tml_switch_workspace2;
    ClutterActor            *desktop1;
    ClutterActor            *desktop2;

    ClutterActor            *background_group;

    MetaPluginInfo          info;

    MetaSwitcher            *switcher;
    MosesOverview           *overview;

    GSettings               *settings;

    guint                   expose_windows_action;
    guint                   expose_all_windows_action;
};

/*
 * Per actor private data we attach to each actor.
 */
typedef struct _ActorPrivate
{
  ClutterActor *orig_parent;

  ClutterTimeline *tml_minimize;
  ClutterTimeline *tml_destroy;
  ClutterTimeline *tml_map;
} ActorPrivate;

/* callback data for when animations complete */
typedef struct
{
  ClutterActor *actor;
  MetaPlugin *plugin;
} EffectCompleteData;


typedef struct _ScreenTilePreview
{
  ClutterActor   *actor;

  GdkRGBA        *preview_color;

  MetaRectangle   tile_rect;
} ScreenTilePreview;

static void
elsa_wm_plugin_dispose (GObject *object)
{
  /* ElsaWmPluginPrivate *priv = ELSA_WM_PLUGIN (object)->priv;
  */
  G_OBJECT_CLASS (elsa_wm_plugin_parent_class)->dispose (object);
}

static void
elsa_wm_plugin_finalize (GObject *object)
{
    m_plugin = NULL;
    G_OBJECT_CLASS (elsa_wm_plugin_parent_class)->finalize (object);
}

static void
elsa_wm_plugin_set_property (GObject      *object,
			    guint         prop_id,
			    const GValue *value,
			    GParamSpec   *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
elsa_wm_plugin_get_property (GObject    *object,
			    guint       prop_id,
			    GValue     *value,
			    GParamSpec *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
elsa_wm_plugin_class_init (ElsaWmPluginClass *klass)
{
  GObjectClass      *gobject_class = G_OBJECT_CLASS (klass);
  MetaPluginClass *plugin_class  = META_PLUGIN_CLASS (klass);

  gobject_class->finalize        = elsa_wm_plugin_finalize;
  gobject_class->dispose         = elsa_wm_plugin_dispose;
  gobject_class->set_property    = elsa_wm_plugin_set_property;
  gobject_class->get_property    = elsa_wm_plugin_get_property;

  plugin_class->start            = start;
  plugin_class->map              = map;
  plugin_class->minimize         = minimize;
  plugin_class->destroy          = destroy;
  plugin_class->switch_workspace = switch_workspace;
  plugin_class->show_tile_preview = show_tile_preview;
  plugin_class->hide_tile_preview = hide_tile_preview;
  plugin_class->plugin_info      = plugin_info;
  plugin_class->kill_window_effects   = kill_window_effects;
  plugin_class->kill_switch_workspace = kill_switch_workspace;
  plugin_class->confirm_display_change = confirm_display_change;

  g_type_class_add_private (gobject_class, sizeof (ElsaWmPluginPrivate));
}

static void
elsa_wm_plugin_init (ElsaWmPlugin *self)
{
    ElsaWmPluginPrivate *priv;

    if (!m_plugin)
        m_plugin = self;

    self->priv = priv = ELSA_WM_PLUGIN_GET_PRIVATE (self);

    priv->info.name        = "Elsa Window Manager";
    priv->info.version     = PACKAGE_VERSION;
    priv->info.author      = "Leslie Zhai";
    priv->info.license     = "GPL";
    priv->info.description = "This is a skeleton of a meta plugin implementation.";

    priv->settings = g_settings_new("org.anthonos.elsa-shell.wm");
}

/*
 * Actor private data accessor
 */
static void
free_actor_private (gpointer data)
{
  if (G_LIKELY (data != NULL))
    g_slice_free (ActorPrivate, data);
}

static ActorPrivate *
get_actor_private (MetaWindowActor *actor)
{
  ActorPrivate *priv = g_object_get_qdata (G_OBJECT (actor), actor_data_quark);

  if (G_UNLIKELY (actor_data_quark == 0))
    actor_data_quark = g_quark_from_static_string (ACTOR_DATA_KEY);

  if (G_UNLIKELY (!priv))
    {
      priv = g_slice_new0 (ActorPrivate);

      g_object_set_qdata_full (G_OBJECT (actor),
                               actor_data_quark, priv,
                               free_actor_private);
    }

  return priv;
}

static void
on_switch_workspace_effect_complete (ClutterTimeline *timeline, gpointer data)
{
  MetaPlugin               *plugin  = META_PLUGIN (data);
  ElsaWmPluginPrivate *priv = ELSA_WM_PLUGIN (plugin)->priv;
  MetaScreen *screen = meta_plugin_get_screen (plugin);
  GList *l = meta_get_window_actors (screen);

  while (l)
    {
      ClutterActor *a = l->data;
      MetaWindowActor *window_actor = META_WINDOW_ACTOR (a);
      ActorPrivate *apriv = get_actor_private (window_actor);

      if (apriv->orig_parent)
        {
          clutter_actor_reparent (a, apriv->orig_parent);
          apriv->orig_parent = NULL;
        }

      l = l->next;
    }

  clutter_actor_destroy (priv->desktop1);
  clutter_actor_destroy (priv->desktop2);

  priv->tml_switch_workspace1 = NULL;
  priv->tml_switch_workspace2 = NULL;
  priv->desktop1 = NULL;
  priv->desktop2 = NULL;

  meta_plugin_switch_workspace_completed (plugin);
}

static void
on_monitors_changed (MetaScreen *screen,
                     MetaPlugin *plugin)
{
  ElsaWmPlugin *self = ELSA_WM_PLUGIN (plugin);
  int i, n;
  clutter_actor_destroy_all_children (self->priv->background_group);

  n = meta_screen_get_n_monitors (screen);
  for (i = 0; i < n; i++)
    {
      ClutterActor *bg = budgie_background_new(screen, i);
      clutter_actor_add_child(self->priv->background_group, bg);
      clutter_actor_show(bg);
    }
}

static void switcher_destroy_cb(MetaSwitcher* switcher, ElsaWmPlugin* self) 
{
    if (self->priv->switcher)
        self->priv->switcher = NULL;
}

static void switch_applications_cb(MetaDisplay *display, 
                                   MetaScreen *screen, 
                                   MetaWindow *event_window, 
                                   ClutterKeyEvent *event, 
                                   MetaKeyBinding *binding, 
                                   gpointer user_data) 
{
    ElsaWmPlugin *self = ELSA_WM_PLUGIN(user_data);

    if (self->priv->switcher)
        return;

    self->priv->switcher = meta_switcher_new(META_PLUGIN(self));
    g_signal_connect(G_OBJECT(self->priv->switcher), "destroy", 
            G_CALLBACK(switcher_destroy_cb), self);

    if (!meta_switcher_show(self->priv->switcher)) {
        g_object_unref(self->priv->switcher);
        self->priv->switcher = NULL;
    }
}

guint elsa_wm_get_action(ElsaWmPlugin *self, const char *name)
{
    g_return_val_if_fail(ELSA_IS_WM_PLUGIN(self), 0);

    if (g_strcmp0(name, "expose-windows") == 0)
        return self->priv->expose_windows_action;
    else if (g_strcmp0(name, "expose-all-windows") == 0)
        return self->priv->expose_all_windows_action;

    return 0;
}

static void _on_overview_destroy(MosesOverview *overview, ElsaWmPlugin *plugin) 
{
    if (plugin->priv->overview) {
        plugin->priv->overview = NULL;
    }
}

static void do_expose_windows(ElsaWmPlugin *self, gboolean is_expose_all)      
{                                                                               
    if (self->priv->overview) 
        return;

    self->priv->overview = moses_overview_new(META_PLUGIN(self));
    g_signal_connect(G_OBJECT(self->priv->overview), "destroy",
            G_CALLBACK(_on_overview_destroy), self);
    moses_overview_show(self->priv->overview, is_expose_all);
}                                                                               
                                                                                
static void expose_window_handler(MetaDisplay *display, 
                                  MetaScreen *screen,
                                  MetaWindow *window,
                                  ClutterKeyEvent *event,
                                  MetaKeyBinding *binding,
                                  gpointer user_data)
{
    ElsaWmPlugin* self = ELSA_WM_PLUGIN(user_data);
    g_debug("%s: mask %x", __func__, event->modifier_state);

    ClutterModifierType state = clutter_event_get_state((ClutterEvent*)event);
    guint action = meta_display_get_keybinding_action(display,
            clutter_event_get_key_code((ClutterEvent*)event), state);

    do_expose_windows(self, action == self->priv->expose_all_windows_action);
}

static void
start(MetaPlugin *plugin)
{
    ElsaWmPlugin *self = ELSA_WM_PLUGIN(plugin);
    MetaScreen *screen = meta_plugin_get_screen(plugin);
    MetaDisplay *display = meta_screen_get_display(screen);

    self->priv->background_group = meta_background_group_new();
    clutter_actor_insert_child_below(meta_get_window_group_for_screen(screen),
                                     self->priv->background_group, NULL);

    g_signal_connect(screen, "monitors-changed",
                     G_CALLBACK(on_monitors_changed), plugin);
    on_monitors_changed(screen, plugin);

    /*
     * switcher and expose inherit from Sian Cao`s moses-wm ;-)
     */
    meta_keybindings_set_custom_handler("switch-applications", 
        switch_applications_cb, self, NULL);
    meta_keybindings_set_custom_handler("switch-applications-backward",
        switch_applications_cb, self, NULL);

    self->priv->expose_windows_action = meta_display_add_keybinding(display,
        "expose-windows", self->priv->settings, 0, expose_window_handler, self, NULL);
    self->priv->expose_all_windows_action = meta_display_add_keybinding(display,
        "expose-all-windows", self->priv->settings, 0, expose_window_handler, self, NULL);
    if (self->priv->expose_windows_action == META_KEY_BINDING_NONE)
        g_warning("%s, line %d: register expose-windows failed", __func__, __LINE__);

    clutter_actor_show(meta_get_stage_for_screen(screen));
}

static void
switch_workspace (MetaPlugin *plugin,
                  gint from, gint to,
                  MetaMotionDirection direction)
{
  MetaScreen *screen;
  ElsaWmPluginPrivate *priv = ELSA_WM_PLUGIN (plugin)->priv;
  GList        *l;
  ClutterActor *workspace0  = clutter_group_new ();
  ClutterActor *workspace1  = clutter_group_new ();
  ClutterActor *stage;
  int           screen_width, screen_height;
  ClutterAnimation *animation;

  screen = meta_plugin_get_screen (plugin);
  stage = meta_get_stage_for_screen (screen);

  meta_screen_get_size (screen,
                        &screen_width,
                        &screen_height);

  clutter_actor_set_anchor_point (workspace1,
                                  screen_width,
                                  screen_height);
  clutter_actor_set_position (workspace1,
                              screen_width,
                              screen_height);

  clutter_actor_set_scale (workspace1, 0.0, 0.0);

  clutter_container_add_actor (CLUTTER_CONTAINER (stage), workspace1);
  clutter_container_add_actor (CLUTTER_CONTAINER (stage), workspace0);

  if (from == to)
    {
      meta_plugin_switch_workspace_completed (plugin);
      return;
    }

  l = g_list_last (meta_get_window_actors (screen));

  while (l)
    {
      MetaWindowActor *window_actor = l->data;
      ActorPrivate    *apriv	    = get_actor_private (window_actor);
      ClutterActor    *actor	    = CLUTTER_ACTOR (window_actor);
      MetaWorkspace   *workspace;
      gint             win_workspace;

      workspace = meta_window_get_workspace (meta_window_actor_get_meta_window (window_actor));
      win_workspace = meta_workspace_index (workspace);

      if (win_workspace == to || win_workspace == from)
        {
          apriv->orig_parent = clutter_actor_get_parent (actor);

          clutter_actor_reparent (actor,
				  win_workspace == to ? workspace1 : workspace0);
          clutter_actor_show_all (actor);
          clutter_actor_raise_top (actor);
        }
      else if (win_workspace < 0)
        {
          /* Sticky window */
          apriv->orig_parent = NULL;
        }
      else
        {
          /* Window on some other desktop */
          clutter_actor_hide (actor);
          apriv->orig_parent = NULL;
        }

      l = l->prev;
    }

  priv->desktop1 = workspace0;
  priv->desktop2 = workspace1;

  animation = clutter_actor_animate (workspace0, CLUTTER_EASE_IN_SINE,
                                     SWITCH_TIMEOUT,
                                     "scale-x", 1.0,
                                     "scale-y", 1.0,
                                     NULL);
  priv->tml_switch_workspace1 = clutter_animation_get_timeline (animation);
  g_signal_connect (priv->tml_switch_workspace1,
                    "completed",
                    G_CALLBACK (on_switch_workspace_effect_complete),
                    plugin);

  animation = clutter_actor_animate (workspace1, CLUTTER_EASE_IN_SINE,
                                     SWITCH_TIMEOUT,
                                     "scale-x", 0.0,
                                     "scale-y", 0.0,
                                     NULL);
  priv->tml_switch_workspace2 = clutter_animation_get_timeline (animation);
}


/*
 * Minimize effect completion callback; this function restores actor state, and
 * calls the manager callback function.
 */
static void
on_minimize_effect_complete (ClutterTimeline *timeline, EffectCompleteData *data)
{
  /*
   * Must reverse the effect of the effect; must hide it first to ensure
   * that the restoration will not be visible.
   */
  MetaPlugin *plugin = data->plugin;
  ActorPrivate *apriv;
  MetaWindowActor *window_actor = META_WINDOW_ACTOR (data->actor);

  apriv = get_actor_private (META_WINDOW_ACTOR (data->actor));
  apriv->tml_minimize = NULL;

  clutter_actor_hide (data->actor);

  /* FIXME - we shouldn't assume the original scale, it should be saved
   * at the start of the effect */
  clutter_actor_set_scale (data->actor, 1.0, 1.0);

  /* Now notify the manager that we are done with this effect */
  meta_plugin_minimize_completed (plugin, window_actor);

  g_free (data);
}

/*
 * Simple minimize handler: it applies a scale effect (which must be reversed on
 * completion).
 */
static void
minimize (MetaPlugin *plugin, MetaWindowActor *window_actor)
{
  MetaWindowType type;
  MetaRectangle icon_geometry;
  MetaWindow *meta_window = meta_window_actor_get_meta_window (window_actor);
  ClutterActor *actor  = CLUTTER_ACTOR (window_actor);


  type = meta_window_get_window_type (meta_window);

  if (!meta_window_get_icon_geometry(meta_window, &icon_geometry))
    {
      icon_geometry.x = 0;
      icon_geometry.y = 0;
    }

  if (type == META_WINDOW_NORMAL)
    {
      ClutterAnimation *animation;
      EffectCompleteData *data = g_new0 (EffectCompleteData, 1);
      ActorPrivate *apriv = get_actor_private (window_actor);

      animation = clutter_actor_animate (actor,
                                         CLUTTER_EASE_IN_SINE,
                                         MINIMIZE_TIMEOUT,
                                         "scale-x", 0.0,
                                         "scale-y", 0.0,
                                         "x", (double)icon_geometry.x,
                                         "y", (double)icon_geometry.y,
                                         NULL);
      apriv->tml_minimize = clutter_animation_get_timeline (animation);
      data->plugin = plugin;
      data->actor = actor;
      g_signal_connect (apriv->tml_minimize, "completed",
                        G_CALLBACK (on_minimize_effect_complete),
                        data);

    }
  else
    meta_plugin_minimize_completed (plugin, window_actor);
}

static void
on_map_effect_complete (ClutterTimeline *timeline, EffectCompleteData *data)
{
  /*
   * Must reverse the effect of the effect.
   */
  MetaPlugin *plugin = data->plugin;
  MetaWindowActor  *window_actor = META_WINDOW_ACTOR (data->actor);
  ActorPrivate  *apriv = get_actor_private (window_actor);

  apriv->tml_map = NULL;

  /* Now notify the manager that we are done with this effect */
  meta_plugin_map_completed (plugin, window_actor);

  g_free (data);
}

/*
 * Simple map handler: it applies a scale effect which must be reversed on
 * completion).
 */
static void
map (MetaPlugin *plugin, MetaWindowActor *window_actor)
{
  MetaWindowType type;
  ClutterActor *actor = CLUTTER_ACTOR (window_actor);
  MetaWindow *meta_window = meta_window_actor_get_meta_window (window_actor);

  type = meta_window_get_window_type (meta_window);

  if (type == META_WINDOW_NORMAL)
    {
      ClutterAnimation *animation;
      EffectCompleteData *data = g_new0 (EffectCompleteData, 1);
      ActorPrivate *apriv = get_actor_private (window_actor);

      clutter_actor_set_pivot_point (actor, 0.5, 0.5);
      clutter_actor_set_opacity (actor, 0);
      clutter_actor_set_scale (actor, 0.5, 0.5);
      clutter_actor_show (actor);

      animation = clutter_actor_animate (actor,
                                         CLUTTER_EASE_OUT_QUAD,
                                         MAP_TIMEOUT,
                                         "opacity", 255,
                                         "scale-x", 1.0,
                                         "scale-y", 1.0,
                                         NULL);
      apriv->tml_map = clutter_animation_get_timeline (animation);
      data->actor = actor;
      data->plugin = plugin;
      g_signal_connect (apriv->tml_map, "completed",
                        G_CALLBACK (on_map_effect_complete),
                        data);
    }
  else
    meta_plugin_map_completed (plugin, window_actor);
}

/*
 * Destroy effect completion callback; this is a simple effect that requires no
 * further action than notifying the manager that the effect is completed.
 */
static void
on_destroy_effect_complete (ClutterTimeline *timeline, EffectCompleteData *data)
{
  MetaPlugin *plugin = data->plugin;
  MetaWindowActor *window_actor = META_WINDOW_ACTOR (data->actor);
  ActorPrivate *apriv = get_actor_private (window_actor);

  apriv->tml_destroy = NULL;

  meta_plugin_destroy_completed (plugin, window_actor);
}

/*
 * Simple TV-out like effect.
 */
static void
destroy (MetaPlugin *plugin, MetaWindowActor *window_actor)
{
  MetaWindowType type;
  ClutterActor *actor = CLUTTER_ACTOR (window_actor);
  MetaWindow *meta_window = meta_window_actor_get_meta_window (window_actor);

  type = meta_window_get_window_type (meta_window);

  if (type == META_WINDOW_NORMAL)
    {
      ClutterAnimation *animation;
      EffectCompleteData *data = g_new0 (EffectCompleteData, 1);
      ActorPrivate *apriv = get_actor_private (window_actor);

      animation = clutter_actor_animate (actor,
                                         CLUTTER_EASE_OUT_QUAD,
                                         DESTROY_TIMEOUT,
                                         "opacity", 0,
                                         "scale-x", 0.8,
                                         "scale-y", 0.8,
                                         NULL);
      apriv->tml_destroy = clutter_animation_get_timeline (animation);
      data->plugin = plugin;
      data->actor = actor;
      g_signal_connect (apriv->tml_destroy, "completed",
                        G_CALLBACK (on_destroy_effect_complete),
                        data);
    }
  else
    meta_plugin_destroy_completed (plugin, window_actor);
}

/*
 * Tile preview private data accessor
 */
static void
free_screen_tile_preview (gpointer data)
{
  ScreenTilePreview *preview = data;

  if (G_LIKELY (preview != NULL)) {
    clutter_actor_destroy (preview->actor);
    g_slice_free (ScreenTilePreview, preview);
  }
}

static ScreenTilePreview *
get_screen_tile_preview (MetaScreen *screen)
{
  ScreenTilePreview *preview = g_object_get_qdata (G_OBJECT (screen), screen_tile_preview_data_quark);

  if (G_UNLIKELY (screen_tile_preview_data_quark == 0))
    screen_tile_preview_data_quark = g_quark_from_static_string (SCREEN_TILE_PREVIEW_DATA_KEY);

  if (G_UNLIKELY (!preview))
    {
      preview = g_slice_new0 (ScreenTilePreview);

      preview->actor = clutter_actor_new ();
      clutter_actor_set_background_color (preview->actor, CLUTTER_COLOR_Blue);
      clutter_actor_set_opacity (preview->actor, 100);

      clutter_actor_add_child (meta_get_window_group_for_screen (screen), preview->actor);
      g_object_set_qdata_full (G_OBJECT (screen),
                               screen_tile_preview_data_quark, preview,
                               free_screen_tile_preview);
    }

  return preview;
}

static void
show_tile_preview (MetaPlugin    *plugin,
                   MetaWindow    *window,
                   MetaRectangle *tile_rect,
                   int            tile_monitor_number)
{
  MetaScreen *screen = meta_plugin_get_screen (plugin);
  ScreenTilePreview *preview = get_screen_tile_preview (screen);
  ClutterActor *window_actor;

  if (clutter_actor_is_visible(preview->actor)
      && preview->tile_rect.x == tile_rect->x
      && preview->tile_rect.y == tile_rect->y
      && preview->tile_rect.width == tile_rect->width
      && preview->tile_rect.height == tile_rect->height)
    return; /* nothing to do */

  clutter_actor_set_position (preview->actor, tile_rect->x, tile_rect->y);
  clutter_actor_set_size (preview->actor, tile_rect->width, tile_rect->height);

  clutter_actor_show (preview->actor);

  window_actor = CLUTTER_ACTOR (meta_window_get_compositor_private (window));
  clutter_actor_lower (preview->actor, window_actor);

  preview->tile_rect = *tile_rect;
}

static void
hide_tile_preview (MetaPlugin *plugin)
{
  MetaScreen *screen = meta_plugin_get_screen (plugin);
  ScreenTilePreview *preview = get_screen_tile_preview (screen);

  clutter_actor_hide (preview->actor);
}

static void
kill_switch_workspace (MetaPlugin     *plugin)
{
  ElsaWmPluginPrivate *priv = ELSA_WM_PLUGIN (plugin)->priv;

  if (priv->tml_switch_workspace1)
    {
      clutter_timeline_stop (priv->tml_switch_workspace1);
      clutter_timeline_stop (priv->tml_switch_workspace2);
      g_signal_emit_by_name (priv->tml_switch_workspace1, "completed", NULL);
    }
}

static void
kill_window_effects (MetaPlugin      *plugin,
                     MetaWindowActor *window_actor)
{
  ActorPrivate *apriv;

  apriv = get_actor_private (window_actor);

  if (apriv->tml_minimize)
    {
      clutter_timeline_stop (apriv->tml_minimize);
      g_signal_emit_by_name (apriv->tml_minimize, "completed", NULL);
    }

  if (apriv->tml_map)
    {
      clutter_timeline_stop (apriv->tml_map);
      g_signal_emit_by_name (apriv->tml_map, "completed", NULL);
    }

  if (apriv->tml_destroy)
    {
      clutter_timeline_stop (apriv->tml_destroy);
      g_signal_emit_by_name (apriv->tml_destroy, "completed", NULL);
    }
}

static const MetaPluginInfo *
plugin_info (MetaPlugin *plugin)
{
  ElsaWmPluginPrivate *priv = ELSA_WM_PLUGIN (plugin)->priv;

  return &priv->info;
}

static void
on_dialog_closed (GPid     pid,
                  gint     status,
                  gpointer user_data)
{
  MetaPlugin *plugin = user_data;
  gboolean ok;

  ok = g_spawn_check_exit_status (status, NULL);
  meta_plugin_complete_display_change (plugin, ok);
}

static void
confirm_display_change (MetaPlugin *plugin)
{
  GPid pid;

  pid = meta_show_dialog ("--question",
                          "Does the display look OK?",
                          "20",
                          NULL,
                          "_Keep This Configuration",
                          "_Restore Previous Configuration",
                          "preferences-desktop-display",
                          0,
                          NULL, NULL);

  g_child_watch_add (pid, on_dialog_closed, plugin);
}

ElsaWmPlugin *elsa_wm_plugin_get() 
{
    if (!m_plugin)
        m_plugin = g_object_new(ELSA_TYPE_WM_PLUGIN, NULL);

    return m_plugin;
}

MetaScreen *elsa_wm_plugin_get_screen(ElsaWmPlugin *self) 
{
    return meta_plugin_get_screen(META_PLUGIN(self));
}

MetaDisplay *elsa_wm_plugin_get_display(ElsaWmPlugin *self) 
{
    return meta_screen_get_display(elsa_wm_plugin_get_screen(self));
}
