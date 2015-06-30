AC_INIT([elsa-shell], [0.1], [xiang.zhai@i-soft.com.cn])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_PROG_CC
AM_PROG_AR
AC_CONFIG_MACRO_DIR([m4])
AM_MAINTAINER_MODE
IT_PROG_INTLTOOL([0.35.0])                                                      
LT_INIT

CFLAGS='-std=gnu11 -Werror'

MUTTER_REQUIRED=3.16.2
SN_REQUIRED=0.12
X11_REQUIRED=1.6.3
GTK3_REQUIRED=3.16.4
GDK3_REQUIRED=3.16.4
GLIB2_REQUIRED=2.44.1
GNOME_MENU_REQUIRED=3.13.3
WNCK3_REQUIRED=3.14.1
GNOME_DESKTOP_REQUIRED=3.16.2
GSETTINGS_DESKTOP_SCHEMAS_REQUIRED=3.16.1

PKG_CHECK_MODULES(MUTTER, libmutter >= MUTTER_REQUIRED)
AC_SUBST(MUTTER_CFLAGS)
AC_SUBST(MUTTER_LIBS)

PKG_CHECK_MODULES(SN, libstartup-notification-1.0 >= SN_REQUIRED)
AC_SUBST(SN_CFLAGS)
AC_SUBST(SN_LIBS)

PKG_CHECK_MODULES(X11, x11 >= X11_REQUIRED)
AC_SUBST(X11_CFLAGS)
AC_SUBST(X11_LIBS)

PKG_CHECK_MODULES(GTK3, gtk+-3.0 >= GTK3_REQUIRED
                  gdk-3.0 >= GDK3_REQUIRED
                  glib-2.0 >= GLIB2_REQUIRED)
AC_SUBST(GTK3_CFLAGS)
AC_SUBST(GTK3_LIBS)

PKG_CHECK_MODULES(GNOME_MENU, libgnome-menu-3.0 >= GNOME_MENU_REQUIRED)
AC_SUBST(GNOME_MENU_CFLAGS)
AC_SUBST(GNOME_MENU_LIBS)

PKG_CHECK_MODULES(WNCK3, libwnck-3.0 >= WNCK3_REQUIRED)
AC_SUBST(WNCK3_CFLAGS)
AC_SUBST(WNCK3_LIBS)

PKG_CHECK_MODULES(GNOME_DESKTOP, gsettings-desktop-schemas >= GSETTINGS_DESKTOP_SCHEMAS_REQUIRED)
AC_SUBST(GNOME_DESKTOP_CFLAGS)
AC_SUBST(GNOME_DESKTOP_LIBS)

dnl ================================================================            
dnl  Language Support                                                           
dnl ================================================================            
GETTEXT_PACKAGE=elsa-shell 
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Define the gettext package to be used])
AC_SUBST(GETTEXT_PACKAGE)                                                       
AM_GLIB_GNU_GETTEXT
AM_GLIB_DEFINE_LOCALEDIR(ELSA_SHELL_LOCALEDIR)

AC_ARG_ENABLE(debug,
              [AS_HELP_STRING([--enable-debug=@<:@no/yes@:>@],
                              [turn on debugging [default=no]])],,
              [enable_debug=no])

if test "x$enable_debug" = "xyes"; then
  test "$cflags_set" = set || CFLAGS="$CFLAGS -g -DDEBUG"
fi

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
 Makefile
 po/Makefile.in
 src/Makefile
 src/wm/Makefile
 src/session/Makefile
 src/desktop/Makefile
 src/panel/Makefile
 src/panel/launcher/Makefile
 src/panel/taskbar/Makefile
 src/panel/systray/Makefile
 src/panel/datetime/Makefile
])
AC_OUTPUT
