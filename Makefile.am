##
## Process this file with automake to produce Makefile.in
##

AUTOMAKE_OPTIONS = 1.3

SUBDIRS = src examples doc misc m4

DEBFILES = debian/README.Debian debian/bug-presubj debian/changelog \
	debian/control debian/copyright debian/dvdnav-config.1 \
	debian/libdvdnav-dev.examples debian/libdvdnav-dev.files \
	debian/rules debian/shlibs

EXTRA_DIST = autogen.sh \
	     automake.diff \
	     libdvdnav.spec \
	     $(DEBFILES) \
	     AUTHORS \
	     ChangeLog \
	     configure \
	     config.guess \
	     config.sub \
	     COPYING \
	     INSTALL \
	     install-sh \
	     libtool \
	     ltmain.sh \
	     missing \
	     mkinstalldirs \
	     README \
	     TODO \
	     depcomp \
	     CREDITS \
	     @DEPCOMP@

debug:
	@list='$(SUBDIRS)'; for subdir in $$list; do \
	  (cd $$subdir && $(MAKE) $@) || exit;\
	done;

install-debug: debug
	@list='$(SUBDIRS)'; for subdir in $$list; do \
	  (cd $$subdir && $(MAKE) $@) || exit; \
	done;

prune-cache:
	-rm -f config.cache

mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#* 
	-rm -f $(PACKAGE)_$(VERSION).tar.gz
	-rm -f $(distdir).tar.gz $(PACKAGE).tgz package_descriptions
	-rm -rf $(distdir)

maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in configure acinclude.m4 aclocal.m4
	-rm -f config.h.in stamp-h.in ltconfig ltmain.sh
	-rm -f config.guess config.sub install-sh missing mkinstalldirs
	-rm -f depcomp config.log

maintainer-clean-generic-hook: 
	rm -f config.status
