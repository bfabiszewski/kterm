/* parse_layout.c
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

#include <stdlib.h>
#include <string.h>
#include "keyboard.h"
#include "config.h"

/** Global config */
extern KTconf *conf;

/** Parser state */
typedef struct {
    Keyboard *keyboard; /** Keyboard structure to be filled */
    GtkWidget *container; /** Container widget for keyboard */
    GtkWidget *current_row; /** Currently parsed row */
    Key *current_key; /** Currently parsed key */
} State;

/**
 * Lookup table name to keyval
 */
static struct kbsymlookup kbspecial[] = {
#if GTK_CHECK_VERSION(3,0,0)
    { GDK_KEY_BackSpace, "backspace" },
    { GDK_KEY_Tab, "tab" },
    { GDK_KEY_Linefeed, "linefeed" },
    { GDK_KEY_Clear, "clear" },
    { GDK_KEY_Return, "return" },
    { GDK_KEY_Pause, "pause" },
    { GDK_KEY_Scroll_Lock, "scrolllock" },
    { GDK_KEY_Sys_Req, "sysreq" },
    { GDK_KEY_Escape, "escape" },
    { GDK_KEY_Delete, "delete" },
    { GDK_KEY_Home, "home" },
    { GDK_KEY_Left, "left" },
    { GDK_KEY_Up, "up" },
    { GDK_KEY_Right, "right" },
    { GDK_KEY_Down, "down" },
    { GDK_KEY_Prior, "prior" },
    { GDK_KEY_Page_Up, "pageup" },
    { GDK_KEY_Next, "next" },
    { GDK_KEY_Page_Down, "pagedown" },
    { GDK_KEY_End, "end" },
    { GDK_KEY_Begin, "begin" },
    { GDK_KEY_space, "space" },
    { GDK_KEY_F1, "f1" },
    { GDK_KEY_F2, "f2" },
    { GDK_KEY_F3, "f3" },
    { GDK_KEY_F4, "f4" },
    { GDK_KEY_F5, "f5" },
    { GDK_KEY_F6, "f6" },
    { GDK_KEY_F7, "f7" },
    { GDK_KEY_F8, "f8" },
    { GDK_KEY_F9, "f9" },
    { GDK_KEY_F10, "f10" },
    { GDK_KEY_F11, "f11" },
    { GDK_KEY_F12 , "f12" }
#else
    { GDK_BackSpace, "backspace" },
    { GDK_Tab, "tab" },
    { GDK_Linefeed, "linefeed" },
    { GDK_Clear, "clear" },
    { GDK_Return, "return" },
    { GDK_Pause, "pause" },
    { GDK_Scroll_Lock, "scrolllock" },
    { GDK_Sys_Req, "sysreq" },
    { GDK_Escape, "escape" },
    { GDK_Delete, "delete" },
    { GDK_Home, "home" },
    { GDK_Left, "left" },
    { GDK_Up, "up" },
    { GDK_Right, "right" },
    { GDK_Down, "down" },
    { GDK_Prior, "prior" },
    { GDK_Page_Up, "pageup" },
    { GDK_Next, "next" },
    { GDK_Page_Down, "pagedown" },
    { GDK_End, "end" },
    { GDK_Begin, "begin" },
    { GDK_space, "space" },
    { GDK_F1, "f1" },
    { GDK_F2, "f2" },
    { GDK_F3, "f3" },
    { GDK_F4, "f4" },
    { GDK_F5, "f5" },
    { GDK_F6, "f6" },
    { GDK_F7, "f7" },
    { GDK_F8, "f8" },
    { GDK_F9, "f9" },
    { GDK_F10, "f10" },
    { GDK_F11, "f11" },
    { GDK_F12 , "f12" }
#endif
};

/** Max size of kbspecial array */
#define KBSYM_SIZE sizeof(kbspecial)/sizeof(kbspecial[0])

/**
 * Lookup table name to gdk modifier type
 */
static struct kbmodlookup kbmod[] = {
    { GDK_SHIFT_MASK, "shift" },
    { GDK_LOCK_MASK, "caps" },
    { GDK_CONTROL_MASK, "ctrl" },
    { GDK_MOD1_MASK, "alt" },
    { GDK_MOD2_MASK, "mod1" },
    { GDK_MOD3_MASK, "mod2" },
    { GDK_MOD4_MASK, "mod3" }
};

