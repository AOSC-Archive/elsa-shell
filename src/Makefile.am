bin_PROGRAMS = elsa-shell

SUBDIRS = wm	\
		  session	\
		  panel	\
		  $(NULL)

elsa_shell_SOURCES = main.c	\
					 $(NULL)
elsa_shell_CFLAGS = $(MUTTER_CFLAGS)	\
					$(GTK3_CFLAGS)	\
					-I$(srcdir)/wm	\
					-I$(srcdir)/session	\
					-I$(srcdir)/panel
elsa_shell_LDFLAGS = $(MUTTER_LIBS)	\
					 $(GTK3_LIBS)	\
					 $(builddir)/wm/libelsa-wm.la	\
					 $(builddir)/session/libelsa-session.la	\
					 $(builddir)/panel/libelsa-panel.la
