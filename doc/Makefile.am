EXTRA_DIST = doxy.conf html/* latex/*

all:	doc

doc:	doxy.conf
	doxygen doxy.conf >/dev/null 2>/dev/null

clean:
	rm -rf html latex man
