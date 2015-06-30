/*
 * This file inherit from gnome-shell/src/shell-app.c 
 */

#include "config.h"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <meta/display.h>

#include "shell-app.h"
#include "elsa-wm-plugin.h"

#ifdef HAVE_SYSTEMD
#include <systemd/sd-journal.h>
#include <errno.h>
#include <unistd.h>
#endif

/* This is mainly a memory usage optimization - the user is going to
 * be running far fewer of the applications at one time than they have
 * installed.  But it also just helps keep the code more logically
 * separated.
 */
typedef struct {
  guint refcount;

  /* Signal connection to dirty window sort list on workspace changes */
  guint workspace_switch_id;

  GSList *windows;

  guint interesting_windows;

  /* Whether or not we need to resort the windows; this is done on demand */
  guint window_sort_stale : 1;

} ShellAppRunningState;

/**
 * SECTION:shell-app
 * @short_description: Object representing an application
 *
 * This object wraps a #GDesktopAppInfo, providing methods and signals
 * primarily useful for running applications.
 */
struct _ShellApp
{
  GObject parent;

  int started_on_workspace;

  GDesktopAppInfo *info; /* If NULL, this app is backed by one or more
                          * MetaWindow.  For purposes of app title
                          * etc., we use the first window added,
                          * because it's most likely to be what we
                          * want (e.g. it will be of TYPE_NORMAL from
                          * the way shell-window-tracker.c works).
                          */

  ShellAppRunningState *running_state;

  char *window_id_string;
  char *name_collation_key;
};

enum {
  PROP_0,
  PROP_STATE,
  PROP_ID,
  PROP_DBUS_ID,
};

enum {
  WINDOWS_CHANGED,
  LAST_SIGNAL
};

static guint shell_app_signals[LAST_SIGNAL] = { 0 };

static void create_running_state (ShellApp *app);
static void unref_running_state (ShellAppRunningState *state);

G_DEFINE_TYPE (ShellApp, shell_app, G_TYPE_OBJECT)

