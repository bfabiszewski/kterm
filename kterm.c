/* kterm.c
 *
 * Copyright(C) 2013 Bartek Fabiszewski (www.fabiszewski.net)
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

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <vte/vte.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include "config.h"

kterm_conf *conf;
unsigned int debug = FALSE;

void clean_and_exit(){
  D printf("clean and exit\n");
  kill(-getpid(), SIGTERM);  // cleanup children
  exit(0); 
}

void terminal_exit(){
  sleep(1); // time for kb to send key up event
  clean_and_exit(); 
}
 
void install_signal_handlers(){
  signal(SIGCHLD, SIG_IGN);  // kernel should handle zombies
  signal(SIGINT, clean_and_exit);
  signal(SIGQUIT, clean_and_exit);
  signal(SIGTERM, clean_and_exit);
}

unsigned long launch_keyboard(){
  int i = 0;
  int stdout_pipe[2];
  int stdin_pipe[2];
  char kb_bin_path[4096], kb_conf_path[4096], kb_env[4096], buf[256], c;
  size_t n;
  unsigned long result = 0;
  char self[4096], *s;

  // if kb config is not found
  if(access(conf->kb_conf_path, R_OK) == 0){   // conf->kb_conf_path defaults to MB_KBD_FULL_PATH in parse_config.c
    snprintf(kb_conf_path, sizeof(kb_conf_path), "%s", conf->kb_conf_path);
  }
  // if MB_KBD_FULL_PATH doesn't exist and a path wasn't set via conf or command line,
  // this will end up checking MB_KBD_FULL_PATH twice:
  else if(access(MB_KBD_FULL_PATH, R_OK) == 0){
    snprintf(kb_conf_path, sizeof(kb_conf_path), "%s", MB_KBD_FULL_PATH);
  } else {
    // set path to kterm binary's path
    if (readlink("/proc/self/exe", self, sizeof(self)-1) != -1){
      if((s = strrchr(self, '/')) != NULL){
        *s = '\0';
        snprintf(kb_conf_path, sizeof(kb_conf_path), "%s/%s", self, MB_KBD_CONFIG);
      }
    }
  }
  D printf("config: %s\n", kb_conf_path);

  // if kb binary is not found
  if(access(KB_FULL_PATH, F_OK) == 0){
    snprintf(kb_bin_path, sizeof(kb_bin_path), "%s%s", KB_FULL_PATH, KB_ARGS);
  } else {
    // set path to kterm binary's path
    if (readlink("/proc/self/exe", self, sizeof(self)-1) != -1){
      if((s = strrchr(self, '/')) != NULL){
        *s = '\0';
        snprintf(kb_bin_path, sizeof(kb_bin_path), "%s/%s%s", self, KB_BINARY, KB_ARGS);
      }
    }
  }
  D printf("binary: %s\n", kb_bin_path);

  // set env vars
  snprintf(kb_env, sizeof(kb_env), "MB_KBD_CONFIG=%s", kb_conf_path);
  D printf("kb_env: %s\n", kb_env);
  putenv(kb_env);

  pipe(stdout_pipe);
  pipe(stdin_pipe);

  switch(fork()){
    case 0:
      {
	// close the child process' STDOUT
	close(1);
	dup(stdout_pipe[1]);
	close(stdout_pipe[0]);
	close(stdout_pipe[1]);
        execlp("/bin/sh", "sh", "-c", kb_bin_path, NULL);
      }
    case -1:
      perror("Failed to launch matchbox-keyboard\n");
      exit(1);
  }

  // parent

  // close the write end of STDOUT
  close(stdout_pipe[1]);

  // read xid
  do {
    n = read(stdout_pipe[0], &c, 1);
    if(n == 0 || c == '\n')
      break;
      buf[i++] = c;
  } while(i < 256);

  buf[i] = '\0';
  result = atol(buf);

  close(stdout_pipe[0]);

  return result;
}

void set_terminal_font(VteTerminal *terminal, char *font_family, int font_size){
  char font_name[200];
  snprintf(font_name, sizeof(font_name), "%s %i", font_family, font_size);
  D printf("font_name: %s\n", font_name);
  vte_terminal_set_font_from_string(terminal, font_name);
}

void resize_font(gpointer terminal, unsigned int i){
  const PangoFontDescription *pango_desc;
  char *pango_name, *pch, font_family[200];
  int font_size;
  pango_desc = vte_terminal_get_font(VTE_TERMINAL(terminal));
  pango_name = pango_font_description_to_string(pango_desc);
  pch = strrchr(pango_name, ' ');
  if(pch){
    font_size = atoi(pch);
    snprintf(font_family, 
             ((pch - pango_name + 1 < sizeof(font_family)) ? (pch - pango_name + 1) : (sizeof(font_family))), 
             "%s", pango_name);
    g_free(pango_name);
    D printf("font_family: %s\n", font_family);
    D printf("font_size: %i\n", font_size);
    if(i == FONT_UP){
      font_size++;
    }
    else if(font_size > 1){
      font_size--;
    }
    set_terminal_font(terminal, font_family, font_size);
  }
}

void fontup(GtkWidget *widget, gpointer terminal){
  resize_font(terminal, FONT_UP);
}

void fontdown(GtkWidget *widget, gpointer terminal){
  resize_font(terminal, FONT_DOWN);
}

void set_terminal_colors(GtkWidget *terminal, int scheme){
  // light background
  GdkColor palette_light[8] = {{ 0, 0x0000, 0x0000, 0x0000 }, // black 
  { 0, 0x5050, 0x5050, 0x5050 }, // red
  { 0, 0x7070, 0x7070, 0x7070 }, // green
  { 0, 0xa0a0, 0xa0a0, 0xa0a0 }, // yellow
  { 0, 0x8080, 0x8080, 0x8080 }, // blue
  { 0, 0x3030, 0x3030, 0x3030 }, // magenta
  { 0, 0x9090, 0x9090, 0x9090 }, // cyan
  { 0, 0xffff, 0xffff, 0xffff }}; // white
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
  switch(scheme){
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
  vte_terminal_set_color_dim(VTE_TERMINAL(terminal), &color_dim);
  vte_terminal_set_color_bold(VTE_TERMINAL(terminal), &color_fg);
  vte_terminal_set_color_cursor(VTE_TERMINAL(terminal), NULL);
  vte_terminal_set_color_highlight(VTE_TERMINAL(terminal), NULL);
  conf->color_scheme = scheme;
}

void reverse_colors(GtkWidget *widget, gpointer terminal){
  set_terminal_colors(terminal, conf->color_scheme^1);
}

void set_box_size(GtkWidget *box){
  GList *box_list, *box_child;
  GdkScreen *screen = gdk_screen_get_default();
  gint screen_height = gdk_screen_get_height(screen);
  gint screen_width = gdk_screen_get_width(screen);
  GtkAllocation *alloc = g_new(GtkAllocation, 1);
  gtk_widget_get_allocation(box, alloc);
  gint box_height = alloc->height;
  gint box_width = alloc->width;
  g_free(alloc);
  D printf("\nscreen size: %ix%i\n", screen_width, screen_height);
  D printf("box size: %ix%i\n", box_width, box_height);
  int kb_height = (conf->kb_on)?(int)floor(screen_height/KB_HEIGHT_FACTOR):0;
  int term_height = screen_height - kb_height;
  const char *box_name = gtk_widget_get_name(box);
  D printf("box: %s\n", box_name);
  // apply to termBoxFixed
  if(!strncmp(box_name, "termBoxFixed", 12) && (box_height != term_height || box_width != screen_width)){
    D printf("==> resize term to %ix%i\n", screen_width, term_height);
    gtk_widget_set_size_request(box, screen_width, term_height);
    // apply to child, termBox
    box_list = gtk_container_get_children(GTK_CONTAINER(box));
    box_child = g_list_first(box_list);
    for (box_child = box_list; box_child != NULL; box_child = box_child->next){
      gtk_widget_set_size_request(GTK_WIDGET(box_child->data), screen_width, term_height);
    }
    g_list_free(box_list);
    g_list_free(box_child);

  } 
  // apply to kbBoxFixed
  if(!strncmp(box_name, "kbBoxFixed", 5) && (box_height != kb_height || box_width != screen_width) && kb_height > 0){
    D printf("==> resize kb to %ix%i\n", screen_width, kb_height);
    gtk_widget_set_size_request(box, screen_width, kb_height);  
  }
}

void lipc_rotate(){
  switch(fork()){
    case 0:
      {
        execlp("/bin/sh", "sh", "-c", "/usr/bin/lipc-set-prop com.lab126.winmgr orientationLock R", NULL);
      }
    case -1:
      perror("Failed to run /usr/bin/lipc-set-prop com.lab126.winmgr\n");
      exit(1);
  }
}

//FIXME: this does not work reliably as matchbox-keyboard does not resize to new screen dimensions
void screen_rotate(GtkWidget *widget, gpointer box){
  GList *box_list, *valid;
  const char *box_name;
  D box_name  = gtk_widget_get_name(box);
  D printf("box: %s\n", box_name);
  // call lipc
  lipc_rotate();
  box_list = gtk_container_get_children(GTK_CONTAINER(box));
  valid = g_list_first(box_list);
  for (valid = box_list; valid != NULL; valid = valid->next){
    D box_name = gtk_widget_get_name(GTK_WIDGET(valid->data));
    D printf("box: %s\n", box_name);
    set_box_size(GTK_WIDGET(valid->data));
  }  
  g_list_free(box_list);
  g_list_free(valid);  
}

void reset_terminal(GtkWidget *widget, gpointer terminal){
  vte_terminal_reset(terminal, TRUE, TRUE);
}

void toggle_keyboard(GtkWidget *widget, gpointer box){
  GList *box_list, *box_child;
  const char *box_name;
  GtkWidget *terminal_box = NULL, *keyboard_box = NULL;
  box_list = gtk_container_get_children(GTK_CONTAINER(box));
  box_child = g_list_first(box_list);
  for (box_child = box_list; box_child != NULL; box_child = box_child->next){
    box_name = gtk_widget_get_name(GTK_WIDGET(box_child->data));
    D printf("box: %s\n", box_name);
    if(!strncmp(box_name, "termBox", 7)){ terminal_box = GTK_WIDGET(box_child->data); }
    if(!strncmp(box_name, "kbBox", 5)){ keyboard_box = GTK_WIDGET(box_child->data); }
  }
  if(keyboard_box && terminal_box){
    if(conf->kb_on){
      gtk_widget_hide(keyboard_box);
      conf->kb_on = FALSE;
      set_box_size(terminal_box);
    } else {
      gtk_widget_show(keyboard_box);
      conf->kb_on = TRUE;
      set_box_size(terminal_box);
    }
  }
  g_list_free(box_list);
  g_list_free(box_child);  
}


static gboolean button_event(GtkWidget *terminal, GdkEventButton *event, gpointer box){
  D printf("event-type: %i\n", event->type);
  D printf("event-button: %i\n", event->button);
  // ignore any motion events (i think we don't need selecting text as one finger motion scrolls buffer)
  if(event->type == GDK_MOTION_NOTIFY) return TRUE;
  // right click
  if(event->button == 2){
    // ignore right button click to disable paste (quite messy on kindle)
    if(event->type == GDK_BUTTON_PRESS) return TRUE;
    // popup menu on button release
    GtkWidget *menu, *fontup_item, *fontdown_item, *color_item, /* *rotate_item,*/ *reset_item, *kb_item, *quit_item;
    menu = gtk_menu_new();
    fontup_item = gtk_menu_item_new_with_label("Font increase");
    fontdown_item = gtk_menu_item_new_with_label("Font decrease");
    color_item = gtk_menu_item_new_with_label("Reverse colors");
    //rotate_item = gtk_menu_item_new_with_label("Screen rotate");
    kb_item = gtk_menu_item_new_with_label("Toggle keyboard");
    reset_item = gtk_menu_item_new_with_label("Reset terminal");
    quit_item = gtk_menu_item_new_with_label("Quit");

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), fontup_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), fontdown_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), color_item);
    //gtk_menu_shell_append(GTK_MENU_SHELL(menu), rotate_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), kb_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), reset_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), quit_item);

    g_signal_connect(G_OBJECT(fontup_item), "activate", G_CALLBACK(fontup), (gpointer) terminal); 
    g_signal_connect(G_OBJECT(fontdown_item), "activate", G_CALLBACK(fontdown), (gpointer) terminal); 
    g_signal_connect(G_OBJECT(color_item), "activate", G_CALLBACK(reverse_colors), (gpointer) terminal); 
    //g_signal_connect(G_OBJECT(rotate_item), "activate", G_CALLBACK(screen_rotate), box); 
    g_signal_connect(G_OBJECT(kb_item), "activate", G_CALLBACK(toggle_keyboard), box); 
    g_signal_connect(G_OBJECT(reset_item), "activate", G_CALLBACK(reset_terminal), (gpointer) terminal); 
    g_signal_connect(G_OBJECT(quit_item), "activate", G_CALLBACK(clean_and_exit), NULL); 

    gtk_widget_show_all(menu);

    GdkEventButton *bevent = (GdkEventButton *) event;
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, bevent->button, bevent->time);
    return TRUE;
  }
  return FALSE;
}

