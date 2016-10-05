/* kterm.c
 *
 * This file is part of kterm
 *
 * Copyright(C) 2013-16 Bartek Fabiszewski (www.fabiszewski.net)
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

#include <gtk/gtk.h>
#include <vte/vte.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <getopt.h>
#include "keyboard.h"
#ifdef KINDLE
#include "kindle.h"
#endif
#include "config.h"

/** Vte version check for early versions */
#ifndef VTE_CHECK_VERSION
#define VTE_CHECK_VERSION(x,y,z) FALSE
#endif

/** Global config */
KTconf *conf;
/** Global debug */
gboolean debug = FALSE;

/**
 * Signals handler
 * @param signo Signal number
 */
static void exit_on_signal(gint signo) {
    D printf("exiting on signal %i\n", signo);
    if (gtk_main_level()) {
        gtk_main_quit();
    } else {
        exit(0);
    }
}

/**
 * Install signal handlers
 */
static void install_signal_handlers(void) {
    signal(SIGCHLD, SIG_IGN); // kernel should handle zombies
    signal(SIGINT, exit_on_signal);
    signal(SIGQUIT, exit_on_signal);
    signal(SIGTERM, exit_on_signal);
}

/**
 * Free all resources
 * @param keyboard Keyboard structure
 */
static void clean_on_exit(Keyboard *keyboard) {
    D printf("cleanup\n");
#ifdef KINDLE
    keyboard_grab(NULL, FALSE);
    orientation_restore();
#endif
    keyboard_free(&keyboard);
    g_free(conf);
}

/**
 * Terminal exit handler
 */
static void terminal_exit(void) {
    sleep(1); // time for kb to send key up event
    gtk_main_quit();
}

/**
 * Set terminal font
 * @param terminal Terminal
 * @param font_family Font family
 * @param font_size Font size
 */
static void set_terminal_font(VteTerminal *terminal, const gchar *font_family, const gint font_size) {
    gchar font_name[200];
    snprintf(font_name, sizeof(font_name), "%s %i", font_family, font_size);
    D printf("font_name: %s\n", font_name);
    PangoFontDescription *desc = pango_font_description_from_string(font_name);
    vte_terminal_set_font(VTE_TERMINAL(terminal), desc);
    pango_font_description_free(desc);
}

/**
 * Resize terminal font
 * @param terminal Terminal
 * @param mod FONT_UP or FONT_DOWN
 */
