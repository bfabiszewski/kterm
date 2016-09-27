/* keyboard.c
 *
 * This file is part of kterm
 *
 * Copyright(C) 2016 Bartek Fabiszewski (www.fabiszewski.net)
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

#ifndef keyboard_h
#define keyboard_h

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

typedef enum {
    KBT_DEFAULT = 0,
    KBT_SHIFT,
    KBT_MOD1,
    KBT_MOD2,
    KBT_MOD3,
    KBT_COUNT
} KBtype;

struct kbsymlookup {
    guint keyval;
    gchar *name;
};

struct kbmodlookup {
    GdkModifierType modifier;
    gchar *name;
};

#define KB_MODIFIERS_SET_MASK (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_MOD2_MASK | GDK_MOD3_MASK | GDK_MOD4_MASK)
#define KB_MODIFIERS_BASIC_MASK (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK)

#define KEYS_MAX 500
#define ROWS_MAX 10
#define KEY_UNIT 1000

struct Keyboard;

typedef struct Key {
    GtkWidget *button;
    gchar *label[KBT_COUNT];
    GtkWidget *image[KBT_COUNT];
    guint keyval[KBT_COUNT];
    GdkModifierType modifier;
    guint width;
    gboolean obey_caps;
    gboolean fill;
    gboolean extended;
    struct Keyboard *keyboard;
} Key;

typedef struct Keyboard {
    Key **keys;
    guint32 modifier_mask;
    guint key_count;
    guint row_count;
    guint key_per_row[ROWS_MAX];
    guint unit_width;
    guint unit_height;
    gboolean is_portrait;
    guint row_width;
} Keyboard;


Keyboard * build_layout(GtkWidget *parent, GError **error);
gboolean keyboard_event(GtkWidget *button, GdkEvent *ev, Key *key);
void keyboard_set_size(GtkWidget *keyboard_box, Keyboard *keyboard);
void keyboard_free(Keyboard **keyboard);
void keyboard_key_free(Key *key);

#endif /* keyboard_h */
