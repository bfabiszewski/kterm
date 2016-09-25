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
static GdkDevice *getkbdevice(void) {
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

static KBtype kbstate_to_kbtype(guint32 modifiers) {
    if (modifiers & (GDK_LOCK_MASK | GDK_SHIFT_MASK)) {
        return KBT_SHIFT;
    } else if (modifiers & GDK_MOD2_MASK) {
        return KBT_MOD1;
    } else if (modifiers & GDK_MOD3_MASK) {
        return KBT_MOD2;
    } else if (modifiers & GDK_MOD4_MASK) {
        return KBT_MOD3;
    }
    return KBT_DEFAULT;
}

static gboolean modifier_only_caps(Keyboard *keyboard) {
    return (keyboard->modifiers & GDK_LOCK_MASK) && !(keyboard->modifiers & GDK_SHIFT_MASK);
}

static void keyboard_set_layout(Keyboard *keyboard) {
    KBtype kb_type = kbstate_to_kbtype(keyboard->modifiers);
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
        } else if (key->label[type]) {
            if (key->label[type] != gtk_button_get_label(GTK_BUTTON(key->button))) {
                gtk_button_set_label(GTK_BUTTON(key->button), key->label[type]);
            }
        }
    }
}

static void keyboard_reset_modifiers(Keyboard *keyboard) {
    D printf("resetting modifiers\n");
    for (guint i = 0; i < keyboard->key_count; i++) {
        Key *key = keyboard->keys[i];
        if (key->modifier && key->modifier != GDK_LOCK_MASK) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(key->button), FALSE);
        }
    }
    keyboard->modifiers &= GDK_LOCK_MASK;
}

