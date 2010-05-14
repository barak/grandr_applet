SUBDIRS = src po

grandrdocdir = ${prefix}/share/doc/grandr_applet
grandrdoc_DATA = \
	README\
	COPYING\
	AUTHORS\
	ChangeLog\
	INSTALL\
	NEWS
	
serverdir = $(libdir)/bonobo/servers
server_in_files = GrandrApplet.server.in
server_DATA = GrandrApplet.server

pixmapsdir = $(datadir)/pixmaps
pixmaps_DATA = grandr.png

EXTRA_DIST = $(grandr_DATA) $(server_DATA) $(pixmaps_DATA) GrandrApplet.server.in grandr_applet.spec.in

# Copy all the spec files. Of cource, only one is actually used.
dist-hook:
	for specfile in *.spec; do \
		if test -f $$specfile; then \
			cp -p $$specfile $(distdir); \
		fi \
	done



