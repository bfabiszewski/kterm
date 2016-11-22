AC_INIT([kterm], [2.3])
AM_INIT_AUTOMAKE([foreign])
AM_PROG_CC_C_O
AC_PROG_CC_C99
AC_PROG_INSTALL
AC_CONFIG_FILES([Makefile])
AC_CONFIG_MACRO_DIR([m4])

AC_ARG_ENABLE([kindle],
  [AS_HELP_STRING([--enable-kindle], [compile for Kindle [default=no]])],
  [enable_kindle=$enableval], [enable_kindle=no])

AM_CONDITIONAL([KINDLE], [test "x$enable_kindle" = "xyes"])

AC_ARG_ENABLE([gtk3],
  [AS_HELP_STRING([--enable-gtk3], [compile with GTK+ version 3 [default=no]])],
  [enable_gtk3=$enableval], [enable_gtk3=no])

AM_CONDITIONAL([GTK3], [test "x$enable_gtk3" = "xyes"])

AM_COND_IF([GTK3],
  [gtk_package=gtk+-3.0],
  [gtk_package=gtk+-2.0])

PKG_CHECK_MODULES([GTK], [$gtk_package])

AM_COND_IF([KINDLE],
[
  AC_MSG_CHECKING([whether compiler supports ARM target])
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [return __arm__])],
    [AC_MSG_RESULT([yes])],
    [AC_MSG_RESULT([no])
     AC_MSG_WARN([building for Kindle without ARM toolchain])])

  dnl Libtermcap is needed for static builds with vte < 2.91
  require_termcap=yes
  AC_DEFINE([KINDLE], [1])

  saved_PKG_CONFIG=$PKG_CONFIG
  PKG_CONFIG="$PKG_CONFIG --static"
  AM_COND_IF([GTK3],
    [PKG_CHECK_MODULES([VTE], [vte-2.91], [require_termcap=no], [PKG_CHECK_MODULES([VTE], [vte-2.90])])],
    [PKG_CHECK_MODULES([VTE], [vte])])
  PKG_CONFIG=$saved_PKG_CONFIG

  AM_CONDITIONAL([TERMCAP_NEEDED], [test "x$require_termcap" = "xyes"])
  TERMCAP_LIBS=
  AM_COND_IF([TERMCAP_NEEDED],
    [saved_LIBS=$LIBS
     LIBS=
     AC_SEARCH_LIBS([tgetent],[curses ncurses readline termcap],
       [TERMCAP_LIBS=$LIBS],
       [AC_MSG_WARN([libtermcap is needed for static build])])
     LIBS=$saved_LIBS
    ],[])

  AC_MSG_CHECKING([whether libvte may be linked statically])
  dnl This will only work with gnu linker
  [VTE_STATIC_LIBS=`echo "-Wl,--as-needed $VTE_LIBS -Wl,--no-as-needed" | sed -e "s/\(-lvte[-_.0-9]*\)/-Wl,-Bstatic \1 -Wl,-Bdynamic $TERMCAP_LIBS/g"`]
  saved_CFLAGS=$CFLAGS
  saved_LIBS=$LIBS
  CFLAGS="$GTK_CFLAGS $VTE_CFLAGS"
  LIBS="$GTK_LIBS $VTE_STATIC_LIBS"
  AC_LINK_IFELSE(
    [AC_LANG_PROGRAM([[#include <vte/vte.h>]],[[vte_terminal_new();]])],
    [AC_MSG_RESULT([yes])
     VTE_LIBS=$VTE_STATIC_LIBS],
    [AC_MSG_RESULT([no])
     AC_MSG_WARN([building for Kindle with non-static libvte])])
  LIBS=$saved_LIBS
  CFLAGS=$saved_CFLAGS

  PKG_CHECK_MODULES([DBUS], [dbus-1], [AC_DEFINE([HAVE_DBUS], [1])], [AC_MSG_WARN([screen rotation on Kindle will be disabled without dbus])])
],[
  AM_COND_IF([GTK3],
    [PKG_CHECK_MODULES([VTE], [vte-2.91], [], [PKG_CHECK_MODULES([VTE], [vte-2.90])])],
    [PKG_CHECK_MODULES([VTE], [vte])])
])

AC_SUBST([GTK_CFLAGS])
AC_SUBST([GTK_LIBS])
AC_SUBST([VTE_CFLAGS])
AC_SUBST([VTE_LIBS])
AC_SUBST([GDK_X11_CFLAGS])
AC_SUBST([GDK_X11_LIBS])
AC_SUBST([DBUS_LIBS])
AC_SUBST([DBUS_CFLAGS])

AC_OUTPUT
