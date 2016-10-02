/* kindle.h
 *
 * This file is part of kterm
 *
 * Copyright(C) 2016 Bartek Fabiszewski (www.fabiszewski.net)
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

#ifndef kindle_h
#define kindle_h

#include <stdbool.h>
#include <gtk/gtk.h>

gchar get_orientation(void);
gboolean set_orientation(gchar orientation);
void orientation_init(void);
void orientation_restore(void);

gboolean grab_keyboard_cb(GtkWidget *widget, GdkEvent *event, gpointer data);
gboolean keyboard_grab(GdkWindow *window, gboolean grab);

void inject_gtkrc(void);
void inject_styles(void);

#endif /* kindle_h */
