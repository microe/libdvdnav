EXTRA_DIST = doxy.conf html/* latex/*

all:	doc

doc:	doxy.conf
	doxygen doxy.conf

clean:
	rm -rf html latex man
