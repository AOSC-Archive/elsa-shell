noinst_LTLIBRARIES = libelsa-panel.la

SUBDIRS = launcher	\
		  showdesktop	\
		  taskbar	\
		  systray	\
		  sound	\
		  datetime	\
		  $(NULL)

libelsa_panel_la_SOURCES = elsa-panel.c	\
						   elsa-popup.c	\
						   $(NULL)
libelsa_panel_la_CFLAGS = $(GTK3_CFLAGS)	\
						  -I$(srcdir)/launcher	\
						  -I$(srcdir)/showdesktop	\
						  -I$(srcdir)/taskbar	\
						  -I$(srcdir)/systray	\
						  -I$(srcdir)/sound	\
						  -I$(srcdir)/datetime	\
						  $(NULL)
libelsa_panel_la_LIBADD = $(GTK3_LIBS)	\
						  $(builddir)/launcher/libelsa-launcher.la	\
						  $(builddir)/showdesktop/libelsa-showdesktop.la	\
						  $(builddir)/taskbar/libelsa-taskbar.la	\
						  $(builddir)/systray/libelsa-systray.la	\
						  $(builddir)/sound/libelsa-sound.la	\
						  $(builddir)/datetime/libelsa-datetime.la	\
						  $(NULL)
