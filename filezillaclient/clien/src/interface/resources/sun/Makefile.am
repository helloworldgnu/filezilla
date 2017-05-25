resourcesdir = $(pkgdatadir)/resources/sun
resources48dir = $(pkgdatadir)/resources/sun/48x48

dist_resources_DATA = theme.xml

dist_resources48_DATA = 48x48/ascii.png \
		48x48/auto.png \
		48x48/binary.png \
		48x48/bookmark.png \
		48x48/cancel.png \
		48x48/compare.png \
		48x48/disconnect.png \
		48x48/download.png \
		48x48/downloadadd.png \
		48x48/file.png \
		48x48/filter.png \
		48x48/find.png \
		48x48/folder.png \
		48x48/folderback.png \
		48x48/folderclosed.png \
		48x48/folderup.png \
		48x48/help.png \
		48x48/localtreeview.png \
		48x48/lock.png \
		48x48/logview.png \
		48x48/processqueue.png \
		48x48/queueview.png \
		48x48/reconnect.png \
		48x48/refresh.png \
		48x48/remotetreeview.png \
		48x48/showhidden.png \
		48x48/sitemanager.png \
		48x48/speedlimits.png \
		48x48/synchronize.png \
		48x48/upload.png \
		48x48/uploadadd.png

if MACAPPBUNDLE
mac-copy-resources: $(dist_resources_DATA)
	mkdir -p "$(top_builddir)/FileZilla.app/Contents/SharedSupport/resources/sun/48x48";
	for i in $(dist_resources48_DATA); do \
	  cp -f "$(srcdir)/$$i" "$(top_builddir)/FileZilla.app/Contents/SharedSupport/resources/sun/48x48"; \
	done
	for i in $(dist_resources_DATA); do \
	  cp -f "$(srcdir)/$$i" "$(top_builddir)/FileZilla.app/Contents/SharedSupport/resources/sun"; \
	done
endif

