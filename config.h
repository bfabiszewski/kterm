/* config.h
 *
 * Copyright (C) 2013-2016 Bartek Fabiszewski (www.fabiszewski.net)
 *
 * This is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Library General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef config_h
#define config_h

#include <gtk/gtk.h>

/** Kterm version */
#ifndef VERSION
# define VERSION "2.0"
#endif
#ifdef KINDLE
/** Kindle title scheme */
# define TITLE "L:A_N:application_ID:net.fabiszewski.kterm_PC:N_O:URL"
# define TITLE_DIALOG "L:D_N:application_ID:net.fabiszewski.kterm_O:URL"
/** Mouse button to open popup menu */
// middle button on kindle
# define BUTTON_MENU 2
/** Terminfo path */
# define TERMINFO_PATH SYSCONFDIR "/vte/terminfo"
#else
/** Window title */
# define TITLE "kterm " VERSION
/** Mouse button to open popup menu */
# define BUTTON_MENU 3
#endif
/** Sysconf path */
#ifndef SYSCONFDIR
# define SYSCONFDIR "/etc/local"
#endif
/** Default keyboard config path */
#define KB_FULL_PATH SYSCONFDIR "/layouts/keyboard.xml"
/** Keyboard max factor. Keyboard takes at most 1/3 of the screen height */
#define KB_HEIGHTMAX_FACTOR 3
/** mm to inch conversion multiplier */
#define MM_TO_IN 0.0393701
/** Preferred key button height in inches */
#define KB_KEYHEIGHT_PREF (double) (8 * MM_TO_IN)
/** Config file name */
#define CONFIG_FILE "kterm.conf"
/** Default config file path */
#define CONFIG_FULL_PATH SYSCONFDIR "/kterm/" CONFIG_FILE

/** Resize font up */
#define FONT_UP 0
/** Resize font down */
#define FONT_DOWN 1

#ifndef MAX
/** MAX macro */
# define MAX(a, b)  (((a) > (b)) ? (a) : (b))
#endif

#ifndef PATH_MAX
/** Path max */
# define PATH_MAX 4096
#endif

/** Max count of arguments passed to terminal */
#define TERM_ARGS_MAX 50

/** Delay for key release event */
#define KB_RELEASE_DELAY_MS 100

/** Terminal scrollback size */
#define VTE_SCROLLBACK_LINES 200
/** Default terminal font family */
#define VTE_FONT_FAMILY "monospace"
/** Default terminal font size */
#define VTE_FONT_SIZE 8
/** Default terminal encoding */
#define VTE_ENCODING "UTF-8"
/** Terminal color scheme normal */
#define VTE_SCHEME_LIGHT FALSE
/** Terminal color scheme reversed */
#define VTE_SCHEME_DARK TRUE

/** Debug */
extern gboolean debug;
#define D if(debug)

/** Unused variable */
#define UNUSED(x) (void)(x)

/** Kterm config */
typedef struct {
    gboolean kb_on; /** Keyboard visibility */
    gboolean color_reversed; /** Color scheme, is reversed */
    gchar font_family[50]; /** Terminal font family */
    guint font_size;  /** Terminal font size */
    gchar kb_conf_path[PATH_MAX];  /** Keyboard config path */
    gchar orientation;  /** Screen orientation: 'U', 'R' or 'L' */
    gchar orientation_saved;  /** Initial screen orientation: 'U', 'R' or 'L' */
} KTconf;

KTconf * parse_config(void);

#endif
