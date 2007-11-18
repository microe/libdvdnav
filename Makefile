# libdvdnav Makefile

include config.mak

.SUFFIXES: .so

AR=ar
LD=ld
RANLIB=ranlib

VPATH+= $(SRC_PATH_BARE)/src
SRCS = dvdnav.c highlight.c navigation.c read_cache.c remap.c searching.c settings.c

VPATH+= $(SRC_PATH_BARE)/src/vm
SRCS+= decoder.c vm.c vmcmd.c

HEADERS = src/dvd_types.h src/dvdnav.h src/dvdnav_events.h

CFLAGS += $(USEDEBUG) -Wall -funsigned-char
CFLAGS += -I$(CURDIR) -I$(SRC_PATH)/src -I$(SRC_PATH)/src/vm
CFLAGS += -DDVDNAV_COMPILE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE
CFLAGS += -DHAVE_CONFIG_H -DHAVE_DLFCN_H

ifeq ($(DVDREAD),internal)
L=libdvdnav
MINI_L=libdvdnavmini
DVDREAD_L=libdvdread
VPATH+= $(SRC_PATH_BARE)/src/dvdread
DVDREAD_HEADERS = src/dvdread/dvd_reader.h \
	src/dvdread/ifo_print.h \
	src/dvdread/ifo_read.h \
	src/dvdread/ifo_types.h \
	src/dvdread/nav_print.h \
	src/dvdread/nav_read.h \
	src/dvdread/dvd_udf.h \
	src/dvdread/nav_types.h
DVDREAD_SRCS = dvd_input.c dvd_reader.c dvd_udf.c ifo_print.c ifo_read.c \
	md5.c nav_print.c nav_read.c
CFLAGS += -I$(SRC_PATH)/src/dvdread
else
L=libdvdnavmini
CFLAGS += -I$(DVDREAD_DIR)
endif

LIB = $(L).a
DVDREAD_LIB = $(DVDREAD_L).a
SHLIB = $(L).so
MINI_SHLIB = $(MINI_L).so
DVDREAD_SHLIB = $(DVDREAD_L).so

.OBJDIR=        obj
DEPFLAG = -M

OBJS = $(patsubst %.c,%.o, $(SRCS))
DVDREAD_OBJS = $(patsubst %.c,%.o, $(DVDREAD_SRCS))
SHOBJS = $(patsubst %.c,%.so, $(SRCS))
DVDREAD_SHOBJS = $(patsubst %.c,%.so, $(DVDREAD_SRCS))
DEPS= ${OBJS:%.o=%.d}
DVDREAD_DEPS= ${DVDREAD_OBJS:%.o=%.d}

BUILDDEPS = Makefile config.mak

ifeq ($(BUILD_SHARED),yes)
all:	$(SHLIB) $(MINI_SHLIB) $(DVDREAD_SHLIB) dvdnav-config
install: $(SHLIB) $(DVDREAD_SHLIB) install-shared install-dvdnav-config
endif

ifeq ($(BUILD_STATIC),yes)
all:	$(LIB) $(DVDREAD_LIB) dvdnav-config
install: $(LIB) $(DVDREAD_LIB) install-static install-dvdnav-config
endif

install: install-headers

# Let version.sh create version.h

SVN_ENTRIES = $(SRC_PATH_BARE)/.svn/entries
ifeq ($(wildcard $(SVN_ENTRIES)),$(SVN_ENTRIES))
version.h: $(SVN_ENTRIES)
endif

version.h:
	sh $(SRC_PATH)/version.sh $(SRC_PATH)


# General targets

$(.OBJDIR):
	mkdir $(.OBJDIR)

${LIB}: version.h $(.OBJDIR) $(OBJS) $(BUILDDEPS)
	cd $(.OBJDIR) && $(AR) rc $@ $(OBJS)
	cd $(.OBJDIR) && $(RANLIB) $@
${DVDREAD_LIB}: version.h $(.OBJDIR) $(DVDREAD_OBJS) $(BUILDDEPS)
	cd $(.OBJDIR) && $(AR) rc $@ $(DVDREAD_OBJS)
	cd $(.OBJDIR) && $(RANLIB) $@

${SHLIB}: version.h $(.OBJDIR) $(SHOBJS) $(BUILDDEPS)
	cd $(.OBJDIR) && $(CC) $(SHLDFLAGS) -o $@ $(SHOBJS) -ldvdread $(THREADLIB)
${MINI_SHLIB}: version.h $(.OBJDIR) $(SHOBJS) $(BUILDDEPS)
	cd $(.OBJDIR) && $(CC) $(SHLDFLAGS) -o $@ $(SHOBJS) $(THREADLIB)
