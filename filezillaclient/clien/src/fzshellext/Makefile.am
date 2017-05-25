ACLOCAL_AMFLAGS = -I ../../m4

CLEANFILES = fzshellext.o

lib_LTLIBRARIES = libfzshellext.la

libfzshellext_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(builddir)/..
libfzshellext_la_SOURCES = shellext.cpp
libfzshellext_la_CXXFLAGS = $(AM_CXXFLAGS) -fno-exceptions -fno-rtti

if USE_RESOURCEFILE
RESOURCEFILE = -Wl,fzshellext.o
libfzshellext_la_DEPENCENCIES = fzshellext.o
endif

# Due to a braindead Libtool, we _NEED_ to use -Wl to link with some win32 system libraries
libfzshellext_la_LDFLAGS = -Wl,--kill-at -Wl,-lole32 -Wl,-luuid -Wl,-lversion -no-undefined -version-info 0:0:0 $(RESOURCEFILE)

noinst_HEADERS = shellext.h resource.h

dist_noinst_DATA = fzshellext.def fzshellext.vcxproj fzshellext.rc version.rc.in

if USE_RESOURCEFILE

fzshellext.o: $(srcdir)/fzshellext.rc
	@if [ -f $@~ ]; then rm $@~; fi
	@WINDRES@ --include-dir=.. --include-dir=$(srcdir) $(WINDRESFLAGS) $(srcdir)/fzshellext.rc -D HAS_VERSIONRC $@~ || rm $@~
	@if [ -f $@ ]; then rm $@; fi
	mv $@~ $@

BUILT_SOURCES = fzshellext.o

endif
