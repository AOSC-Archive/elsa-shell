/* Copyright (C) 2015 Leslie Zhai <xiang.zhai@i-soft.com.cn> */

#include "config.h"
#include "elsa-session.h"

#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <glib.h>
#include <gio/gdesktopappinfo.h>

static const char *autostart_filter[] = {
    "cairo-dock.desktop",
    "caribou-autostart.desktop",
    "kmix_autostart.desktop",
    "krunner.desktop",
    "orbital-launcher.desktop",
    "org.kde.klipper.desktop",
    "plasmashell.desktop",
    "polkit-kde-authentication-agent-1.desktop",
    "tracker-extract.desktop",
    "tracker-miner-apps.desktop",
    "tracker-miner-fs.desktop",
    "tracker-miner-user-guides.desktop",
    "tracker-store.desktop",
    NULL};

static gboolean is_autostart(const char *file) 
{
    for (int i = 0; autostart_filter[i]; i++) {
        if (strstr(file, autostart_filter[i]))
            return FALSE;
    }

    return TRUE;
}

static void traverse_directory(const gchar *path) 
{
    GDir *dir = NULL;
    GError *error = NULL;
    const gchar *file = NULL;
    char buf[PATH_MAX];

    dir = g_dir_open(path, 0, &error);
    if (!dir) {
        g_warning("%s, line %d, %s\n", __func__, __LINE__, error->message);
        g_error_free(error);
        error = NULL;
        return;
    }

    while (file = g_dir_read_name(dir)) {
        if (!is_autostart(file))
            continue;

        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf) - 1, "%s/%s", path, file);
        
        if (g_file_test(buf, G_FILE_TEST_IS_DIR)) {
            g_warning("I argue that mkdir sub-directory is not good habit ;-)");
            traverse_directory(buf);
        } else if (g_file_test(buf, G_FILE_TEST_IS_REGULAR)) {
#if DEBUG
            g_message("%s, line %d, %s", __func__, __LINE__, buf);
#endif
            GDesktopAppInfo *appinfo = g_desktop_app_info_new_from_filename(buf);
            if (appinfo)
                g_app_info_launch((GAppInfo *)appinfo, NULL, NULL, NULL);
        }
    }

    g_dir_close(dir);
    dir = NULL;
}

void elsa_session_autostart() 
{
    char buf[PATH_MAX] = {'\0'};

    traverse_directory("/etc/xdg/autostart");
    snprintf(buf, sizeof(buf) - 1, "%s/.config/autostart", g_get_home_dir());
    g_message("%s, line %d, %s\n", __func__, __LINE__, buf);
    traverse_directory(buf);
}