/** Max size of kbmod array */
#define KBMOD_SIZE sizeof(kbmod)/sizeof(kbmod[0])

/** 
 * Start parsing row node
 * @param state Parser state structure
 * @return True on success, false otherwise
 */
static gboolean parser_row_start(State *state) {
    if (state->keyboard->row_count >= ROWS_MAX) {
        D printf("Too many rows\n");
        return FALSE;
    }
    if (state->current_row) {
        D printf("Row not empty\n");
        return FALSE;
    }
#if GTK_CHECK_VERSION(3,0,0)
    state->current_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
#else
    state->current_row = gtk_hbox_new(FALSE, 0);
#endif
    state->keyboard->row_count++;
    return TRUE;
}

/**
 * End parsing row node
 * @param state Parser state structure
 * @return True on success, false otherwise
 */
static gboolean parser_row_end(State *state) {
    if (state->current_row == NULL) {
        D printf("Row empty\n");
        return FALSE;
    }
    gtk_box_pack_start(GTK_BOX(state->container), state->current_row, TRUE, TRUE, 0);
    state->current_row = NULL;
    return TRUE;
}

/**
 * Parse key node
 * @param state Parser state structure
 * @param attribute_names Attribute names array
 * @param attribute_values Attribute values array
 * @return True on success, false otherwise
 */
static gboolean parser_button_start(State *state, const gchar **attribute_names, const gchar **attribute_values) {
    if (state->current_key) {
        D printf("Key not empty\n");
        return FALSE;
    }
    Key *key = g_malloc0(sizeof(Key));
    if (!key) {
        D printf("Memory allocation failed\n");
        return FALSE;
    }
    for (gint j = 0; attribute_names[j]; j++) {
        if (!g_ascii_strcasecmp(attribute_names[j], "obey-caps") && !g_ascii_strcasecmp(attribute_values[j], "true")) {
            key->obey_caps = TRUE;
        }
        else if (!g_ascii_strcasecmp(attribute_names[j], "fill") && !g_ascii_strcasecmp(attribute_values[j], "true")) {
            key->fill = TRUE;
        }
        else if (!g_ascii_strcasecmp(attribute_names[j], "extended") && !g_ascii_strcasecmp(attribute_values[j], "true")) {
            key->extended = TRUE;
        }
        else if (!g_ascii_strcasecmp(attribute_names[j], "width")) {
            key->width = (guint) atoi(attribute_values[j]);
        }
    }
    key->keyboard = state->keyboard;
    key->button = gtk_toggle_button_new();
    gtk_widget_set_name(key->button, "ktermKbButton");
    gtk_widget_set_can_focus(key->button, FALSE);
    state->current_key = key;
    return TRUE;
}

/**
 * End parsing key node
 * @param state Parser state structure
 * @return True on success, false otherwise
 */
static gboolean parser_button_end(State *state) {
    if (state->current_key == NULL) {
        D printf("Button empty\n");
        return FALSE;
    }
    gtk_box_pack_start(GTK_BOX(state->current_row), state->current_key->button, FALSE, FALSE, 0);
    state->keyboard->keys[state->keyboard->key_count++] = state->current_key;
    state->current_key = NULL;
    guint row_number = state->keyboard->row_count - 1;
    state->keyboard->key_per_row[row_number]++;
    return TRUE;
}

/**
 * Parse space node
 * @param state Parser state structure
 * @param attribute_names Attribute names array
 * @param attribute_values Attribute values array
 * @return True on success, false otherwise
 */
static gboolean parser_button_space(State *state, const gchar **attribute_names, const gchar **attribute_values) {
    Key *key = g_malloc0(sizeof(Key));
    if (!key) {
        D printf("Memory allocation failed\n");
        return FALSE;
    }
    for (gint j = 0; attribute_names[j]; j++) {
        if (!g_ascii_strcasecmp(attribute_names[j], "fill") && !g_ascii_strcasecmp(attribute_values[j], "true")) {
            key->fill = TRUE;
        }
        else if (!g_ascii_strcasecmp(attribute_names[j], "extended") && !g_ascii_strcasecmp(attribute_values[j], "true")) {
            key->extended = TRUE;
        }
        else if (!g_ascii_strcasecmp(attribute_names[j], "width")) {
            key->width = (guint) atoi(attribute_values[j]);
        }
    }
    key->keyboard = state->keyboard;
    key->button = gtk_label_new(NULL);
    gtk_widget_set_can_focus(key->button, FALSE);
    state->current_key = key;
    parser_button_end(state);
    return TRUE;
}

