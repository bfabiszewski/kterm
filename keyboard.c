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


#include <gtk/gtk.h>
#include <vte/vte.h>
#include <string.h>
#include "keyboard.h"
#include "config.h"

#if GTK_CHECK_VERSION(3,0,0)
/** 
 * Get gdk keyboard device
 * @return GdkDevice device. This object must not be freed.
 */
static GdkDevice * getkbdevice(void) {
    GdkDevice *device = NULL;
#if GTK_CHECK_VERSION(3,20,0)
    GdkDisplay *display = gdk_display_get_default();
    GdkSeat *seat = gdk_display_get_default_seat(display);
    device = gdk_seat_get_keyboard(seat);
#else
    GdkDeviceManager *manager = gdk_display_get_device_manager(gdk_display_get_default());
    GList *list = gdk_device_manager_list_devices(manager, GDK_DEVICE_TYPE_MASTER);
    
    for (GList *cur = list; cur != NULL; cur = cur->next) {
        if (gdk_device_get_source(cur->data) == GDK_SOURCE_KEYBOARD) {
            device = cur->data;
        }
    }
    g_list_free(list);
#endif
    return device;
}
#endif

/**
 * Translate modifier mask to layout variant
 * @param modifier_mask Modifier mask
 * @return KBtype layout variant
 */
static KBtype kbstate_to_kbtype(const guint32 modifier_mask) {
    KBtype type = KBT_DEFAULT;
    if (modifier_mask & (GDK_LOCK_MASK | GDK_SHIFT_MASK)) {
        type = KBT_SHIFT;
    } else if (modifier_mask & GDK_MOD2_MASK) {
        type = KBT_MOD1;
    } else if (modifier_mask & GDK_MOD3_MASK) {
        type = KBT_MOD2;
    } else if (modifier_mask & GDK_MOD4_MASK) {
        type = KBT_MOD3;
    }
    return type;
}

/**
 * Is caps lock selected and caps shift deslected
 * @param keyboard Keyboard structure
 * @return True if only caps lock selected
 */
static gboolean modifier_only_caps(const Keyboard *keyboard) {
    return (keyboard->modifier_mask & GDK_LOCK_MASK) && !(keyboard->modifier_mask & GDK_SHIFT_MASK);
}

/**
 * Set layout variant based on modifiers set
 * @param keyboard Keyboard structure
 */
static void keyboard_set_layout(const Keyboard *keyboard) {
    KBtype kb_type = kbstate_to_kbtype(keyboard->modifier_mask);
    gboolean only_caps = modifier_only_caps(keyboard);
    D printf("setting layout %d, caps: %i\n", kb_type, only_caps);
    for (guint i = 0; i < keyboard->key_count; i++) {
        Key *key = keyboard->keys[i];
        KBtype type = kb_type;
        if (only_caps && !key->obey_caps) {
            type = KBT_DEFAULT;
        }
        if (key->image[type]) {
            if (key->image[type] != gtk_button_get_image(GTK_BUTTON(key->button))) {
                g_object_ref(key->image[type]);
                gtk_button_set_image(GTK_BUTTON(key->button), key->image[type]);
                gtk_button_set_image_position(GTK_BUTTON(key->button), GTK_POS_BOTTOM);
            }
            if (gtk_button_get_label(GTK_BUTTON(key->button))) {
                gtk_button_set_label(GTK_BUTTON(key->button), NULL);
            }
        } else if (key->label[type]) {
            if (key->label[type] != gtk_button_get_label(GTK_BUTTON(key->button))) {
                gtk_button_set_label(GTK_BUTTON(key->button), key->label[type]);
            }
            if (gtk_button_get_image(GTK_BUTTON(key->button))) {
                gtk_button_set_image(GTK_BUTTON(key->button), NULL);
            }
        }
    }
}

/**
 * Deactivate modifier buttons based on modifiers set
 * @param keyboard Keyboard structure
 */
static void keyboard_reset_modifiers(Keyboard *keyboard) {
    D printf("resetting modifier_mask\n");
    for (guint i = 0; i < keyboard->key_count; i++) {
        Key *key = keyboard->keys[i];
        if (key->modifier && key->modifier != GDK_LOCK_MASK) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(key->button), FALSE);
        }
    }
    keyboard->modifier_mask &= GDK_LOCK_MASK;
}