static void resize_font(VteTerminal *terminal, const guint mod) {
    const PangoFontDescription *pango_desc = vte_terminal_get_font(VTE_TERMINAL(terminal));
    gchar *pango_name = pango_font_description_to_string(pango_desc);
    if G_UNLIKELY(!pango_name) {
        return;
    }
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

/**
 * Increase font size menu callback
 * @param widget Calling widget
 * @param terminal Terminal
 */
static void fontup(GtkWidget *widget, gpointer terminal) {
    UNUSED(widget);
    resize_font(terminal, FONT_UP);
}

/**
 * Decrease font size menu callback
 * @param widget Calling widget
 * @param terminal Terminal
 */
static void fontdown(GtkWidget *widget, gpointer terminal) {
    UNUSED(widget);
    resize_font(terminal, FONT_DOWN);
}
/**
 * Setup terminal color scheme
 * @param terminal Terminal
 * @param scheme VTE_SCHEME_LIGHT or VTE_SCHEME_DARK
 */
static void set_terminal_colors(GtkWidget *terminal, gboolean scheme) {
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
    GdkColor color_dim = { 0, 0x8888, 0x8888, 0x8888 };
    GdkColor color_bg, color_fg;
#endif
    switch (scheme) {
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
    conf->color_reversed = scheme;
}

/**
 * Reverse color scheme menu callback
 * @param widget Calling widget
 * @param terminal Terminal
 */
static void reverse_colors(GtkWidget *widget, gpointer terminal) {
    UNUSED(widget);
    set_terminal_colors(terminal, !conf->color_reversed);
}

/**
 * Keyboard widget size allocation signal handler.
 * Updates keyboard size
 * @param keyboard_box Keyboard widget
 * @param alloc Size allocation
 * @param keyboard Keyboard structure
 */
static void keyboard_update(GtkWidget *keyboard_box, GtkAllocation *alloc, Keyboard *keyboard) {
    UNUSED(keyboard_box);
    GdkScreen *screen = gdk_screen_get_default();
    gint screen_height = gdk_screen_get_height(screen);
    static gint saved_width = -1;
    static gint saved_height = -1;
    if (conf->kb_on && alloc && (alloc->width != saved_width || screen_height != saved_height)) {
        D printf("set keyboard size: %ix%i\n", alloc->width, alloc->height);
        g_idle_add(keyboard_set_size, keyboard);
        saved_width = alloc->width;
        saved_height = screen_height;
    }
}

#ifdef KINDLE
/**
 * Rotate screen menu callback
 * @param widget Calling widget
 * @param box Kterm container
 */
static void screen_rotate(GtkWidget *widget, gpointer box) {
    UNUSED(widget);
    UNUSED(box);
    char request = 0;
    if (conf->orientation == 'U') {
        request = 'R';
    } else {
        request = 'U';
    }
    if (set_orientation(request)) {
        conf->orientation = request;
    }
}
#endif

/**
 * Reset terminal manu callback
 * @param widget Calling widget
 * @param terminal Terminal
 */
static void reset_terminal(GtkWidget *widget, gpointer terminal) {
    UNUSED(widget);
    vte_terminal_reset(terminal, TRUE, TRUE);
}

/**
 * Toggle keyboard menu callback
 * @param widget Calling widget
 * @param box Kterm container
 */
static void toggle_keyboard(GtkWidget *widget, gpointer box) {
    UNUSED(widget);
    GtkWidget *keyboard_box = NULL;
    GList *box_list = gtk_container_get_children(GTK_CONTAINER(box));
    for (GList *cur = box_list; cur != NULL; cur = cur->next) {
        const gchar *box_name = gtk_widget_get_name(GTK_WIDGET(cur->data));
        D printf("box: %s\n", box_name);
        if (!strncmp(box_name, "kbBox", 5)) { keyboard_box = GTK_WIDGET(cur->data); }
    }
    g_list_free(box_list);
    if (keyboard_box) {
        if (conf->kb_on) {
            gtk_widget_hide(keyboard_box);
            conf->kb_on = FALSE;
        } else {
            gtk_widget_show(keyboard_box);
            conf->kb_on = TRUE;
        }
    }
}

/**
 * Mouse button event callback
 * @param terminal Terminal widget
 * @param event Button event
 * @param box Kterm container
 */
static gboolean button_event(GtkWidget *terminal, GdkEventButton *event, gpointer box) {
    D printf("event-type: %i\n", event->type);
    D printf("event-button: %i\n", event->button);
#ifdef KINDLE
    // ignore any motion events (i think we don't need selecting text as one finger motion scrolls buffer)
    if (event->type == GDK_MOTION_NOTIFY) { return TRUE; }
#endif
    if (event->button == BUTTON_MENU) {
#ifdef KINDLE
        // ignore button click to disable paste (quite messy on kindle)
        if (event->type == GDK_BUTTON_PRESS) { return TRUE; }
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
        
        gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0, event->time);
        return TRUE;
    }
    return FALSE;
}

/**
 * Print version number.
 * If possible check for consistent libraries usage.
 */
static void version(void) {
    printf("kterm %s (vte %i.%i.%i, gtk+ %i.%i.%i)\n\n",
           VERSION, VTE_MAJOR_VERSION, VTE_MINOR_VERSION, VTE_MICRO_VERSION,
           GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION);
#if VTE_CHECK_VERSION(0,40,0)
    if (vte_get_major_version() != VTE_MAJOR_VERSION ||
        vte_get_minor_version() != VTE_MINOR_VERSION ||
        vte_get_micro_version() != VTE_MICRO_VERSION) {
        printf("Warning, using different vte version than compiled with (%i.%i.%i)!\n",
               vte_get_major_version(),
               vte_get_minor_version(),
               vte_get_micro_version());
    }
#endif
#if GTK_CHECK_VERSION(3,0,0)
    if (gtk_get_major_version() != GTK_MAJOR_VERSION ||
        gtk_get_minor_version() != GTK_MINOR_VERSION ||
        gtk_get_micro_version() != GTK_MICRO_VERSION) {
        printf("Warning, using different gtk+ version than compiled with (%i.%i.%i)!\n",
               gtk_get_major_version(),
               gtk_get_minor_version(),
               gtk_get_micro_version());
    }
#endif
    exit(0);
}
/**
 * Print usage info and exit
 */
static void usage(void) {
    printf("Usage: kterm [OPTIONS]\n");
    printf("        -c <0|1>     color scheme (0 light, 1 dark)\n");
    printf("        -d           debug mode\n");
    printf("        -e <command> execute command in kterm\n");
    printf("        -E <var>     set environment variable\n");
    printf("        -f <family>  font family\n");
    printf("        -h           show this message\n");
    printf("        -k <0|1>     keyboard off/on\n");
    printf("        -l <path>    keyboard layout config path\n");
#ifdef KINDLE
    printf("        -o <U|R|L>   screen orientation (up, right, left)\n");
#endif
    printf("        -s <size>    font size\n");
    printf("        -v           print version and exit\n");
    exit(0);
}

/**
 * Setup terminal
 * @param terminal Terminal
 * @param command Command passed to terminal, null if none
 * @param envv Null terminated array of env variable=value pairs passed to terminal
 * @param error Set on error, null otherwise
 */
static void setup_terminal(GtkWidget *terminal, gchar *command, gchar **envv, GError **error) {
    gchar *argv[TERM_ARGS_MAX] = { NULL };
    gint argc = 0;
    gchar *shell = NULL;
#if VTE_CHECK_VERSION(0,25,1)
    if (!command || *command == '\0') {
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

    set_terminal_colors(terminal, conf->color_reversed);
    vte_terminal_set_scrollback_lines(VTE_TERMINAL(terminal), VTE_SCROLLBACK_LINES);
    set_terminal_font(VTE_TERMINAL(terminal), conf->font_family, (gint) conf->font_size);
#if VTE_CHECK_VERSION(0,38,0)
    vte_terminal_set_encoding(VTE_TERMINAL(terminal), VTE_ENCODING, NULL);
#else
    vte_terminal_set_encoding(VTE_TERMINAL(terminal), VTE_ENCODING);
#endif
    vte_terminal_set_allow_bold(VTE_TERMINAL(terminal), TRUE);
    
#if VTE_CHECK_VERSION(0,38,0)
    vte_terminal_spawn_sync(VTE_TERMINAL(terminal), 0, NULL, argv, envv, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, error);
#elif VTE_CHECK_VERSION(0,25,1)
    vte_terminal_fork_command_full(VTE_TERMINAL(terminal), 0, NULL, argv, envv, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, error);
#else
    gboolean ret = TRUE;
    ret = vte_terminal_fork_command(VTE_TERMINAL(terminal), argv[0], (argv[0] ? argv : NULL), envv, NULL, FALSE, FALSE, FALSE);
    if (!ret) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "vte_terminal_fork_command returned error");
    }
#endif
    if (shell) { g_free(shell); }
    if (*error) {
        g_prefix_error(error, "VTE terminal fork failed.\n");
        D printf("%s\n", (*error)->message);
    }
}

