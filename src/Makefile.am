lib_LTLIBRARIES = libdvdnav.la

AM_CPPFLAGS = -DDVDNAV_COMPILE
AM_CFLAGS = @THREAD_CFLAGS@

libdvdnav_la_SOURCES = decoder.c dvdnav.c vm.c vmcmd.c \
		       read_cache.c navigation.c highlight.c \
		       searching.c settings.c remap.c \
                       dvd_reader.c nav_read.c ifo_read.c \
                       dvd_input.c dvd_udf.c md5.c

libdvdnav_la_LIBADD = @DYNAMIC_LD_LIBS@

libdvdnav_la_LDFLAGS = $(THREAD_LIBS) \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)
#	-release $(DVDNAV_MAJOR).$(DVDNAV_MINOR).$(DVDNAV_SUB)

include_HEADERS = dvdnav.h dvdnav_events.h dvd_types.h \
        nav_read.h ifo_read.h nav_print.h dvd_reader.h ifo_types.h nav_types.h

noinst_HEADERS = decoder.h dvdnav_internal.h vm.h vmcmd.h \
	read_cache.h remap.h

EXTRA_DIST = README.MAP FELLOWSHIP.map

###
# Install header files (default=$includedir/xine)
#
install-includeHEADERS: $(include_HEADERS)
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(includedir)/dvdnav
	@list='$(include_HEADERS)'; for p in $$list; do \
	  if test -f "$$p"; then d= ; else d="$(srcdir)/"; fi; \
	  echo " $(INSTALL_DATA) $$d$$p $(DESTDIR)$(includedir)/dvdnav/$$p"; \
	  $(INSTALL_DATA) $$d$$p $(DESTDIR)$(includedir)/dvdnav/$$p; \
	done


###
# Remove them
#
uninstall-includeHEADERS:
	@$(NORMAL_UNINSTALL)
	list='$(include_HEADERS)'; for p in $$list; do \
	  rm -f $(DESTDIR)$(includedir)/dvdnav/$$p; \
	done

debug:
	$(MAKE) CFLAGS="$(DEBUG_CFLAGS)"

install-debug: debug
	@$(MAKE) $(AM_MAKEFLAGS) install-exec-am install-data-am

mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#*

maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in
