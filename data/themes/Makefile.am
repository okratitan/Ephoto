MAINTAINERCLEANFILES = Makefile.in Makefile

EDJE_CC = @edje_cc@
EDJE_FLAGS = -id $(top_srcdir)/data/themes

filesdir = $(datadir)/$(PACKAGE)/themes
files_DATA = crop.edj

EXTRA_DIST = crop.edc \
             sel_border.png \
             sel_corner1.png \
             sel_corner3.png \
             sel_corner5.png \
             sel_corner7.png

crop.edj: Makefile $(EXTRA_DIST)
	$(EDJE_CC) $(EDJE_FLAGS) \
	$(top_srcdir)/data/themes/crop.edc \
	$(top_builddir)/data/themes/crop.edj

clean-local:
	rm -f *.edj
