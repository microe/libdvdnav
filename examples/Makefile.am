
CFLAGS = -I$(top_srcdir)/src/ 

LDFLAGS = -L$(top_srcdir)/src/

noinst_PROGRAMS = menus

menus_SOURCES = menus.c
menus_LDADD = $(top_builddir)/src/libdvdnav.la

debug:
	@$(MAKE) CFLAGS="$(CFLAGS) $(DEBUG_CFLAGS)"

install-debug: debug
	@$(MAKE) $(AM_MAKEFLAGS) install-exec-am install-data-am

mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#*

maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in
	-rm -f utils.c