/**
 * Set key button label/image
 * @param key Key structure
 * @param attribute_value Display attribute value
 * @param kb_type Layout variant
 * @param width Will be set to minium width of the button
 * @param height Will be set to minium height of the button
 */
static void parser_button_label(Key *key, const gchar *attribute_value, const KBtype kb_type, gint *width, gint *height) {
    const gchar prefix[] = "image:";
    const guint prefix_len = sizeof(prefix) - 1;
    *width = 0;
    *height = 0;
    if (!strncmp(attribute_value, prefix, prefix_len)) {
        GtkWidget *button_image = gtk_image_new();
        gchar path[PATH_MAX];
        if (attribute_value[prefix_len] == '/') {
            // absolute path
            snprintf(path, sizeof(path), "%s", &attribute_value[prefix_len]);
        } else {
            // relative to config
            snprintf(path, sizeof(path), "%s", conf->kb_conf_path);
            gchar *p = NULL;
            if ((p = strrchr(path, '/')) != NULL) {
                *++p = '\0';
                guint space_left = sizeof(path) - (guint) strlen(path);
                strncpy(p, &attribute_value[prefix_len], space_left);
            } else {
                snprintf(path, sizeof(path), "%s", &attribute_value[prefix_len]);
            }
        }
        gtk_image_set_from_file(GTK_IMAGE(button_image), path);
        if (kb_type == KBT_DEFAULT) {
            // initiate default layout
            g_object_ref(button_image);
            gtk_button_set_image(GTK_BUTTON(key->button), button_image);
            gtk_button_set_image_position(GTK_BUTTON(key->button), GTK_POS_BOTTOM);
        }
        key->image[kb_type] = button_image;
        key->label[kb_type] = NULL;
        if (!key->width) {
            const GdkPixbuf *pixbuf = gtk_image_get_pixbuf(GTK_IMAGE(button_image));
            if (pixbuf) {
                *width = gdk_pixbuf_get_width(pixbuf);
                *height = gdk_pixbuf_get_height(pixbuf);
            }
        }
    } else {
        key->label[kb_type] = g_strdup(attribute_value);
        key->image[kb_type] = NULL;
        if (kb_type == KBT_DEFAULT) {
            gtk_button_set_label(GTK_BUTTON(key->button), key->label[kb_type]);
        }
        if (!key->width && g_utf8_strlen(key->label[kb_type], -1) == 1) {
            PangoLayout *layout = gtk_widget_create_pango_layout(key->button, key->label[kb_type]);
            pango_layout_get_pixel_size(layout, width, height);
            g_object_unref(layout);
        }
    }
}

/**
 * Get gdk modifier type for name string
 * @param mod_str Modifier name
 * @return Gdk modifier type or zero
 */
static GdkModifierType parser_get_modtype(const gchar *mod_str) {
    for (guint i = 0; i < KBMOD_SIZE; i++) {
        if (!g_ascii_strcasecmp(mod_str, kbmod[i].name)) {
            return kbmod[i].modifier;
        }
    }
    return 0;
}

/**
 * Get keyval for name string
 * @param special Special button name
 * @return Keyval
 */
static guint parser_get_keyval(const gchar *special) {
    for (guint i = 0; i < KBSYM_SIZE; i++) {
        if (!g_ascii_strcasecmp(special, kbspecial[i].name)) {
            return kbspecial[i].keyval;
        }
    }
    return 0;
}

/**
 * Set key button action
 * @param key Key structure
 * @param attribute_value Action attribute value
 * @param kb_type Layout variant
 */