void widget_destroy(GtkWidget *widget, gpointer data){
   XUngrabKeyboard(GDK_DISPLAY(), CurrentTime);
   gtk_widget_destroy(widget);
   gtk_main_quit();
}

void usage(){
   printf("kterm %s\n", VERSION);
   printf("Usage: kterm [OPTIONS]\n");
   printf("        -c <0|1>     - color scheme (0 light, 1 dark)\n");
   printf("        -d           - debug mode\n");
   printf("        -e <command> - execute command in kterm\n");
   printf("        -f <family>  - font family\n");
   printf("        -h           - show this message\n");
   printf("        -k <0|1>     - keyboard off/on\n");
   printf("        -l <path>    - keyboard layout config path\n");
   printf("        -s <size>    - font size\n");
   printf("        -v           - print version and exit\n");
   exit(0);
}

int main(int argc, char **argv){ 
  GtkWidget *window, *terminal, *vbox;
  GtkWidget *socket, *keyboard_box;
  unsigned long kb_xid;

  conf = parse_config(); // call first so args overide defaults/config

  // support for '-e' argument contributed by fvek
  char *cargv[255];
  char *argbuf;
  int c, i;
  cargv[0]=NULL;
  while((c = getopt(argc, argv, "c:de:f:hk:l:s:v")) != -1){
    switch(c){
      case 'd':
        debug = TRUE;
        break;
      case 'e':
        argbuf = strtok(optarg, " ");
        i = 0;
        while(argbuf != NULL){
          cargv[i] = argbuf;
          i++;
          argbuf = strtok(NULL, " ");
        }
        cargv[i] = NULL;
        break;
      case 'c':
        i = atoi(optarg);
        if ((i == 0) | (i == 1)) conf->color_scheme = i;
        break;
      case 'k':
        i = atoi(optarg);
        if ((i == 0) | (i == 1)) conf->kb_on = i;
        break;
      case 'l':
        snprintf(conf->kb_conf_path, sizeof(conf->kb_conf_path), "%s", optarg);
        break;
      case 's':
        i = atoi(optarg);
        if (i > 0) conf->font_size = i;
        break;
      case 'f':
        snprintf(conf->font_family, sizeof(conf->font_family), "%s", optarg);
        break;
      case 'h':
      case 'v':
        usage();
        break;
    }
  }

  gtk_init(&argc, &argv);

  install_signal_handlers();

  // launch keyboard
  kb_xid = launch_keyboard();
  if(!kb_xid){
    perror("matchbox-keyboard failed to return valid window ID\n");
    exit(1);
  }
  
  // window 
  //  \- vbox
  //      \- terminal_fixed  \- keyboard_fixed
  //         \- terminal        \- keyboard_box
  //                 
  // main window
  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), TITLE);
  gtk_window_set_decorated(GTK_WINDOW(window), FALSE);

  // box
  vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(window), vbox);

  // terminal
  putenv("PS1=[\\W]\\$ ");
  GtkWidget *terminal_fixed = gtk_fixed_new();
  gtk_widget_set_name(terminal_fixed, "termBoxFixed");
  terminal = vte_terminal_new();
  gtk_widget_set_name(terminal, "termBox");
  gtk_fixed_put(GTK_FIXED(terminal_fixed), terminal, 0, 0);
  set_box_size(terminal_fixed);
  set_terminal_colors(terminal, conf->color_scheme);
  vte_terminal_set_scrollback_lines(VTE_TERMINAL(terminal), VTE_SCROLLBACK_LINES);
  set_terminal_font(VTE_TERMINAL(terminal), conf->font_family, conf->font_size);
  vte_terminal_set_encoding(VTE_TERMINAL(terminal), VTE_ENCODING);
  vte_terminal_set_allow_bold(VTE_TERMINAL(terminal), TRUE);
  vte_terminal_fork_command(VTE_TERMINAL(terminal), 
                            cargv[0], 
                            (cargv[0] ? cargv : NULL), 
                            NULL, NULL, FALSE, FALSE, FALSE);
  gtk_box_pack_start(GTK_BOX(vbox), terminal_fixed, TRUE, TRUE, 0);

  // embed keyboard (XEMBED)
  GtkWidget *keyboard_fixed = gtk_fixed_new();
  gtk_widget_set_name(keyboard_fixed, "kbBoxFixed");
  keyboard_box = gtk_event_box_new();
  gtk_widget_set_name(keyboard_box, "kbBox");
  gtk_fixed_put(GTK_FIXED(keyboard_fixed), keyboard_box, 0, 0);
  set_box_size(keyboard_fixed);
  socket = gtk_socket_new();
  gtk_container_add(GTK_CONTAINER(keyboard_box), socket);
  gtk_box_pack_start(GTK_BOX(vbox), keyboard_fixed, TRUE, TRUE, 0);
  gtk_socket_add_id(GTK_SOCKET(socket), kb_xid); 

  // signals
  g_signal_connect(window, "destroy", G_CALLBACK(widget_destroy), NULL);
  g_signal_connect(window, "delete_event", G_CALLBACK(clean_and_exit), NULL);
  g_signal_connect(terminal, "child-exited", G_CALLBACK(terminal_exit), NULL);
  g_signal_connect(terminal, "button-press-event", G_CALLBACK(button_event), (gpointer) vbox);
  g_signal_connect(terminal, "button-release-event", G_CALLBACK(button_event), (gpointer) vbox);
  g_signal_connect(terminal, "motion-notify-event", G_CALLBACK(button_event), (gpointer) vbox);
  g_signal_connect(keyboard_box, "size-request", G_CALLBACK(set_box_size), NULL);
  g_signal_connect_swapped(window, "size-request", G_CALLBACK(set_box_size), terminal_fixed);
  g_signal_connect_swapped(window, "size-request", G_CALLBACK(set_box_size), keyboard_fixed);

  gtk_widget_show_all(window);

  // grab keyboard
  // this is necessary for kindle, because its framework intercepts some keystrokes
  Window root = DefaultRootWindow(GDK_DISPLAY());
  XGrabKeyboard(GDK_DISPLAY(), root, TRUE, GrabModeAsync, GrabModeAsync, CurrentTime);

  gtk_main();

  return 0;
}
