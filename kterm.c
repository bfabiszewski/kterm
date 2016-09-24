/* kterm.c
 *
 * This file is part of kterm
 *
 * Copyright(C) 2013-16 Bartek Fabiszewski (www.fabiszewski.net)
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

#define _XOPEN_SOURCE 500
#define _POSIX_SOURCE 1
#include <gtk/gtk.h>
#ifdef KINDLE
#include <gdk/gdkx.h>
#endif
#include <vte/vte.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <getopt.h>
#include "keyboard.h"
#include "config.h"

#ifndef VTE_CHECK_VERSION
#define VTE_CHECK_VERSION(x,y,z) FALSE
#endif

KTconf *conf;
unsigned int debug = FALSE;

static void exit_on_signal(int signo) {
    D printf("exiting on signal %i\n", signo);
    if (gtk_main_level()) {
        gtk_main_quit();
    } else {
        exit(0);
    }
}

static void clean_on_exit(Keyboard *keyboard) {
    D printf("cleanup\n");
#ifdef KINDLE
    XUngrabKeyboard(GDK_DISPLAY(), CurrentTime);
#endif
    keyboard_free(keyboard);
    g_free(conf);
}

static void terminal_exit(void) {
    sleep(1); // time for kb to send key up event
    gtk_main_quit();
}

static void install_signal_handlers() {
    signal(SIGCHLD, SIG_IGN); // kernel should handle zombies
    signal(SIGINT, exit_on_signal);
    signal(SIGQUIT, exit_on_signal);
    signal(SIGTERM, exit_on_signal);
}

static void set_terminal_font(VteTerminal *terminal, const gchar *font_family, gint font_size) {
    gchar font_name[200];
    snprintf(font_name, sizeof(font_name), "%s %i", font_family, font_size);
    D printf("font_name: %s\n", font_name);
    PangoFontDescription *desc = pango_font_description_from_string(font_name);
    vte_terminal_set_font(VTE_TERMINAL(terminal), desc);
    pango_font_description_free(desc);
}

static void resize_font(VteTerminal *terminal, guint mod) {
    const PangoFontDescription *pango_desc = vte_terminal_get_font(VTE_TERMINAL(terminal));
    gchar *pango_name = pango_font_description_to_string(pango_desc);
    gchar *size_offset = strrchr(pango_name, ' ');
    if (size_offset) {
        *size_offset = '\0';
        gint font_size = atoi(++size_offset);
        D printf("font_family: %s\n", pango_name);
        D printf("font_size: %i\n", font_size);
        if (mod == FONT_UP) {
            font_size++;
        }
        else if (font_size > 1) {
            font_size--;
        }
        set_terminal_font(terminal, pango_name, font_size);
    }
    g_free(pango_name);
}

static void fontup(GtkWidget *widget, gpointer terminal) {
    UNUSED(widget);
    resize_font(terminal, FONT_UP);
}

static void fontdown(GtkWidget *widget, gpointer terminal) {
    UNUSED(widget);
    resize_font(terminal, FONT_DOWN);
}

static void set_terminal_colors(GtkWidget *terminal, unsigned int scheme) {
#if GTK_CHECK_VERSION(3,14,0)
    // light background
    GdkRGBA palette_light[8] = {{ 0, 0, 0, 1 }, // black
        { 0x5050/0xffff, 0x5050/0xffff, 0x5050/0xffff, 1 }, // red
        { 0x7070/0xffff, 0x7070/0xffff, 0x7070/0xffff, 1 }, // green
        { 0xa0a0/0xffff, 0xa0a0/0xffff, 0xa0a0/0xffff, 1 }, // yellow
        { 0x8080/0xffff, 0x8080/0xffff, 0x8080/0xffff, 1 }, // blue
        { 0x3030/0xffff, 0x3030/0xffff, 0x3030/0xffff, 1 }, // magenta
        { 0x9090/0xffff, 0x9090/0xffff, 0x9090/0xffff, 1 }, // cyan
        { 1, 1, 1, 1 }}; // white
    // dark background
    GdkRGBA palette_dark[8] = {{ 0, 0, 0, 1 },
        { 0x8888/0xffff, 0x8888/0xffff, 0x8888/0xffff, 1 },
        { 0x9898/0xffff, 0x9898/0xffff, 0x9898/0xffff, 1 },
        { 0xd0d0/0xffff, 0xd0d0/0xffff, 0xd0d0/0xffff, 1 },
        { 0xa8a8/0xffff, 0xa8a8/0xffff, 0xa8a8/0xffff, 1 },
        { 0x7070/0xffff, 0x7070/0xffff, 0x7070/0xffff, 1 },
        { 0xb8b8/0xffff, 0xb8b8/0xffff, 0xb8b8/0xffff, 1 },
        { 1, 1, 1, 1 }};
    GdkRGBA *palette;
    GdkRGBA color_white = { 1, 1, 1, 1 };
    GdkRGBA color_black = { 0, 0, 0, 1 };
    GdkRGBA color_bg, color_fg;
#else
    // light background
    GdkColor palette_light[8] = {{ 0, 0x0000, 0x0000, 0x0000 },
        { 0, 0x5050, 0x5050, 0x5050 },
        { 0, 0x7070, 0x7070, 0x7070 },
        { 0, 0xa0a0, 0xa0a0, 0xa0a0 },
        { 0, 0x8080, 0x8080, 0x8080 },
        { 0, 0x3030, 0x3030, 0x3030 },
        { 0, 0x9090, 0x9090, 0x9090 },
        { 0, 0xffff, 0xffff, 0xffff }};
    // dark background
    GdkColor palette_dark[8] = {{ 0, 0x0000, 0x0000, 0x0000 },
        { 0, 0x8888, 0x8888, 0x8888 },
        { 0, 0x9898, 0x9898, 0x9898 },
        { 0, 0xd0d0, 0xd0d0, 0xd0d0 },
        { 0, 0xa8a8, 0xa8a8, 0xa8a8 },
        { 0, 0x7070, 0x7070, 0x7070 },
        { 0, 0xb8b8, 0xb8b8, 0xb8b8 },
        { 0, 0xffff, 0xffff, 0xffff }};
    GdkColor *palette;
    GdkColor color_white = { 0, 0xffff, 0xffff, 0xffff };
    GdkColor color_black = { 0, 0x0000, 0x0000, 0x0000 };
    GdkColor color_bg, color_fg;
    GdkColor color_dim = { 0, 0x8888, 0x8888, 0x8888 };
#endif
    switch(scheme) {
        default:
        case VTE_SCHEME_LIGHT:
            palette = palette_light;
            color_bg = color_white;
            color_fg = color_black;
            break;
        case VTE_SCHEME_DARK:
            palette = palette_dark;
            color_bg = color_black;
            color_fg = color_white;
            break;
    }
    vte_terminal_set_colors(VTE_TERMINAL(terminal), NULL, NULL, palette, 8);
    vte_terminal_set_color_background(VTE_TERMINAL(terminal), &color_bg);
    vte_terminal_set_color_foreground(VTE_TERMINAL(terminal), &color_fg);
#if !GTK_CHECK_VERSION(3,14,0)
    vte_terminal_set_color_dim(VTE_TERMINAL(terminal), &color_dim);
#endif
    vte_terminal_set_color_bold(VTE_TERMINAL(terminal), &color_fg);
    vte_terminal_set_color_cursor(VTE_TERMINAL(terminal), NULL);
    vte_terminal_set_color_highlight(VTE_TERMINAL(terminal), NULL);
    conf->color_scheme = scheme;
}

static void reverse_colors(GtkWidget *widget, gpointer terminal) {
    UNUSED(widget);
    set_terminal_colors(terminal, conf->color_scheme^1);
}

static gboolean set_box_size(GtkWidget *box, GdkEvent *event, void *data) {
    UNUSED(box);
    GdkScreen *screen = gdk_screen_get_default();
    gint screen_height = gdk_screen_get_height(screen);
    gint screen_width = gdk_screen_get_width(screen);
    gint box_height = (event) ? ((GdkEventConfigure *) event)->height : 0;
    gint box_width = (event) ? ((GdkEventConfigure *) event)->width : 0;
    gint kb_height = (conf->kb_on) ? (int) (screen_height/KB_HEIGHT_FACTOR) : 0;
    D printf("box size: %ix%i\n", box_width, box_height);
    static gint saved_width = -1;
    if (box_width != saved_width) {
        gtk_widget_set_size_request(box, screen_width, kb_height);
        Keyboard *keyboard = data;
        if (keyboard) {
            keyboard_set_widths(keyboard);
        }
        saved_width = box_width;
    }
    return FALSE;
}

#ifdef KINDLE
static void lipc_rotate() {
    GdkScreen *screen = gdk_screen_get_default();
    gint screen_height = gdk_screen_get_height(screen);
    gint screen_width = gdk_screen_get_width(screen);
    const char *command;
    if (screen_width > screen_height) {
        command = "/usr/bin/lipc-set-prop com.lab126.winmgr orientationLock U";
    } else {
        command = "/usr/bin/lipc-set-prop com.lab126.winmgr orientationLock R";
    }
    switch(fork()) {
        case 0:
        {
            execlp("/bin/sh", "sh", "-c", command, NULL);
        }
        case -1:
            perror("Failed to run /usr/bin/lipc-set-prop com.lab126.winmgr\n");
            exit(1);
    }
}

static void screen_rotate(GtkWidget *widget, gpointer box) {
    UNUSED(widget);
    const char *box_name;
    D box_name  = gtk_widget_get_name(box);
    D printf("box: %s\n", box_name);
    // call lipc
    lipc_rotate();
}
#endif

static void reset_terminal(GtkWidget *widget, gpointer terminal) {
    UNUSED(widget);
    vte_terminal_reset(terminal, TRUE, TRUE);
}

static void toggle_keyboard(GtkWidget *widget, gpointer box) {
    UNUSED(widget);
    GtkWidget *keyboard_box = NULL;
    GList *box_list = gtk_container_get_children(GTK_CONTAINER(box));
    for (GList *cur = box_list; cur != NULL; cur = cur->next) {
        const char *box_name = gtk_widget_get_name(GTK_WIDGET(cur->data));
        D printf("box: %s\n", box_name);
        if (!strncmp(box_name, "kbBox", 5)) { keyboard_box = GTK_WIDGET(cur->data); }
    }
    if (keyboard_box) {
        if (conf->kb_on) {
            gtk_widget_hide(keyboard_box);
            conf->kb_on = FALSE;
        } else {
            gtk_widget_show(keyboard_box);
            conf->kb_on = TRUE;
        }
    }
    g_list_free(box_list);
}

static gboolean button_event(GtkWidget *terminal, GdkEventButton *event, gpointer box) {
    D printf("event-type: %i\n", event->type);
    D printf("event-button: %i\n", event->button);
#ifdef KINDLE
    // ignore any motion events (i think we don't need selecting text as one finger motion scrolls buffer)
    if (event->type == GDK_MOTION_NOTIFY) return TRUE;
#endif
    if (event->button == BUTTON_MENU) {
#ifdef KINDLE
        // ignore right button click to disable paste (quite messy on kindle)
        if (event->type == GDK_BUTTON_PRESS) return TRUE;
#endif
        // popup menu on button release
        GtkWidget *menu = gtk_menu_new();
        GtkWidget *fontup_item = gtk_menu_item_new_with_label("Font increase");
        GtkWidget *fontdown_item = gtk_menu_item_new_with_label("Font decrease");
        GtkWidget *color_item = gtk_menu_item_new_with_label("Reverse colors");
        GtkWidget *kb_item = gtk_menu_item_new_with_label("Toggle keyboard");
        GtkWidget *reset_item = gtk_menu_item_new_with_label("Reset terminal");
#ifdef KINDLE
        GtkWidget *rotate_item = gtk_menu_item_new_with_label("Screen rotate");
#endif
        GtkWidget *quit_item = gtk_menu_item_new_with_label("Quit");
        
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), fontup_item);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), fontdown_item);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), color_item);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), kb_item);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), reset_item);
#ifdef KINDLE
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), rotate_item);
#endif
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), quit_item);


        g_signal_connect(G_OBJECT(fontup_item), "activate", G_CALLBACK(fontup), (gpointer) terminal);
        g_signal_connect(G_OBJECT(fontdown_item), "activate", G_CALLBACK(fontdown), (gpointer) terminal);
        g_signal_connect(G_OBJECT(color_item), "activate", G_CALLBACK(reverse_colors), (gpointer) terminal);
        g_signal_connect(G_OBJECT(kb_item), "activate", G_CALLBACK(toggle_keyboard), box);
        g_signal_connect(G_OBJECT(reset_item), "activate", G_CALLBACK(reset_terminal), (gpointer) terminal);
#ifdef KINDLE
        g_signal_connect(G_OBJECT(rotate_item), "activate", G_CALLBACK(screen_rotate), box);
#endif
        g_signal_connect(G_OBJECT(quit_item), "activate", G_CALLBACK(gtk_main_quit), NULL);
        
        gtk_widget_show_all(menu);
        
        GdkEventButton *bevent = (GdkEventButton *) event;
        gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, bevent->button, bevent->time);
        return TRUE;
    }
    return FALSE;
}

#ifdef KINDLE
static void inject_gtkrc(void) {
    gtk_rc_parse_string
    ("gtk_color_scheme = \"white: #ffffff\ngray: #f0f0f0\""
     "style \"kterm-style\" {"
     " bg[NORMAL] = @gray"
     " bg[PRELIGHT] = @gray"
     " bg[INSENSITIVE] = @gray"
     " bg[ACTIVE] = @gray"
     " bg[SELECTED] = @gray"
     "}"
     "widget \"*ktermKbButton\" style : lowest \"kterm-style\"");
}
#endif

static void version(void) {
    printf("kterm %s (vte %i.%i.%i, gtk+ %i.%i.%i)\n",
           VERSION, VTE_MAJOR_VERSION, VTE_MINOR_VERSION, VTE_MICRO_VERSION,
           GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION);
    exit(0);
}

static void usage(void) {
    printf("Usage: kterm [OPTIONS]\n");
    printf("        -c <0|1>     - color scheme (0 light, 1 dark)\n");
    printf("        -d           - debug mode\n");
    printf("        -e <command> - execute command in kterm\n");
    printf("        -E <var>     - set environment variable\n");
    printf("        -f <family>  - font family\n");
    printf("        -h           - show this message\n");
    printf("        -k <0|1>     - keyboard off/on\n");
    printf("        -l <path>    - keyboard layout config path\n");
    printf("        -s <size>    - font size\n");
    printf("        -v           - print version and exit\n");
    exit(0);
}

static gboolean setup_terminal(GtkWidget *terminal, gchar *command, gchar **envv) {
    gboolean ret = TRUE;
    gchar *argv[TERM_ARGS_MAX] = { NULL };
    gint argc = 0;
    gchar *shell = NULL;
#if VTE_CHECK_VERSION(0,25,1)
    if (!command) {
        // prepend args with shell
        if ((shell = vte_get_user_shell()) == NULL) {
            shell = g_strdup("/bin/sh");
        }
        argv[argc++] = shell;
    }
#endif
    if (command) {
        gchar *argbuf = strtok(command, " ");
        while (argbuf != NULL && argc < (TERM_ARGS_MAX - 1)) {
            argv[argc++] = argbuf;
            argbuf = strtok(NULL, " ");
        }
    }

    set_terminal_colors(terminal, conf->color_scheme);
    vte_terminal_set_scrollback_lines(VTE_TERMINAL(terminal), VTE_SCROLLBACK_LINES);
    set_terminal_font(VTE_TERMINAL(terminal), conf->font_family, (int) conf->font_size);
#if VTE_CHECK_VERSION(0,38,0)
    vte_terminal_set_encoding(VTE_TERMINAL(terminal), VTE_ENCODING, NULL);
#else
    vte_terminal_set_encoding(VTE_TERMINAL(terminal), VTE_ENCODING);
#endif
    vte_terminal_set_allow_bold(VTE_TERMINAL(terminal), TRUE);
    
#if VTE_CHECK_VERSION(0,38,0)
    ret = vte_terminal_spawn_sync(VTE_TERMINAL(terminal), 0, NULL, argv, envv, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, NULL);
#elif VTE_CHECK_VERSION(0,25,1)
    ret = vte_terminal_fork_command_full(VTE_TERMINAL(terminal), 0, NULL, argv, envv, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
#else
    ret = vte_terminal_fork_command(VTE_TERMINAL(terminal), argv[0], (argv[0] ? argv : NULL), envv, NULL, FALSE, FALSE, FALSE);
#endif
    if (shell) { g_free(shell); }
    return ret;
}

int main(int argc, char **argv) {
    
    conf = parse_config(); // call first so args overide defaults/config
    
    gint c = -1;
    gint i = 0;
    gchar *command = NULL;
    gchar *envv[TERM_ARGS_MAX] = { NULL };
    gint envc = 0;
#ifdef KINDLE
    // set short prompt
    envv[envc++] = "PS1=[\\W]\\$ ";
#endif
    while((c = getopt(argc, argv, "c:de:E:f:hk:l:s:v")) != -1) {
        switch(c) {
            case 'd':
                debug = TRUE;
                break;
            case 'e':
                command = optarg;
                break;
            case 'E':
                if (envc < TERM_ARGS_MAX - 1) {
                    envv[envc++] = optarg;
                }
                break;
            case 'c':
                i = atoi(optarg);
                if ((i == 0) | (i == 1)) conf->color_scheme = (guint) i;
                break;
            case 'k':
                i = atoi(optarg);
                if ((i == 0) | (i == 1)) conf->kb_on = (guint) i;
                break;
            case 'l':
                snprintf(conf->kb_conf_path, sizeof(conf->kb_conf_path), "%s", optarg);
                break;
            case 's':
                i = atoi(optarg);
                if (i > 0) conf->font_size = (guint) i;
                break;
            case 'f':
                snprintf(conf->font_family, sizeof(conf->font_family), "%s", optarg);
                break;
            case 'h':
                usage();
                break;
            case 'v':
                version();
                break;
        }
    }

#ifdef KINDLE
    inject_gtkrc();
#endif
    gtk_init(&argc, &argv);
    
    install_signal_handlers();
    
    // window
    //  \- vbox
    //      \- terminal  \- keyboard_box
    //
    // main window
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_maximize(GTK_WINDOW(window));
    gtk_window_set_title(GTK_WINDOW(window), TITLE);
#ifdef KINDLE
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
#endif
    // box
#if GTK_CHECK_VERSION(3,2,0)
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
#else
    GtkWidget *vbox = gtk_vbox_new(FALSE, 0);
#endif
    gtk_widget_set_name(vbox, "ktermBox");
    gtk_container_add(GTK_CONTAINER(window), vbox);

#if GTK_CHECK_VERSION(3,0,0)
    GtkWidget *keyboard_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
#else
    GtkWidget *keyboard_box = gtk_vbox_new(FALSE, 0);
#endif
    
    gtk_widget_set_name(keyboard_box, "kbBox");
    set_box_size(keyboard_box, NULL, NULL);
    Keyboard *keyboard = build_layout(keyboard_box);
    gtk_box_pack_end(GTK_BOX(vbox), keyboard_box, FALSE, FALSE, 0);
    
    GtkWidget *terminal = vte_terminal_new();
    if G_UNLIKELY(!setup_terminal(terminal, command, envv)) {
        clean_on_exit(keyboard);
        exit(0);
    }
    gtk_widget_set_name(terminal, "termBox");
    gtk_widget_grab_focus(terminal);
    gtk_box_pack_start(GTK_BOX(vbox), terminal, TRUE, TRUE, 0);
    
    // signals
    g_signal_connect(window, "delete_event", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(terminal, "child-exited", G_CALLBACK(terminal_exit), NULL);
    g_signal_connect(terminal, "button-press-event", G_CALLBACK(button_event), vbox);
    g_signal_connect(terminal, "button-release-event", G_CALLBACK(button_event), vbox);
    g_signal_connect(terminal, "motion-notify-event", G_CALLBACK(button_event), vbox);
    
    gtk_widget_show_all(window);
    keyboard_set_widths(keyboard);
    if G_UNLIKELY(!keyboard) {
        gtk_widget_hide(keyboard_box);
    }
    
    g_signal_connect(window, "configure-event", G_CALLBACK(set_box_size), keyboard);
    
#ifdef KINDLE
    // grab keyboard
    // this is necessary for kindle, because its framework intercepts some keystrokes
    Window root = DefaultRootWindow(GDK_DISPLAY());
    XGrabKeyboard(GDK_DISPLAY(), root, TRUE, GrabModeAsync, GrabModeAsync, CurrentTime);
#endif
    gtk_main();
    
    clean_on_exit(keyboard);
    D printf("the end\n");
    return 0;
}