${DVDREAD_SHLIB}: version.h $(.OBJDIR) $(DVDREAD_SHOBJS) $(BUILDDEPS)
	cd $(.OBJDIR) && $(CC) $(SHLDFLAGS) -ldl -o $@ $(DVDREAD_SHOBJS)

.c.so:	$(BUILDDEPS)
	cd $(.OBJDIR) && $(CC) -fPIC -DPIC -MD $(CFLAGS) -c -o $@ $<

.c.o:	$(BUILDDEPS)
	cd $(.OBJDIR) && $(CC) -MD $(CFLAGS) -c -o $@ $<


# Install targets

install-headers:
	install -d $(incdir)
	install -m 644 $(HEADERS) $(incdir)
	install -d $(dvdread_incdir)
	install -m 644 $(DVDREAD_HEADERS) $(dvdread_incdir)

install-shared: $(SHLIB)
	install -d $(shlibdir)

	install $(INSTALLSTRIP) -m 755 $(.OBJDIR)/$(SHLIB) \
		$(shlibdir)/$(SHLIB).$(SHLIB_VERSION)
	install $(INSTALLSTRIP) -m 755 $(.OBJDIR)/$(MINI_SHLIB) \
		$(shlibdir)/$(MINI_SHLIB).$(SHLIB_VERSION)
	install $(INSTALLSTRIP) -m 755 $(.OBJDIR)/$(DVDREAD_SHLIB) \
		$(shlibdir)/$(DVDREAD_SHLIB).$(SHLIB_VERSION)

	cd $(shlibdir) && \
		ln -sf $(SHLIB).$(SHLIB_VERSION) $(SHLIB).$(SHLIB_MAJOR)
	cd $(shlibdir) && \
		ln -sf $(MINI_SHLIB).$(SHLIB_VERSION) $(MINI_SHLIB).$(SHLIB_MAJOR)
	cd $(shlibdir) && \
		ln -sf $(DVDREAD_SHLIB).$(SHLIB_VERSION) $(DVDREAD_SHLIB).$(SHLIB_MAJOR)
	cd $(shlibdir) && \
		ln -sf $(SHLIB).$(SHLIB_MAJOR) $(SHLIB)
	cd $(shlibdir) && \
		ln -sf $(MINI_SHLIB).$(SHLIB_MAJOR) $(MINI_SHLIB)
	cd $(shlibdir) && \
		ln -sf $(DVDREAD_SHLIB).$(SHLIB_MAJOR) $(DVDREAD_SHLIB)

install-static: $(LIB)
	install -d $(libdir)

	install $(INSTALLSTRIP) -m 755 $(.OBJDIR)/$(LIB) $(libdir)/$(LIB)
	install $(INSTALLSTRIP) -m 755 $(.OBJDIR)/$(DVDREAD_LIB) $(libdir)/$(DVDREAD_LIB)


# Clean targets

clean:
	rm -rf  *~ $(.OBJDIR) version.h


distclean: clean
	find . -name "*~" | xargs rm -rf
	rm -rf config.mak

dvdnav-config: $(.OBJDIR)
	echo -e '#!/bin/sh\n\nprefix='$(PREFIX)'\n' > $(.OBJDIR)/dvdnav-config
	echo -e 'version='$(SHLIB_VERSION)'\n' >> $(.OBJDIR)/dvdnav-config
	echo -e 'dvdread='$(DVDREAD)'\n' >> $(.OBJDIR)/dvdnav-config
	echo -e 'dvdreaddir='$(DVDREAD_DIR)'\n' >> $(.OBJDIR)/dvdnav-config
	echo -e 'threadlib='$(THREADLIB)'\n\n' >> $(.OBJDIR)/dvdnav-config
	cat $(SRC_PATH_BARE)/misc/dvdnav-config2.sh >> $(.OBJDIR)/dvdnav-config
	chmod 0755 $(SRC_PATH_BARE)/$(.OBJDIR)/dvdnav-config

install-dvdnav-config: dvdnav-config
	install -d $(PREFIX)/bin
	install -m 0755 $(.OBJDIR)/dvdnav-config $(PREFIX)/bin/dvdnav-config


vpath %.so ${.OBJDIR}
vpath %.o ${.OBJDIR}
vpath ${LIB} ${.OBJDIR}

# include dependency files if they exist
$(addprefix ${.OBJDIR}/, ${DEPS}): ;
-include $(addprefix ${.OBJDIR}/, ${DEPS})
