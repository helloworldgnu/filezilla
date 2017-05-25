noinst_LIBRARIES = libengine.a

libengine_a_CPPFLAGS = -I$(srcdir)/../include
libengine_a_CPPFLAGS += $(LIBGNUTLS_CFLAGS) $(WX_CPPFLAGS)
libengine_a_CXXFLAGS = $(WX_CXXFLAGS_ONLY)
libengine_a_CFLAGS = $(WX_CFLAGS_ONLY)

libengine_a_SOURCES = \
		backend.cpp \
		commands.cpp \
		ControlSocket.cpp \
		directorycache.cpp \
		directorylisting.cpp \
		directorylistingparser.cpp \
		engine_context.cpp \
		engineprivate.cpp \
		event_handler.cpp \
		event_loop.cpp \
		externalipresolver.cpp \
		FileZillaEngine.cpp \
		file.cpp \
		ftpcontrolsocket.cpp \
		httpcontrolsocket.cpp \
		iothread.cpp \
		local_filesys.cpp \
		local_path.cpp \
		logging.cpp \
		misc.cpp \
		msgbox.cpp \
		mutex.cpp \
		notification.cpp \
		option_change_event_handler.cpp \
		pathcache.cpp \
		process.cpp \
		proxy.cpp \
		ratelimiter.cpp \
		rtt.cpp \
		server.cpp serverpath.cpp\
		servercapabilities.cpp \
		sftpcontrolsocket.cpp \
		sizeformatting_base.cpp \
		socket.cpp \
		tlssocket.cpp \
		timeex.cpp \
		transfersocket.cpp

noinst_HEADERS = backend.h \
		ControlSocket.h \
		directorycache.h \
		directorylistingparser.h \
		engineprivate.h \
		filezilla.h \
		ftpcontrolsocket.h \
		httpcontrolsocket.h iothread.h \
		logging_private.h \
		pathcache.h \
		process.h \
		proxy.h \
		ratelimiter.h \
		rtt.h \
		servercapabilities.h \
		sftpcontrolsocket.h \
		tlssocket.h \
		transfersocket.h

dist_noinst_DATA = engine.vcxproj

CLEANFILES = filezilla.h.gch

if ENABLE_PRECOMP

BUILT_SOURCES = filezilla.h.gch

include ./$(DEPDIR)/filezilla.Po

filezilla.h.gch: filezilla.h
	if $(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(libengine_a_CPPFLAGS) $(CPPFLAGS) $(libengine_a_CXXFLAGS) $(CXXFLAGS) -MT $@ -MD -MP -MF "$(DEPDIR)/$*.Tpo" -c -o $@ $<; \
	then mv -f "$(DEPDIR)/$*.Tpo" "$(DEPDIR)/$*.Po"; else rm -f "$(DEPDIR)/$*.Tpo"; exit 1; fi

endif

