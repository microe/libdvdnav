CFLAGS = @GLOBAL_CFLAGS@ @THREAD_CFLAGS@

lib_LTLIBRARIES = libdvdnav.la

libdvdnav_la_SOURCES = decoder.c dvdnav.c vm.c vmcmd.c \
		       read_cache.c navigation.c highlight.c \
		       searching.c settings.c
libdvdnav_la_LDFLAGS = $(DVDREAD_LIBS) -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-release $(LT_RELEASE) 

include_HEADERS = decoder.h dvdnav.h dvdnav_events.h \
	dvdnav_internal.h vm.h vmcmd.h read_cache.h dvd_types.h

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
