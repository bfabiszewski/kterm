/* config.h
 *
 * Copyright (C) 2013 Bartek Fabiszewski (www.fabiszewski.net)
 *
 * This is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Library General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
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
 
#define VERSION "0.3"
// kindle title scheme
#define TITLE "L:A_N:application_ID:net.fabiszewski.kterm_PC:N_O:U" 
// matchbox keyboard binary
#define KB_FULL_PATH "/usr/bin/matchbox-keyboard"
#define KB_BINARY "matchbox-keyboard"
#define KB_ARGS " --xid"
// matchbox keyboard config path
#define MB_KBD_FULL_PATH "/usr/share/matchbox-keyboard/layouts/keyboard.xml"
#define MB_KBD_CONFIG "../layouts/keyboard.xml"
// matchbox-keyboard takes 1/3 of the screen height
#define KB_HEIGHT_FACTOR 3.05
// config file
#define CONFIG_FULL_PATH "/etc/kterm/kterm.conf"
#define CONFIG_FILE "kterm.conf"
// resize font
#define FONT_UP 0
#define FONT_DOWN 1
// boxes
#define KB_BOX 0
#define TERM_BOX 1

// terminal scrollback size
#define VTE_SCROLLBACK_LINES 200
// default font
#define VTE_FONT_FAMILY "monospace"
#define VTE_FONT_SIZE 8
// default encoding
#define VTE_ENCODING "UTF-8"
// terminal color schemes
#define VTE_SCHEME_LIGHT 0
#define VTE_SCHEME_DARK 1

// menu button
#define MENU_BUTTON_SIZE 20

extern unsigned int debug;
#define D if(debug) 

// parse_config
typedef struct {
  unsigned int kb_on;
  unsigned int color_scheme;
  char font_family[50];
  unsigned int font_size;
} kterm_conf;

kterm_conf *parse_config();

