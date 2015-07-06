/*
 * Copyright (C) 2015 AnthonOS Open Source Community
 *
 * This file inherit from mutter/src/core/mutter.c
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

#include <stdlib.h>
#include <unistd.h>
#include <meta/main.h>
#include <meta/util.h>
#include <gtk/gtk.h>

#include "elsa-wm-plugin.h"
#include "elsa-session.h"
#include "elsa-panel.h"

static ElsaPanel *elsa_panel = NULL;

static gboolean print_version (const gchar* option_name,
        const gchar* value, gpointer data, GError** error)
{
    g_print("%s %s\n", PACKAGE_NAME, PACKAGE_VERSION);

    exit(0);
}

GOptionEntry es_options[] = {
    {
        "version", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
        print_version,
        "Print version",
        NULL
    },
    { NULL }
};

int main(int argc, char *argv[])
{
    GOptionContext *ctx = NULL;
    GError *error = NULL;
    pid_t child = -1;

    ctx = meta_get_option_context();
    g_option_context_add_main_entries(ctx, es_options, GETTEXT_PACKAGE);
    if (!g_option_context_parse(ctx, &argc, &argv, &error)) {
        g_printerr ("%s: %s\n", PACKAGE_NAME, error->message);
        exit (1);
    }
    g_option_context_free(ctx);

    /* 
     * FIXME: gnome-shell puts meta and gjs into only ***one*** g_main_loop
     * 
     * I tried to 
     * 
     * meta_plugin_manager_set_plugin_type(...)
     * meta_set_wm_name(...)
     * meta_init()
     * meta_register_with_session()
     * elsa_panel_show(...)
     * meta_run()
     *
     * But when changed the screen size, for example, xrandr --output LVDS1 -- mode 800x600
     * It failed to change the panel`s position.
     *
     * How to use only one process just like gnome-shell?
     * sorry that multiple g_main_loop are not able to work in multi-pthread
     */
    child = fork();
    if (child >= 0) {
        
        if (child) {
            /* meta parent process */
            meta_plugin_manager_set_plugin_type(ELSA_TYPE_WM_PLUGIN);
            
            meta_set_wm_name(PACKAGE_NAME);
            
            meta_init();

            meta_register_with_session();

            return meta_run();
        } else {
            /* gtk child process */
            gtk_init(&argc, &argv);

            elsa_session_autostart();

            elsa_panel = elsa_panel_new();
            elsa_panel_show(elsa_panel);

            gtk_main();
            
            return 0;
        }
    }

    return 0;
}