/**
 * Calculate and set key button sizes
 * @param keyboard_box Keyboard widget
 * @param keyboard Keyboard structure
 */
void keyboard_set_size(GtkWidget *keyboard_box, Keyboard *keyboard) {
    if G_UNLIKELY(keyboard == NULL) {
        return;
    }
    // update geometry
    GdkScreen *screen = gdk_screen_get_default();
    gint screen_height = gdk_screen_get_height(screen);
    gint screen_width = gdk_screen_get_width(screen);
    gboolean is_portrait = FALSE;
    if (screen_width < screen_height) {
        is_portrait = TRUE;
    }
    // count units per row
    guint units_row_max = 0;
    Key **p = keyboard->keys;
    for (guint i = 0; i < keyboard->row_count; i++) {
        guint units = 0;
        for (guint j = 0; j < keyboard->key_per_row[i]; j++) {
            Key *key = *p++;
            if (key->extended && is_portrait) {
                continue;
            }
            if (key->width) {
                units += key->width;
            } else {
                units += KEY_UNIT;
            }
        }
        // round up units per row
        units = (units + (KEY_UNIT - 1)) / KEY_UNIT;
        units_row_max = MAX(units_row_max, units);
    }
    guint unit_hmin = keyboard->unit_height;
    guint unit_wmax = (units_row_max) ? ((guint) screen_width / units_row_max) : 0;
    guint unit_wmin = keyboard->unit_width;
    // add padding and border
    GtkWidget *first = keyboard->keys[0]->button;
#if GTK_CHECK_VERSION(3,0,0)
    GtkStyleContext *style = gtk_widget_get_style_context(first);
    GtkBorder extra = {0, 0, 0, 0};
    gtk_style_context_get_padding(style, 0, &extra);
    unit_wmin += (guint) (extra.left + extra.right);
    unit_hmin += (guint) (extra.top + extra.bottom);
    gtk_style_context_get_border(style, 0, &extra);
    unit_wmin += (guint) (extra.left + extra.right);
    unit_hmin += (guint) (extra.top + extra.bottom);
#else
    GtkStyle *style = gtk_widget_get_style(first);
    unit_wmin += (guint) (2 * style->xthickness);
    unit_hmin += (guint) (2 * style->ythickness);
    GtkBorder *border = NULL;
    gtk_widget_style_get(first, "inner-border", &border, NULL);
    if (border) {
        unit_wmin += (guint) (border->left + border->right);
        unit_hmin += (guint) (border->top + border->bottom);
        gtk_border_free(border);
    }
#endif
    guint unit_w = MAX(unit_wmax, unit_wmin);
    for (guint i = 0; i < keyboard->key_count; i++) {
        Key *key = keyboard->keys[i];
        guint width = unit_w;
        if (key->width) {
            width *= key->width;
            width /= KEY_UNIT;
        }
        if (key->extended && is_portrait) {
            if (gtk_widget_get_visible(key->button)) {
                gtk_widget_hide(key->button);
            }
            continue;
        } else if (key->extended) {
            if (!gtk_widget_get_visible(key->button)) {
                gtk_widget_show(key->button);
            }
        }
        if (key->fill) {
            gtk_box_set_child_packing(GTK_BOX(gtk_widget_get_parent(key->button)), key->button, TRUE, TRUE, 0, GTK_PACK_START);
        } else {
            gtk_widget_set_size_request(key->button, (gint) width, -1);
        }
    }
    gint kb_width = screen_width;
    
    // calculate keyboard widget height
    gdouble dpi = gdk_screen_get_resolution(screen);
    D printf("screen size: %ix%i (%i dpi)\n", screen_width, screen_height, (gint) dpi);
    if (dpi < 0) { dpi = 96; }
    guint unit_h = unit_hmin;
    const guint unit_hpref = (guint) (KB_KEYHEIGHT_PREF * dpi);
    const guint unit_hmax = (guint) screen_height / KB_HEIGHTMAX_FACTOR / keyboard->row_count;
    if (unit_hmin > unit_hmax || unit_hpref > unit_hmax) {
        unit_h = unit_hmax;
    }
    else if (unit_hpref >= unit_hmin) {
        unit_h = unit_hpref;
    }
    D printf("hmin: %d, hmax: %d, pref: %d => %d\n", unit_hmin, unit_hmax, unit_hpref, unit_h);
    gint kb_height = (gint) (unit_h * keyboard->row_count);
    D printf("keyboard size: %ix%i\n", kb_width, kb_height);
    gtk_widget_set_size_request(keyboard_box, -1, kb_height);
}

