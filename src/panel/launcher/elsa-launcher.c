/* Copyright (C) 2015 Leslie Zhai <xiang.zhai@i-soft.com.cn> */

#include "config.h"
#include "elsa-launcher.h"

#include <stdlib.h>
#include <unistd.h>
#include <glib/gi18n.h>
#define GMENU_I_KNOW_THIS_IS_UNSTABLE
#include <gmenu-tree.h>

static GtkWidget *eventbox = NULL;
static GtkWidget *popup = NULL;

static void elsa_launcher_popup_position(GtkMenu *popup, 
                                         gint *x, 
                                         gint *y, 
                                         gint *push_in, 
                                         gpointer user_data) 
{
    GtkAllocation alloc;

    gtk_widget_get_allocation(eventbox, &alloc);
    *x = alloc.x;
}

static gboolean elsa_launcher_button_press(GtkWidget *eventbox, 
                                           GdkEventButton *event, 
                                           gpointer user_data) 
{
    gtk_widget_show_all(popup);

    gtk_menu_popup(GTK_MENU(popup), NULL, NULL, 
                   elsa_launcher_popup_position, 
                   NULL, 
                   event->button, 
                   event->time);

    return FALSE;
}

static GtkWidget *menu_item_new_with_icon_text(GIcon *icon, const char *text) 
{
    GtkWidget *menuitem = gtk_menu_item_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *image = gtk_image_new_from_gicon(icon, GTK_ICON_SIZE_LARGE_TOOLBAR);
    GtkWidget *label = gtk_label_new(text);

    gtk_image_set_pixel_size(GTK_IMAGE(image), 24);
    gtk_box_pack_start(GTK_BOX(box), image, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(menuitem), box);

    return menuitem;
}

static void menu_item_activate(GtkMenuItem *menuitem, gpointer user_data) 
{
    GAppInfo *app = (GAppInfo *)user_data;
    g_app_info_launch(app, NULL, NULL, NULL);
}

/*
 * This function inherit from gnome-menus/util/test-menu-spec.c
 */
static void traverse_directory(GMenuTreeDirectory *dir, GtkWidget *parent) 
{
    GMenuTreeIter *iter = NULL;
    GtkWidget *dir_submenu = parent ? gtk_menu_new() : NULL;
    GtkWidget *entry_submenu = gtk_menu_new();
    GIcon *icon = NULL;
    const char *text = NULL;
    GtkWidget *menuitem = NULL;

    iter = gmenu_tree_directory_iter(dir);

    while (TRUE) {
        gpointer item = NULL;

        switch (gmenu_tree_iter_next(iter)) {
        case GMENU_TREE_ITEM_INVALID:
            goto done;

        case GMENU_TREE_ITEM_ENTRY:
            item = gmenu_tree_iter_get_entry(iter);
            GDesktopAppInfo *appinfo = gmenu_tree_entry_get_app_info((GMenuTreeEntry *)item);
            icon = g_app_info_get_icon((GAppInfo *)appinfo);
            text = g_app_info_get_display_name((GAppInfo *)appinfo);
            menuitem = menu_item_new_with_icon_text(icon, text);
            gtk_menu_shell_append(GTK_MENU_SHELL(entry_submenu), menuitem);
            g_object_connect(G_OBJECT(menuitem), 
                "signal::activate", G_CALLBACK(menu_item_activate), appinfo,
                NULL);
            break;

        case GMENU_TREE_ITEM_DIRECTORY:
            item = gmenu_tree_iter_get_directory(iter);
            icon = gmenu_tree_directory_get_icon((GMenuTreeDirectory *)item);
            text = gmenu_tree_directory_get_name((GMenuTreeDirectory *)item);
            menuitem = menu_item_new_with_icon_text(icon, text);
            gtk_menu_shell_append(dir_submenu ? 
                                      GTK_MENU_SHELL(dir_submenu) : 
                                      GTK_MENU_SHELL(popup), 
                                  menuitem);

            traverse_directory(item, menuitem);
            break;
        }

        if (item) {
            gmenu_tree_item_unref(item);
            item = NULL;
        }

        continue;

done:
        break;
    }

    if (parent) {
        if (dir_submenu)
            gtk_menu_item_set_submenu(GTK_MENU_ITEM(parent), dir_submenu);

        gtk_menu_item_set_submenu(GTK_MENU_ITEM(parent), entry_submenu);
    }

    if (iter) {
        gmenu_tree_iter_unref(iter);
        iter = NULL;
    }
}

static void logout(GtkMenuItem *menuitem, gpointer user_data)
{
    kill(getppid(), SIGTERM);
    exit(0);
}

static void menu_tree_changed(GMenuTree *tree, gpointer user_data) 
{
    GMenuTreeDirectory *root = NULL;
    GtkWidget *menuitem = NULL;

    popup = gtk_menu_new();

    gmenu_tree_load_sync(tree, NULL);

    root = gmenu_tree_get_root_directory(tree);
    if (root) {
        traverse_directory(root, NULL);
        gmenu_tree_item_unref(root);
        root = NULL;
    }

    menuitem = gtk_menu_item_new_with_label(_("Logout"));
    gtk_menu_shell_append(GTK_MENU_SHELL(popup), menuitem);
    g_object_connect(G_OBJECT(menuitem), 
        "signal::activate", G_CALLBACK(logout), NULL, 
        NULL);
}

GtkWidget *elsa_launcher_new() 
{
    GtkWidget *icon = NULL;
    GMenuTree *tree = NULL;

    icon = gtk_image_new_from_icon_name("start-here", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 24);
    eventbox = gtk_event_box_new();
    gtk_container_add(GTK_CONTAINER(eventbox), icon);

    tree = gmenu_tree_new("gnome-applications.menu", 0);
    if (!tree)
        return eventbox;

    g_object_connect(G_OBJECT(tree), 
        "signal::changed", G_CALLBACK(menu_tree_changed), NULL,
        NULL);

    g_object_connect(G_OBJECT(eventbox), 
        "signal::button-press-event", G_CALLBACK(elsa_launcher_button_press), NULL,
        NULL);

    menu_tree_changed(tree, NULL);

    return eventbox;
}