static void
shell_app_get_property (GObject    *gobject,
                        guint       prop_id,
                        GValue     *value,
                        GParamSpec *pspec)
{
  ShellApp *app = SHELL_APP (gobject);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_string (value, shell_app_get_id (app));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

const char *
shell_app_get_id (ShellApp *app)
{
  if (app->info)
    return g_app_info_get_id (G_APP_INFO (app->info));
  return app->window_id_string;
}

static MetaWindow *
window_backed_app_get_window (ShellApp     *app)
{
  g_assert (app->info == NULL);
  g_assert (app->running_state);
  g_assert (app->running_state->windows);
  return app->running_state->windows->data;
}

static ClutterActor* _clutter_image_from_pixbuf(GdkPixbuf* pix)
{
  gint w = gdk_pixbuf_get_width(pix), h = gdk_pixbuf_get_height(pix);
  GError* error = NULL;
  ClutterContent* cnt = clutter_image_new();
  if (!clutter_image_set_data(CLUTTER_IMAGE(cnt), gdk_pixbuf_read_pixels(pix),
              gdk_pixbuf_get_has_alpha(pix) ? COGL_PIXEL_FORMAT_RGBA_8888: COGL_PIXEL_FORMAT_RGB_888,
              w, h, gdk_pixbuf_get_rowstride(pix), &error)) {
      g_debug("load icon failed");
  }

  ClutterActor* actor = clutter_actor_new();
  clutter_actor_set_content(actor, cnt);
  g_object_unref(cnt);
  clutter_actor_set_size(actor, w, h);
  return actor;
}

static ClutterActor *
window_backed_app_get_icon (ShellApp *app,
                            int       size)
{
    MetaWindow *window;
    ClutterActor *actor = NULL;

    /* During a state transition from running to not-running for
     * window-backend apps, it's possible we get a request for the icon.
     * Avoid asserting here and just return an empty image.
     */
    if (app->running_state == NULL)
    {
        actor = clutter_actor_new();
        g_object_set (actor, "opacity", 0, "width", (float) size, "height", (float) size, NULL);
        return actor;
    }

    window = window_backed_app_get_window (app);

    GdkPixbuf* pix = NULL;
    g_object_get(window, "icon", &pix, NULL);

    if (NULL != pix && GDK_IS_PIXBUF(pix)) {
        actor = _clutter_image_from_pixbuf(pix);
        g_object_set (actor, "width", (float) size, "height", (float) size, NULL);
    }

    return actor;
}

ClutterActor* shell_app_create_icon_texture (ShellApp* app, int size)
{
    GIcon *icon;
    ClutterActor *ret = NULL;
    GtkIconInfo* info = NULL;

    if (app->info == NULL) {
        ret = window_backed_app_get_icon (app, size);
        if (ret) return ret;
        // or else
        info = gtk_icon_theme_lookup_icon(gtk_icon_theme_get_default(),
                                          "application-default-icon", size, GTK_ICON_LOOKUP_FORCE_SIZE);
    } else {
        icon = g_app_info_get_icon (G_APP_INFO (app->info));
        if (icon != NULL) {
            g_debug("%s: icon from app info", __func__);
            info = gtk_icon_theme_lookup_by_gicon(gtk_icon_theme_get_default(),
                                                  icon, size, GTK_ICON_LOOKUP_FORCE_SIZE);
        } else {
            info = gtk_icon_theme_lookup_icon(gtk_icon_theme_get_default(),
                                              "application-default-icon", size, GTK_ICON_LOOKUP_FORCE_SIZE);
        }
    }

    if (info) {
        GError* error = NULL;
        GdkPixbuf* pix = gtk_icon_info_load_icon(info, &error);
        if (error) {
            g_warning("%s: %s", __func__, error->message);
            g_error_free(error);
        } else {
            ret = _clutter_image_from_pixbuf(pix);
            g_object_set (ret, "width", (float) size, "height", (float) size, NULL);
            g_object_unref(pix);
        }

        g_object_unref(info);
    }
    return ret;
}

const char *
shell_app_get_name (ShellApp *app)
{
  if (app->info)
    return g_app_info_get_name (G_APP_INFO (app->info));
  else
    {
      MetaWindow *window = window_backed_app_get_window (app);
      const char *name;

      name = meta_window_get_wm_class (window);
      if (!name)
        name = C_("program", "Unknown");
      return name;
    }
}

const char *
shell_app_get_description (ShellApp *app)
{
  if (app->info)
    return g_app_info_get_description (G_APP_INFO (app->info));
  else
    return NULL;
}

/**
 * shell_app_is_window_backed:
 *
 * A window backed application is one which represents just an open
 * window, i.e. there's no .desktop file assocation, so we don't know
 * how to launch it again.
 */
gboolean
shell_app_is_window_backed (ShellApp *app)
{
  return app->info == NULL;
}

typedef struct {
  MetaWorkspace *workspace;
  GSList **transients;
} CollectTransientsData;

typedef struct {
  ShellApp *app;
  MetaWorkspace *active_workspace;
} CompareWindowsData;

static int
shell_app_compare_windows (gconstpointer   a,
                           gconstpointer   b,
                           gpointer        datap)
{
  MetaWindow *win_a = (gpointer)a;
  MetaWindow *win_b = (gpointer)b;
  CompareWindowsData *data = datap;
  gboolean ws_a, ws_b;
  gboolean vis_a, vis_b;

  ws_a = meta_window_get_workspace (win_a) == data->active_workspace;
  ws_b = meta_window_get_workspace (win_b) == data->active_workspace;

  if (ws_a && !ws_b)
    return -1;
  else if (!ws_a && ws_b)
    return 1;

  vis_a = meta_window_showing_on_its_workspace (win_a);
  vis_b = meta_window_showing_on_its_workspace (win_b);

  if (vis_a && !vis_b)
    return -1;
  else if (!vis_a && vis_b)
    return 1;

  return meta_window_get_user_time (win_b) - meta_window_get_user_time (win_a);
}

/**
 * shell_app_get_windows:
 * @app:
 *
 * Get the windows which are associated with this application. The
 * returned list will be sorted first by whether they're on the
 * active workspace, then by whether they're visible, and finally
 * by the time the user last interacted with them.
 *
 * Returns: (transfer none) (element-type MetaWindow): List of windows
 */
GSList *
shell_app_get_windows(ShellApp *app)
{
    if (app->running_state == NULL)
        return NULL;

    if (app->running_state->window_sort_stale) {
        CompareWindowsData data;
        data.app = app;
        data.active_workspace = meta_screen_get_active_workspace(
            elsa_wm_plugin_get_screen(elsa_wm_plugin_get()));
        app->running_state->windows = g_slist_sort_with_data(
            app->running_state->windows, shell_app_compare_windows, &data);
        app->running_state->window_sort_stale = FALSE;
    }

    return app->running_state->windows;
}

guint
shell_app_get_n_windows (ShellApp *app)
{
  if (app->running_state == NULL)
    return 0;
  return g_slist_length (app->running_state->windows);
}

gboolean
shell_app_is_on_workspace (ShellApp *app,
                           MetaWorkspace   *workspace)
{
  GSList *iter;

  if (app->running_state == NULL)
    return FALSE;

  for (iter = app->running_state->windows; iter; iter = iter->next)
    {
      if (meta_window_get_workspace (iter->data) == workspace)
        return TRUE;
    }

  return FALSE;
}

void
_shell_app_set_app_info (ShellApp        *app,
                         GDesktopAppInfo *info)
{
  g_clear_object (&app->info);
  app->info = g_object_ref (info);

  if (app->name_collation_key != NULL)
    g_free (app->name_collation_key);
  app->name_collation_key = g_utf8_collate_key (shell_app_get_name (app), -1);
}

ShellApp *
_shell_app_new (GDesktopAppInfo *info)
{
  ShellApp *app;

  app = g_object_new (SHELL_TYPE_APP, NULL);

  _shell_app_set_app_info (app, info);

  return app;
}

static void
shell_app_on_unmanaged (MetaWindow      *window,
                        ShellApp *app)
{
  _shell_app_remove_window (app, window);
}

static void
shell_app_on_user_time_changed (MetaWindow *window,
                                GParamSpec *pspec,
                                ShellApp   *app)
{
  g_assert (app->running_state != NULL);

  /* Ideally we don't want to emit windows-changed if the sort order
   * isn't actually changing. This check catches most of those.
   */
  if (window != app->running_state->windows->data)
    {
      app->running_state->window_sort_stale = TRUE;
      g_signal_emit (app, shell_app_signals[WINDOWS_CHANGED], 0);
    }
}

static void
shell_app_on_skip_taskbar_changed (MetaWindow *window,
                                   GParamSpec *pspec,
                                   ShellApp   *app)
{
  g_assert (app->running_state != NULL);

  /* we rely on MetaWindow:skip-taskbar only being notified
   * when it actually changes; when that assumption breaks,
   * we'll have to track the "interesting" windows themselves
   */
  if (meta_window_is_skip_taskbar (window))
    app->running_state->interesting_windows--;
  else
    app->running_state->interesting_windows++;
}

static void
shell_app_on_ws_switch (MetaScreen         *screen,
                        int                 from,
                        int                 to,
                        MetaMotionDirection direction,
                        gpointer            data)
{
  ShellApp *app = SHELL_APP (data);

  g_assert (app->running_state != NULL);

  app->running_state->window_sort_stale = TRUE;

  g_signal_emit (app, shell_app_signals[WINDOWS_CHANGED], 0);
}

void
_shell_app_add_window (ShellApp        *app,
                       MetaWindow      *window)
{
  if (app->running_state && g_slist_find (app->running_state->windows, window))
    return;

  g_object_freeze_notify (G_OBJECT (app));

  if (!app->running_state)
      create_running_state (app);

  app->running_state->window_sort_stale = TRUE;
  app->running_state->windows = g_slist_prepend (app->running_state->windows, g_object_ref (window));
  g_signal_connect (window, "unmanaged", G_CALLBACK(shell_app_on_unmanaged), app);
  g_signal_connect (window, "notify::user-time", G_CALLBACK(shell_app_on_user_time_changed), app);
  g_signal_connect (window, "notify::skip-taskbar", G_CALLBACK(shell_app_on_skip_taskbar_changed), app);

  if (!meta_window_is_skip_taskbar (window))
    app->running_state->interesting_windows++;

  g_object_thaw_notify (G_OBJECT (app));

  g_signal_emit (app, shell_app_signals[WINDOWS_CHANGED], 0);
}

void
_shell_app_remove_window (ShellApp   *app,
                          MetaWindow *window)
{
  g_assert (app->running_state != NULL);

  if (!g_slist_find (app->running_state->windows, window))
    return;

  g_signal_handlers_disconnect_by_func (window, G_CALLBACK(shell_app_on_unmanaged), app);
  g_signal_handlers_disconnect_by_func (window, G_CALLBACK(shell_app_on_user_time_changed), app);
  g_signal_handlers_disconnect_by_func (window, G_CALLBACK(shell_app_on_skip_taskbar_changed), app);
  g_object_unref (window);
  app->running_state->windows = g_slist_remove (app->running_state->windows, window);

  if (!meta_window_is_skip_taskbar (window))
    app->running_state->interesting_windows--;

  if (app->running_state && app->running_state->windows == NULL)
    g_clear_pointer (&app->running_state, unref_running_state);

  g_signal_emit (app, shell_app_signals[WINDOWS_CHANGED], 0);
}

/**
 * shell_app_get_pids:
 * @app: a #ShellApp
 *
 * Returns: (transfer container) (element-type int): An unordered list of process identifers associated with this application.
 */
GSList *
shell_app_get_pids (ShellApp *app)
{
  GSList *result;
  GSList *iter;

  result = NULL;
  for (iter = shell_app_get_windows (app); iter; iter = iter->next)
    {
      MetaWindow *window = iter->data;
      int pid = meta_window_get_pid (window);
      /* Note in the (by far) common case, app will only have one pid, so
       * we'll hit the first element, so don't worry about O(N^2) here.
       */
      if (!g_slist_find (result, GINT_TO_POINTER (pid)))
        result = g_slist_prepend (result, GINT_TO_POINTER (pid));
    }
  return result;
}

#ifdef HAVE_SYSTEMD
/* This sets up the launched application to log to the journal
 * using its own identifier, instead of just "gnome-session".
 */
static void
app_child_setup (gpointer user_data)
{
  const char *appid = user_data;
  int res;
  int journalfd = sd_journal_stream_fd (appid, LOG_INFO, FALSE);
  if (journalfd >= 0)
    {
      do
        res = dup2 (journalfd, 1);
      while (G_UNLIKELY (res == -1 && errno == EINTR));
      do
        res = dup2 (journalfd, 2);
      while (G_UNLIKELY (res == -1 && errno == EINTR));
      (void) close (journalfd);
    }
}
#endif

/**
 * shell_app_get_app_info:
 * @app: a #ShellApp
 *
 * Returns: (transfer none): The #GDesktopAppInfo for this app, or %NULL if backed by a window
 */
GDesktopAppInfo *
shell_app_get_app_info (ShellApp *app)
{
  return app->info;
}

static void
create_running_state (ShellApp *app)
{
  MetaScreen *screen;

  g_assert (app->running_state == NULL);

  screen = elsa_wm_plugin_get_screen(elsa_wm_plugin_get());
  app->running_state = g_slice_new0 (ShellAppRunningState);
  app->running_state->refcount = 1;
  app->running_state->workspace_switch_id =
    g_signal_connect (screen, "workspace-switched", G_CALLBACK(shell_app_on_ws_switch), app);

}

static void
unref_running_state (ShellAppRunningState *state)
{
  MetaScreen *screen;

  g_assert (state->refcount > 0);

  state->refcount--;
  if (state->refcount > 0)
    return;

  screen = elsa_wm_plugin_get_screen(elsa_wm_plugin_get());
  g_signal_handler_disconnect (screen, state->workspace_switch_id);

  g_slice_free (ShellAppRunningState, state);
}

/**
 * shell_app_compare_by_name:
 * @app: One app
 * @other: The other app
 *
 * Order two applications by name.
 *
 * Returns: -1, 0, or 1; suitable for use as a comparison function
 * for e.g. g_slist_sort()
 */
int
shell_app_compare_by_name (ShellApp *app, ShellApp *other)
{
  return strcmp (app->name_collation_key, other->name_collation_key);
}

static void
shell_app_init (ShellApp *self)
{
}

static void
shell_app_dispose (GObject *object)
{
  ShellApp *app = SHELL_APP (object);

  g_clear_object (&app->info);

  while (app->running_state)
    _shell_app_remove_window (app, app->running_state->windows->data);

  /* We should have been transitioned when we removed all of our windows */
  g_assert (app->running_state == NULL);

  G_OBJECT_CLASS(shell_app_parent_class)->dispose (object);
}

static void
shell_app_finalize (GObject *object)
{
  ShellApp *app = SHELL_APP (object);

  g_free (app->window_id_string);

  g_free (app->name_collation_key);

  G_OBJECT_CLASS(shell_app_parent_class)->finalize (object);
}

static void
shell_app_class_init(ShellAppClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = shell_app_get_property;
  gobject_class->dispose = shell_app_dispose;
  gobject_class->finalize = shell_app_finalize;

  shell_app_signals[WINDOWS_CHANGED] = g_signal_new ("windows-changed",
                                     SHELL_TYPE_APP,
                                     G_SIGNAL_RUN_LAST,
                                     0,
                                     NULL, NULL, NULL,
                                     G_TYPE_NONE, 0);


  /**
   * ShellApp:id:
   *
   * The id of this application (a desktop filename, or a special string
   * like window:0xabcd1234)
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ID,
                                   g_param_spec_string ("id",
                                                        "Application id",
                                                        "The desktop file id of this ShellApp",
                                                        NULL,
                                                        G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

}

ShellApp *
_shell_app_new_for_window (MetaWindow      *window)
{
  ShellApp *app;

  app = g_object_new (SHELL_TYPE_APP, NULL);

  app->window_id_string = g_strdup_printf ("window:%d", meta_window_get_stable_sequence (window));

  _shell_app_add_window (app, window);

  return app;
}
