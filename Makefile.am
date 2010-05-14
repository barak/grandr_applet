SUBDIRS = src 

serverdir = $(libdir)/bonobo/servers
server_in_files = GrandrApplet.server
server_DATA = GrandrApplet.server

$(server_in_files): $(server_in_files).in
	sed s,@LIBEXECDIR@,$(libexecdir),g <$< >$@

pixmapsdir = $(datadir)/pixmaps
pixmaps_DATA = grandr.png

EXTRA_DIST = GrandrApplet.server.in grandr.png


