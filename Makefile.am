include $(top_srcdir)/misc/Makefile.common

AUTOMAKE_OPTIONS = 1.3

SUBDIRS = src examples doc misc m4

DEBFILES = debian/README.Debian debian/bug-presubj debian/changelog \
	debian/control debian/copyright debian/dvdnav-config.1 \
	debian/libdvdnav-dev.docs debian/libdvdnav-dev.install \
	debian/rules debian/shlibs debian/compat

EXTRA_DIST = autogen.sh \
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
	     $(DEPCOMP)

MOSTLYCLEANFILES += $(PACKAGE)_$(VERSION).tar.gz \
	$(distdir).tar.gz $(PACKAGE).tgz package_descriptions

MAINTAINERCLEANFILES += configure $(ACLOCAL_M4) config.h.in \
	ltmain.sh config.guess config.sub install-sh missing \
	mkinstalldirs $(DEPCOMP)

world:
	@$(MAKE) clean all install 2> warnings.log
	test -s warnings.log || rm warnings.log

prune-cache:
	-rm -f config.cache

release-check:
	@./config.status misc/relchk.sh
	@./autogen.sh noconfig && $(SHELL) misc/relchk.sh

dist-hook:
	cp -r $(srcdir)/msvc $(distdir)/msvc
	rm -rf `find $(distdir)/msvc -name CVS`