static void parser_button_action(Key *key, const gchar *attribute_value, const KBtype kb_type) {
    const gchar prefix[] = "modifier:";
    const guint prefix_len = sizeof(prefix) - 1;
    if (!strncmp(attribute_value, prefix, prefix_len)) {
        key->keyval[kb_type] = 0;
        key->modifier = parser_get_modtype(&attribute_value[prefix_len]);
    } else {
        if (g_utf8_strlen(attribute_value, -1) == 1) {
            key->keyval[kb_type] = gdk_unicode_to_keyval(g_utf8_get_char(attribute_value));
        } else {
            key->keyval[kb_type] = parser_get_keyval(attribute_value);
        }
    }
}

/**
 * Parse key node attributes
 * @param state Parser state structure
 * @param attribute_names Attribute names array
 * @param attribute_values Attribute values array
 * @param kb_type Keyboard layout variant
 * @return True on success, false otherwise
 */
static gboolean parser_button_contents(State *state, const gchar **attribute_names, const gchar **attribute_values, KBtype kb_type) {
    if (state->current_key == NULL) {
        D printf("Button empty\n");
        return FALSE;
    }
    Key *key = state->current_key;
    const gchar *action = NULL;
    for (gint j = 0; attribute_names[j]; j++) {
        if (!g_ascii_strcasecmp(attribute_names[j], "display")) {
            gint width = 0;
            gint height = 0;
            parser_button_label(key, attribute_values[j], kb_type, &width, &height);
            D printf("key width: %i\n", width);
            if ((guint) width > state->keyboard->unit_width) {
                state->keyboard->unit_width = (guint) width;
            }
            if ((guint) height > state->keyboard->unit_height) {
                state->keyboard->unit_height = (guint) height;
            }
        }
        else if (!g_ascii_strcasecmp(attribute_names[j], "action")) {
            action = attribute_values[j];
        }
    }
    if (action) {
        parser_button_action(key, action, kb_type);
    } else if (key->label[kb_type]) {
        parser_button_action(key, key->label[kb_type], kb_type);
    }
    if (kb_type == KBT_DEFAULT) {
        g_signal_connect(key->button, "button-press-event", G_CALLBACK(keyboard_event), key);
        g_signal_connect(key->button, "button-release-event", G_CALLBACK(keyboard_event), key);
    }

    return TRUE;
}

/**
 * Parser start node callback
 * @param context Parser internal context
 * @param node_name Node name
 * @param attribute_names Attribute names array
 * @param attribute_values Attribute values array
 * @param user_data Contains state structure
 * @param error Set on error, null if success
 */
static void parser_start_node_cb(GMarkupParseContext *context, const gchar *node_name,
                                 const gchar **attribute_names, const gchar **attribute_values,
                                 gpointer user_data, GError **error) {
    UNUSED(context);
    gboolean ret = TRUE;
    State *state = user_data;
    for (gint i = 0; attribute_names[i]; i++) {
        D printf ("attribute %s = \"%s\"\n", attribute_names[i], attribute_values[i]);
    }
    if (!strcmp(node_name, "row")) {
        ret = parser_row_start(state);
    }
    else if (!strcmp(node_name, "key")) {
        ret = parser_button_start(state, attribute_names, attribute_values);
    }
    else if (!strcmp(node_name, "default") || !strcmp(node_name, "normal")) {
        ret = parser_button_contents(state, attribute_names, attribute_values, KBT_DEFAULT);
    }
    else if (!strcmp(node_name, "shifted")) {
        ret = parser_button_contents(state, attribute_names, attribute_values, KBT_SHIFT);
    }
    else if (!strcmp(node_name, "mod1")) {
        ret = parser_button_contents(state, attribute_names, attribute_values, KBT_MOD1);
    }
    else if (!strcmp(node_name, "mod2")) {
        ret = parser_button_contents(state, attribute_names, attribute_values, KBT_MOD2);
    }
    else if (!strcmp(node_name, "mod3")) {
        ret = parser_button_contents(state, attribute_names, attribute_values, KBT_MOD3);
    }
    else if (!strcmp(node_name, "space")) {
        ret = parser_button_space(state, attribute_names, attribute_values);
    }
    if (ret == FALSE) {
        g_set_error(error, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE, "Not a valid kterm keyboard layout");
    }
}

