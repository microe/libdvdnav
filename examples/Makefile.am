AM_CPPFLAGS = -DDVDNAV_COMPILE
AM_CFLAGS = -I$(top_srcdir)/src/

DVDNAV_LIB = $(top_builddir)/src/libdvdnav.la

noinst_PROGRAMS = menus

menus_SOURCES = menus.c
menus_LDFLAGS = -L$(top_srcdir)/src/
menus_LDADD = $(DVDNAV_LIB)

debug:
	@$(MAKE) CFLAGS="$(DEBUG_CFLAGS)"

install-debug: debug
	@$(MAKE) $(AM_MAKEFLAGS) install-exec-am install-data-am

mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#*

maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in
	-rm -f utils.c

$(DVDNAV_LIB):
	@cd $(top_builddir)/src/ && $(MAKE)
