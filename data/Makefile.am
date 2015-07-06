gsettings_SCHEMAS =	\
	org.anthonos.elsa-shell.wm.gschema.xml	\
	$(NULL)

@INTLTOOL_XML_NOMERGE_RULE@

@GSETTINGS_RULES@

@INTLTOOL_XML_NOMERGE_RULE@

EXTRA_DIST = \
	elsa-shell.desktop.in	\
	$(gsettings_SCHEMAS:.xml=.xml.in.in)	\
	$(NULL)

xsessiondir = $(datadir)/xsessions

xsession_DATA = \
	elsa-shell.desktop