/**
 * Parser end node callback
 * @param context Parser internal context
 * @param node_name Node name
 * @param user_data Contains state structure
 * @param error Set on error, null if success
 */
static void parser_end_node_cb(GMarkupParseContext *context, const gchar *node_name,
                               gpointer user_data, GError **error) {
    UNUSED(context);
    gboolean ret = TRUE;
    State *state = user_data;
    D printf("end name: %s\n", node_name);
    if (!strcmp(node_name, "row")) {
        ret = parser_row_end(state);
    }
    else if (!strcmp(node_name, "key")) {
        ret = parser_button_end(state);
    }
    if (ret == FALSE) {
        g_set_error(error, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE, "Not a valid kterm keyboard layout");
    }
}

/**
 * Recursively free state structure
 * @param state Parser state
 */
static void state_cleanup(State *state) {
    if (state) {
        if (state->current_row) { gtk_widget_destroy(state->current_row); }
        if (state->current_key) { keyboard_key_free(state->current_key); }
    }
}

/**
 * Parse keyboard config and build initial layout
 * @param parent Parent widget for keyboard widget
 * @param error Set on error, null if success
 * @return Keyboard structure, null on failure
 */
Keyboard * build_layout(GtkWidget *parent, GError **error) {
    FILE *fp = NULL;
    if (getenv("MB_KBD_CONFIG")) {
        // override path with env variable
        gchar kb_env_path[PATH_MAX];
        snprintf(kb_env_path, sizeof(kb_env_path), "%s", getenv("MB_KBD_CONFIG"));
        fp = fopen(kb_env_path, "r");
    }
    if (fp) {
        snprintf(conf->kb_conf_path, sizeof(conf->kb_conf_path), "%s", getenv("MB_KBD_CONFIG"));
        D printf("Layout path from MB_KBD_CONFIG: %s\n", getenv("MB_KBD_CONFIG"));
    } else if ((fp = fopen(conf->kb_conf_path, "r")) != NULL) {
        D printf("Layout path from config: %s\n", conf->kb_conf_path);
    } else {
        D printf("No layout config\n");
        return NULL;
    }

    State state;
    memset(&state, 0, sizeof(State));
    Keyboard *keyboard = g_malloc0(sizeof(Keyboard));
    if (!keyboard) {
        D printf("Memory allocation failed\n");
        fclose(fp);
        return NULL;
    }
    state.keyboard = keyboard;
    Key **keys = g_malloc0(KEYS_MAX * sizeof(Key*));
    if (!keys) {
        D printf("Memory allocation failed\n");
        fclose(fp);
        g_free(keyboard);
        return NULL;
    }
    keyboard->keys = keys;
    state.container = parent;
    GMarkupParser parser;
    memset(&parser, 0, sizeof(GMarkupParser));
    parser.start_element = parser_start_node_cb;
    parser.end_element = parser_end_node_cb;
    GMarkupParseContext *context = g_markup_parse_context_new(&parser, 0, &state, NULL);
    if G_UNLIKELY(!context) {
        D printf("g_markup_parse_context_new failed\n");
        fclose(fp);
        keyboard_free(&keyboard);
        state_cleanup(&state);
        return NULL;
    }
    gchar buf[500];
    while (fgets(buf, sizeof(buf), fp)) {
        g_markup_parse_context_parse(context, buf, (gssize) strlen(buf), error);
        if (keyboard->key_count + KBT_COUNT >= KEYS_MAX) {
            g_set_error(error, G_MARKUP_ERROR, G_MARKUP_ERROR_PARSE, "Too many keys (max %i)", KEYS_MAX);
        }
        if G_UNLIKELY(*error) { break; }
    }
    g_markup_parse_context_free(context);
    D printf("Parsed %d keys in %d rows\n", keyboard->key_count, keyboard->row_count);
    if G_UNLIKELY(*error){
        g_prefix_error(error, "Keyboard layout parser error.\n");
        D printf("%s\n", (*error)->message);
        keyboard_free(&keyboard);
    } else {
        keyboard->keys = g_realloc(keyboard->keys, keyboard->key_count * sizeof(Key*));
        keyboard->container = state.container;
    }
    fclose(fp);
    state_cleanup(&state);
    
    return keyboard;
}
