EXTRA_DIST = doxy.conf mainpage.cpp tutorial.cpp dvd_structures

docs: doxy.conf
	@if test x"$(DOXYGEN)" != "xno"; then \
	  $(DOXYGEN) doxy.conf > /dev/null 2> /dev/null; \
	fi

clean:
	rm -rf html latex man

debug: all

install-debug: debug

mostlyclean-generic:
	-rm -f *~ \#* .*~ .\#*

maintainer-clean-generic:
	-@echo "This command is intended for maintainers to use;"
	-@echo "it deletes files that may require special tools to rebuild."
	-rm -f Makefile.in
