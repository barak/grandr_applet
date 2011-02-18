INCLUDES = -I$(top_srcdir) -I$(includedir) $(GRANDR_APPLET_CFLAGS) \
       -DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
       -DGNOME_DISABLE_DEPRECATED \
       -DGTK_DISABLE_DEPRECATED \
       -DG_DISABLE_DEPRECATED \
       -DDATADIR=\""$(datadir)/"\" \
       -DPREFIX=\""$(prefix)/"\" \
       -DLIBDIR=\""$(libdir)/"\" \
       -DSYSCONFDIR=\""$(sysconfdir)/"\"

libexec_PROGRAMS = grandr

grandr_SOURCES =         \
        grandr.c

grandr_LDADD = $(GRANDR_APPLET_LIBS) $(ALL_X_LIBS) -lXrandr -lXrender
