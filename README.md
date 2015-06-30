elsa-shell
-----------

Elsa Shell is a Window Manager, Panel and Autostart.


## Dependence

* libmutter
* libstartup-notification-1.0
* libx11
* libgtk+-3.0
* libgnome-menus
* libwnck-3.0
* gsettings-desktop-schemas
* libxml2


## Build

```
./autogen.sh --prefix=/usr --enable-debug
make clean
make
sudo make install
```


## startx

```
vim ~/.xinitrc
exec elsa-shell
```


## Inherit

* [mutter] (https://git.gnome.org/browse/mutter)
* [gnome-shell] (https://git.gnome.org/browse/gnome-shell)
* [budige-desktop] (https://github.com/solus-project/budgie-desktop)
* moses-wm
* [xtk] (https://github.com/xiangzhai/xtk)


## Patch

* [Dragging a non-CSD window leaves the cursor in invalid state] (https://bugzilla.gnome.org/show_bug.cgi?id=750120)
* [tasklist natural_width will be expanded by other windows] (https://bugzilla.gnome.org/show_bug.cgi?id=751423)
* [systray fail clearing to transparent] (https://bugzilla.gnome.org/show_bug.cgi?id=751485)
* [segfault g_settings_new with typo or not installed schema] (https://bugzilla.gnome.org/show_bug.cgi?id=751627)
* [gtk_menu_popup gtk_combo_box_menu_position wrong menu_ypos] (https://bugzilla.gnome.org/show_bug.cgi?id=750372)
* [GtkDialog can not be moved when gtk_dialog_run from GtkListBox row-activated signal callback] (https://bugzilla.gnome.org/show_bug.cgi?id=750384)
* [budgie wm background is not able to parse xml] (https://github.com/solus-project/budgie-desktop/issues/227)


## Recommend

* [Arc theme] (https://github.com/horst3180/Arc-theme)
* [Numix-Circle icon] (https://github.com/numixproject/numix-icon-theme-circle)
* [Nautilus] (https://git.gnome.org/browse/nautilus)
* [Network Manager Applet] (https://git.gnome.org/browse/network-manager-applet)


## Screenshot

![ScreenShot](https://raw.github.com/AOSC-Dev/elsa-shell/master/doc/elsa-shell-snapshot1.png)
