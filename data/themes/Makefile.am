AUTOMAKE_OPTIONS = subdir-objects
MAINTAINERCLEANFILES = Makefile.in Makefile

EDJE_CC = @edje_cc@
EDJE_FLAGS = -id $(top_srcdir)/data/themes

filesdir = $(datadir)/$(PACKAGE)/themes
files_DATA = ephoto.edj

EXTRA_DIST = ephoto.edc \
             images/sel_border.png \
             images/sel_corner1.png \
             images/sel_corner3.png \
             images/sel_corner5.png \
             images/sel_corner7.png 

ephoto.edj: Makefile $(EXTRA_DIST)
	$(EDJE_CC) $(EDJE_FLAGS) \
	$(top_srcdir)/data/themes/ephoto.edc \
	$(top_builddir)/data/themes/ephoto.edj

clean-local:
	rm -f *.edj
