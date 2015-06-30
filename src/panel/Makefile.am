noinst_LTLIBRARIES = libelsa-panel.la

SUBDIRS = launcher	\
		  taskbar	\
		  systray	\
		  datetime	\
		  $(NULL)

libelsa_panel_la_SOURCES = elsa-panel.c	\
						   elsa-popup.c	\
						   $(NULL)
libelsa_panel_la_CFLAGS = $(GTK3_CFLAGS)	\
						  -I$(srcdir)/launcher	\
						  -I$(srcdir)/taskbar	\
						  -I$(srcdir)/systray	\
						  -I$(srcdir)/datetime	\
						  $(NULL)
libelsa_panel_la_LIBADD = $(GTK3_LIBS)	\
						  $(builddir)/launcher/libelsa-launcher.la	\
						  $(builddir)/taskbar/libelsa-taskbar.la	\
						  $(builddir)/systray/libelsa-systray.la	\
						  $(builddir)/datetime/libelsa-datetime.la	\
						  $(NULL)