void keyboard_set_widths(Keyboard *keyboard) {
    if G_UNLIKELY(keyboard == NULL) {
        return;
    }
    // update geometry
    GdkScreen *screen = gdk_screen_get_default();
    gint screen_height = gdk_screen_get_height(screen);
    gint screen_width = gdk_screen_get_width(screen);
    D printf("setting widths: %ix%i\n", screen_width, screen_height);
    if (screen_width < screen_height) {
        keyboard->is_portrait = TRUE;
    }
    keyboard->row_width = (guint) screen_width;
    // count units per row
    guint units_row_max = 0;
    Key **p = keyboard->keys;
    for (guint i = 0; i < keyboard->row_count; i++) {
        guint units = 0;
        for (guint j = 0; j < keyboard->key_per_row[i]; j++) {
            Key *key = *p++;
            if (key->extended && keyboard->is_portrait) {
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
    guint unit_max = (units_row_max) ? (keyboard->row_width / units_row_max) : 0;
    guint unit_min = keyboard->unit_width;
    if (unit_min) {
        // add padding and border
        GtkWidget *first = keyboard->keys[0]->button;
#if GTK_CHECK_VERSION(3,0,0)
        GtkStyleContext *style = gtk_widget_get_style_context(first);
        GtkBorder extra = {0, 0, 0, 0};
        gtk_style_context_get_padding(style, 0, &extra);
        unit_min += (guint) (extra.left + extra.right);
        gtk_style_context_get_border(style, 0, &extra);
        unit_min += (guint) (extra.left + extra.right);
#else
        GtkStyle *style = gtk_widget_get_style(first);
        unit_min += (guint) (2 * style->xthickness);
        GtkBorder *border = NULL;
        gtk_widget_style_get(first, "inner-border", &border, NULL);
        if (border) {
            unit_min += (guint) (border->left + border->right);
        }
        gtk_border_free(border);
#endif
    }
    guint unit = MAX(unit_max, unit_min);
    for (guint i = 0; i < keyboard->key_count; i++) {
        Key *key = keyboard->keys[i];
        guint width = unit;
        if (key->width) {
            width *= key->width;
            width /= KEY_UNIT;
        }
        if (key->extended && key->keyboard->is_portrait) {
            gtk_widget_hide(key->button);
        } else if (key->fill) {
            gtk_box_set_child_packing(GTK_BOX(gtk_widget_get_parent(key->button)), key->button, TRUE, TRUE, 0, GTK_PACK_START);
        } else {
            gtk_widget_set_size_request(key->button, (gint) width, -1);
        }
    }
}

gboolean keyboard_button_off(gpointer user_data) {
    GtkWidget *button = user_data;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);
    return FALSE;
}

gboolean keyboard_event_press(Key *key) {
    Keyboard *keyboard = key->keyboard;
    GtkWidget *button = key->button;
    if (key->modifier) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)));
    } else {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
    }
    KBtype kb_type = kbstate_to_kbtype(keyboard->modifiers);
    if (modifier_only_caps(keyboard)) {
        // only caps lock
        if (!key->obey_caps) { kb_type = KBT_DEFAULT; }
    }
    D printf("press: %s\n", gdk_keyval_name(key->keyval[kb_type]));
    D printf("modifier: %u\n", key->modifier);
    D printf("modifiers: %u\n", keyboard->modifiers);
    guint keyval = key->keyval[kb_type];
    if (!keyval && !key->modifier) {
        if (kb_type == KBT_DEFAULT || (keyval = key->keyval[KBT_DEFAULT]) == 0) {
            D printf("Empty action\n");
            return TRUE;
        }
    }
    if (key->modifier) {
        keyboard->modifiers ^= key->modifier;
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
    event->key.state = (keyboard->modifiers & KB_MODIFIERS_BASIC_MASK) | (guint) keys[0].level;
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

gboolean keyboard_event_release(gpointer data) {
    Key *key = data;
    Keyboard *keyboard = key->keyboard;
    GtkWidget *button = key->button;
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);
    KBtype kb_type = kbstate_to_kbtype(keyboard->modifiers);
    if (modifier_only_caps(keyboard) && !key->obey_caps) {
        kb_type = KBT_DEFAULT;
    }
    D printf("release: %s\n", gdk_keyval_name(key->keyval[kb_type]));
    guint keyval = key->keyval[kb_type];
    if ((!keyval && kb_type == KBT_DEFAULT) || (keyval = key->keyval[KBT_DEFAULT]) == 0) {
            D printf("Empty action\n");
            return FALSE;
    }
    
    if (keyboard->modifiers & KB_MODIFIERS_SET_MASK) {
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
    event->key.state = (keyboard->modifiers & KB_MODIFIERS_BASIC_MASK) | (guint) keys[0].level;
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

void keyboard_terminal_feed(GtkWidget *button, Key *key) {
    // send characters directly via vte_terminal_feed_child()
    GtkWidget *toplevel = gtk_widget_get_toplevel(button);
    GList *box_list = gtk_container_get_children(GTK_CONTAINER(toplevel));
    GtkWidget *kterm_box = box_list->data;
    g_list_free(box_list);
    box_list = gtk_container_get_children(GTK_CONTAINER(kterm_box));
    GtkWidget *terminal = NULL;
    for (GList *cur = box_list; cur != NULL; cur = cur->next) {
        const char *box_name = gtk_widget_get_name(GTK_WIDGET(cur->data));
        if (!strncmp(box_name, "termBox", 7)) { terminal = GTK_WIDGET(cur->data); }
    }
    g_list_free(box_list);

    Keyboard *keyboard = key->keyboard;
    KBtype kb_type = kbstate_to_kbtype(keyboard->modifiers);
    if (modifier_only_caps(keyboard)) {
        // only caps lock
        if (!key->obey_caps) { kb_type = KBT_DEFAULT; }
    }
    gchar utf[6];
    gint utf_len = g_unichar_to_utf8(gdk_keyval_to_unicode(key->keyval[kb_type]), utf);
    D printf("feed child\n");
    vte_terminal_feed_child(VTE_TERMINAL(terminal), utf, utf_len);
}

gboolean keyboard_event(GtkWidget *button, GdkEvent *ev, Key *key) {
    if (ev->type == GDK_BUTTON_PRESS) {
        if (!keyboard_event_press(key)) {
            // not in keymap, try workaround
            keyboard_terminal_feed(button, key);
        }
        return TRUE;
    } else if (ev->type == GDK_BUTTON_RELEASE && !key->modifier) {
        g_timeout_add(100, keyboard_event_release, key);
        return TRUE;
    }
    return FALSE;
}

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

void keyboard_keys_free(Key **keys) {
    guint count = (*keys)->keyboard->key_count;
    if (keys == NULL) {
        return;
    }
    for (guint i = 0; i < count; i++) {
        if (keys[i] == NULL) { break; }
        keyboard_key_free(keys[i]);
    }
    g_free(keys);
}

void keyboard_free(Keyboard **keyboard) {
    if (keyboard && *keyboard) {
        keyboard_keys_free((*keyboard)->keys);
        (*keyboard)->keys = NULL;
        g_free(*keyboard);
        *keyboard = NULL;
    }
}
