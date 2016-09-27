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
/**
 * Keyboard layout variants
 */
typedef enum {
    KBT_DEFAULT = 0,
    KBT_SHIFT,
    KBT_MOD1,
    KBT_MOD2,
    KBT_MOD3,
    KBT_COUNT
} KBtype;

/**
 * Lookup table name to keyval
 */
struct kbsymlookup {
    const guint keyval;
    const gchar *name;
};

/**
 * Lookup table name to gdk modifier type
 */
struct kbmodlookup {
    const GdkModifierType modifier;
    const gchar *name;
};

/** Mask of all kterm supported modifiers set */
#define KB_MODIFIERS_SET_MASK (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_MOD2_MASK | GDK_MOD3_MASK | GDK_MOD4_MASK)
/** Mask of basic kterm supported modifiers set (shift, control, alt) */
#define KB_MODIFIERS_BASIC_MASK (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK)

/** Max count of key buttons */
#define KEYS_MAX 500
/** Max count of keyboard rows */
#define ROWS_MAX 10
/** Basic size of key button in internal units */
#define KEY_UNIT 1000

struct Keyboard;

/**
 * Single keyboard key structure
 */
typedef struct Key {
    GtkWidget *button; /** Button widget */
    gchar *label[KBT_COUNT]; /** Labels array for each layout variant */
    GtkWidget *image[KBT_COUNT]; /** Image widgets array for each layout variant */
    guint keyval[KBT_COUNT]; /** Keyvals array for each layout variant */
    GdkModifierType modifier; /** Modifier type for modifier button */
    guint width; /** Forced button width */
    gboolean obey_caps; /** Button should react to caps lock */
    gboolean fill; /** Button may expand to fill free space */
    gboolean extended; /** Button only present in landscape view */
    struct Keyboard *keyboard; /** Pointer to keyboard structure */
} Key;

/**
 * Keyboard structure
 */
typedef struct Keyboard {
    Key **keys; /** Array of keys */
    guint32 modifier_mask; /** Current state of modifiers */
    guint key_count; /** Keys count */
    guint row_count; /** Rows count */
    guint key_per_row[ROWS_MAX]; /** Keys count in each row */
    guint unit_width; /** Precalculated minimum width of a button */
    guint unit_height; /** Precalculated minimum height of a button */
} Keyboard;


Keyboard * build_layout(GtkWidget *parent, GError **error);
gboolean keyboard_event(GtkWidget *button, GdkEvent *ev, Key *key);
void keyboard_set_size(GtkWidget *keyboard_box, Keyboard *keyboard);
void keyboard_free(Keyboard **keyboard);
void keyboard_key_free(Key *key);

#endif /* keyboard_h */
