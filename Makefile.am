bin_PROGRAMS = kterm
kterm_SOURCES = keyboard.c kterm.c parse_config.c parse_layout.c
if KINDLE
kterm_SOURCES += kindle.c
endif

ktermconfdir = $(sysconfdir)/kterm
ktermconf_DATA = kterm.conf
layoutsdir = $(ktermconfdir)/layouts
layouts_DATA = layouts/keyboard.xml
layoutsimgdir = $(layoutsdir)/img
layoutsimg_DATA = layouts/img/*.png

kterm_CFLAGS = @GTK_CFLAGS@ @VTE_CFLAGS@ @GDK_X11_CFLAGS@ @DBUS_CFLAGS@ -DSYSCONFDIR=\"$(sysconfdir)\"
kterm_LDADD = @GTK_LIBS@ @VTE_LIBS@ @GDK_X11_LIBS@ @DBUS_LIBS@

AM_CPPFLAGS = -pedantic -Wall -Wextra

kterm_pkg = $(abs_builddir)/kterm-kindle-$(VERSION).zip

dist-kindle: kterm
	rm -f $(kterm_pkg)
	KTERM_TMPDIR=`mktemp -d` \
	&& mkdir $${KTERM_TMPDIR}/kterm \
	&& cp -RL kindle.pkg/* $${KTERM_TMPDIR}/kterm \
	&& cp kterm $${KTERM_TMPDIR}/kterm/bin \
	&& cd $${KTERM_TMPDIR} \
	&& sed -i'.bak' 's/@VER@/$(VERSION)/' kterm/config.xml \
	&& rm kterm/config.xml.bak \
	&& zip -qr $(kterm_pkg) kterm \
	&& cd - \
	&& rm -rf $${KTERM_TMPDIR}

distclean-local:
	-rm -f $(kterm_pkg)