/**
 * Key press event callback
 * @param key Event key structure
 * @return True on success, false otherwise
 */
static gboolean keyboard_event_press(Key *key) {
    Keyboard *keyboard = key->keyboard;
    GtkWidget *button = key->button;
    if (key->modifier) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)));
    } else {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
    }
    KBtype kb_type = kbstate_to_kbtype(keyboard->modifier_mask);
    if (modifier_only_caps(keyboard)) {
        // only caps lock
        if (!key->obey_caps) { kb_type = KBT_DEFAULT; }
    }
    D printf("press: %s\n", gdk_keyval_name(key->keyval[kb_type]));
    D printf("modifier: %u\n", key->modifier);
    D printf("modifier_mask: %u\n", keyboard->modifier_mask);
    guint keyval = key->keyval[kb_type];
    if (!keyval && !key->modifier) {
        if (kb_type == KBT_DEFAULT || (keyval = key->keyval[KBT_DEFAULT]) == 0) {
            D printf("Empty action\n");
            return TRUE;
        }
    }
    if (key->modifier) {
        keyboard->modifier_mask ^= key->modifier;
        if (kbstate_to_kbtype(key->modifier)) {
            keyboard_set_layout(keyboard);
        }
        return TRUE;
    }

    GdkKeymapKey *keys = NULL;
    gint n_keys = 0;
    if (!gdk_keymap_get_entries_for_keyval(gdk_keymap_get_default(), keyval, &keys, &n_keys)) {
        return FALSE;
    }
    GdkEvent *event = gdk_event_new(GDK_KEY_PRESS);
    event->key.window = g_object_ref(gtk_widget_get_window(GTK_WIDGET(button)));
    event->key.state = (keyboard->modifier_mask & KB_MODIFIERS_BASIC_MASK) | (guint) keys[0].level;
    event->key.hardware_keycode = (guint16) keys[0].keycode;
    event->key.keyval = keyval;
    event->key.send_event = FALSE;
    event->key.time = GDK_CURRENT_TIME;
#if GTK_CHECK_VERSION(3,0,0)
    // gtk3 complains about fake device
    gdk_event_set_device(event, getkbdevice());
#endif
    gtk_main_do_event(event);
    gdk_event_free(event);
    g_free(keys);
    
    return TRUE;
}

/**
 * Key release event callback
 * @param data Event key structure
 * @return Always false to cancel timout, as called by g_timeout_add()
 */
static gboolean keyboard_event_release(gpointer data) {
    Key *key = data;
    Keyboard *keyboard = key->keyboard;
    GtkWidget *button = key->button;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);
    KBtype kb_type = kbstate_to_kbtype(keyboard->modifier_mask);
    if (modifier_only_caps(keyboard) && !key->obey_caps) {
        kb_type = KBT_DEFAULT;
    }
    D printf("release: %s\n", gdk_keyval_name(key->keyval[kb_type]));
    guint keyval = key->keyval[kb_type];
    if ((!keyval && kb_type == KBT_DEFAULT) || (keyval = key->keyval[KBT_DEFAULT]) == 0) {
        D printf("Empty action\n");
        return FALSE;
    }
    
    if (keyboard->modifier_mask & KB_MODIFIERS_SET_MASK) {
        keyboard_reset_modifiers(keyboard);
        keyboard_set_layout(keyboard);
    }
    
    GdkKeymapKey *keys = NULL;
    gint n_keys = 0;
    if (!gdk_keymap_get_entries_for_keyval(gdk_keymap_get_default(), keyval, &keys, &n_keys)) {
        return FALSE;
    }
    GdkEvent *event = gdk_event_new(GDK_KEY_RELEASE);
    event->key.window = g_object_ref(gtk_widget_get_window(GTK_WIDGET(button)));
    event->key.state = (keyboard->modifier_mask & KB_MODIFIERS_BASIC_MASK) | (guint) keys[0].level;
    event->key.hardware_keycode = (guint16) keys[0].keycode;
    event->key.keyval = keyval;
    event->key.send_event = FALSE;
    event->key.time = GDK_CURRENT_TIME;
