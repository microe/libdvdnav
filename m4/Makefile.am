
if INSTALL_M4
m4datadir = @ACLOCAL_DIR@
m4data_DATA = dvdnav.m4
endif

EXTRA_DIST = dvdnav.m4

debug:
install-debug: install

mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#*

maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in
