SUBDIRS = src misc m4

EXTRA_DIST = debian/README.debian debian/changelog debian/control debian/copyright debian/dirs \
	debian/libdvdnav1.files debian/rules libdvdnav.spec

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