#if GTK_CHECK_VERSION(3,0,0)
    // gtk3 complains about fake device
    gdk_event_set_device(event, getkbdevice());
#endif
    gtk_main_do_event(event);
    gdk_event_free(event);
    g_free(keys);
    return FALSE;
}

/**
 * Key release event callback
 * @param button Key button widget
 * @param key Key structure
 */
static void keyboard_terminal_feed(GtkWidget *button, const Key *key) {
    // send characters directly via vte_terminal_feed_child()
    GtkWidget *toplevel = gtk_widget_get_toplevel(button);
    GList *box_list = gtk_container_get_children(GTK_CONTAINER(toplevel));
    GtkWidget *kterm_box = box_list->data;
    g_list_free(box_list);
    box_list = gtk_container_get_children(GTK_CONTAINER(kterm_box));
    GtkWidget *terminal = NULL;
    for (GList *cur = box_list; cur != NULL; cur = cur->next) {
        const gchar *box_name = gtk_widget_get_name(GTK_WIDGET(cur->data));
        if (!strcmp(box_name, "termBox")) { terminal = GTK_WIDGET(cur->data); }
    }
    g_list_free(box_list);
    if G_UNLIKELY(!terminal) {
        return;
    }

    Keyboard *keyboard = key->keyboard;
    KBtype kb_type = kbstate_to_kbtype(keyboard->modifier_mask);
    if (modifier_only_caps(keyboard)) {
        // only caps lock
        if (!key->obey_caps) { kb_type = KBT_DEFAULT; }
    }
    gchar utf[6];
    gint utf_len = g_unichar_to_utf8(gdk_keyval_to_unicode(key->keyval[kb_type]), utf);
    D printf("feed child: %i chars\n", utf_len);
    vte_terminal_feed_child(VTE_TERMINAL(terminal), utf, utf_len);
}

/**
 * Key event callback
 * @param button Key button widget
 * @param ev Gdk event
 * @param key Key structure
 * @return True to stop processing event, false otherwise
 */
gboolean keyboard_event(GtkWidget *button, GdkEvent *ev, Key *key) {
    if (ev->type == GDK_BUTTON_PRESS) {
        if (!keyboard_event_press(key)) {
            // not in keymap, try workaround
            keyboard_terminal_feed(button, key);
        }
        return TRUE;
    } else if (ev->type == GDK_BUTTON_RELEASE && !key->modifier) {
        g_timeout_add(KB_RELEASE_DELAY_MS, keyboard_event_release, key);
        return TRUE;
    }
    return FALSE;
}

/**
 * Free key structure
 * @param key Key structure
 */
void keyboard_key_free(Key *key) {
    if (key) {
        if (key->button && GTK_IS_WIDGET(key->button)) { gtk_widget_destroy(key->button); }
        for (gint i = 0; i < KBT_COUNT; i++) {
            if (key->image[i] && GTK_IS_WIDGET(key->image[i])) { gtk_widget_destroy(key->image[i]); }
            if (key->label[i]) { g_free(key->label[i]); }
        }
        g_free(key);
        key = NULL;
    }
}

/**
 * Recursively free array of key structures
 * @param keys Array of key structures
 */
static void keyboard_keys_free(Key **keys) {
    if (keys == NULL) {
        return;
    }
    guint count = (*keys && (*keys)->keyboard) ? (*keys)->keyboard->key_count : 0;
    for (guint i = 0; i < count; i++) {
        if (keys[i] == NULL) { break; }
        keyboard_key_free(keys[i]);
    }
    g_free(keys);
}

/**
 * Recursively free keyboard structure
 * @param keyboard structure
 */
void keyboard_free(Keyboard **keyboard) {
    if (keyboard && *keyboard) {
        keyboard_keys_free((*keyboard)->keys);
        (*keyboard)->keys = NULL;
        g_free(*keyboard);
        *keyboard = NULL;
    }
}
