bin_PROGRAMS = kterm
kterm_SOURCES = keyboard.c kterm.c parse_config.c parse_layout.c

ktermconfdir = $(sysconfdir)/kterm
ktermconf_DATA = kterm.conf
layoutsdir = $(ktermconfdir)/layouts
layouts_DATA = layouts/keyboard.xml
layoutsimgdir = $(layoutsdir)/img
layoutsimg_DATA = layouts/img/*.png

kterm_CFLAGS = @GTK_CFLAGS@ @VTE_CFLAGS@ @KINDLE_CFLAGS@ @GDK_X11_CFLAGS@ -DSYSCONFDIR=\"${sysconfdir}\" 
kterm_LDADD = @GTK_LIBS@ @VTE_LIBS@ @GDK_X11_LIBS@

AM_CPPFLAGS = -pedantic -Wall -Wextra