/**
 * Display dialog with error message and clear error
 * @param window Parent window
 * @param error Error structure
 */
static void error_handle(GtkWidget *window, GError **error) {
    GtkDialogFlags flags = GTK_DIALOG_DESTROY_WITH_PARENT;
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(window), flags,
                                               GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                                               "%s", (*error)->message);
#ifdef KINDLE
    gtk_window_set_title(GTK_WINDOW(dialog), TITLE_DIALOG);
#endif
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER_ALWAYS);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    g_clear_error(error);
}

/** main */
gint main(gint argc, gchar **argv) {
    conf = parse_config(); // call first so args overide defaults/config
    
    gint c = -1;
    gint i = 0;
    gchar *command = NULL;
    gchar *envv[TERM_ARGS_MAX] = { NULL };
    gint envc = 0;
#ifdef KINDLE
    // modify buttons style (gtk+ 2)
    inject_gtkrc();
    // set short prompt
    envv[envc++] = "PS1=[\\W]\\$ ";
    // set terminfo path
    envv[envc++] = "TERMINFO=" TERMINFO_PATH;
#endif
    while((c = getopt(argc, argv, "c:de:E:f:hk:l:o:s:v")) != -1) {
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
                if ((i == TRUE) | (i == FALSE)) { conf->color_reversed = i; }
                break;
            case 'k':
                i = atoi(optarg);
                if ((i == TRUE) | (i == FALSE)) { conf->kb_on = i; }
                break;
            case 'l':
                snprintf(conf->kb_conf_path, sizeof(conf->kb_conf_path), "%s", optarg);
                break;
            case 'o':
                if (optarg[0] == 'U' || optarg[0] == 'R' || optarg[0] == 'L') { conf->orientation = optarg[0]; }
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
    orientation_init();
#endif
    
    GError *error = NULL;
    gtk_init(&argc, &argv);

    install_signal_handlers();
    
    // window
    //  \- vbox
    //      \- terminal  \- keyboard_box
    //
    // main window
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), TITLE);
#ifdef KINDLE
    // modify buttons style (gtk+ 3)
    inject_styles();
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
    
    Keyboard *keyboard = build_layout(keyboard_box, &error);
    if G_UNLIKELY(error) {
        error_handle(window, &error);
        clean_on_exit(keyboard);
        exit(1);
    }
    gtk_box_pack_end(GTK_BOX(vbox), keyboard_box, FALSE, FALSE, 0);
    keyboard_set_size(keyboard);
    
    GtkWidget *terminal = vte_terminal_new();
    setup_terminal(terminal, command, envv, &error);
    if G_UNLIKELY(error) {
        error_handle(window, &error);
        clean_on_exit(keyboard);
        exit(1);
    }
    gtk_widget_set_name(terminal, "termBox");
    gtk_box_pack_start(GTK_BOX(vbox), terminal, TRUE, TRUE, 0);
    
    // signals
    g_signal_connect(window, "delete_event", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(terminal, "child-exited", G_CALLBACK(terminal_exit), NULL);
    g_signal_connect(terminal, "button-press-event", G_CALLBACK(button_event), vbox);
    g_signal_connect(terminal, "button-release-event", G_CALLBACK(button_event), vbox);
    g_signal_connect(terminal, "motion-notify-event", G_CALLBACK(button_event), vbox);
#ifdef KINDLE
    g_object_set(window, "events", GDK_VISIBILITY_NOTIFY_MASK, NULL);
    g_signal_connect(window, "visibility-notify-event", G_CALLBACK(grab_keyboard_cb), NULL);
#endif
    
    gtk_widget_show_all(window);
    if (!conf->kb_on) {
        gtk_widget_hide(keyboard_box);
    }
    g_signal_connect(keyboard_box, "size-allocate", G_CALLBACK(keyboard_update), keyboard);
    gtk_window_maximize(GTK_WINDOW(window));
    gtk_widget_grab_focus(terminal);
    gtk_main();
    
    clean_on_exit(keyboard);
    D printf("the end\n");
    return 0;
}
