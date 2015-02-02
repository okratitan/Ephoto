MAINTAINERCLEANFILES = Makefile.in Makefile

EDJE_CC = @edje_cc@

filesdir = $(datadir)/$(PACKAGE)/themes
files_DATA = crop.edj

EXTRA_DIST = crop.edc

crop.edj: Makefile $(EXTRA_DIST)
	$(EDJE_CC) \
	$(top_srcdir)/data/themes/crop.edc \
	$(top_builddir)/data/themes/crop.edj

clean-local:
	rm -f *.edj
