/* kindle.c
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

#include <stdio.h>
#include "kindle.h"
#include "config.h"

/** Global config */
extern KTconf *conf;

#ifdef HAVE_DBUS

#include <dbus/dbus.h>

/**
 * Send dbus message, get string response if requested
 * @param destination Name that the message should be sent to
 * @param method Method to invoke
 * @param param Optional param do append to message
 * @param reply_msg Location to store reply message or NULL to ignore response
 * @return True on success, false otherwise
 */
static gboolean dbus_send(const gchar *dest, const gchar *method, const gchar *param, gchar **reply_msg) {
    gboolean ret = TRUE;
    DBusError error;
    DBusBusType type = DBUS_BUS_SYSTEM;
    gchar iface[256];
    snprintf(iface, sizeof(iface), "%s.%s", dest, method);
    const gchar *path = "/default";
    
    dbus_error_init(&error);
    DBusConnection *connection = dbus_bus_get(type, &error);
    if G_UNLIKELY(!connection) {
        D printf("Dbus connection failed: %s\n", error.message);
        dbus_error_free(&error);
        return FALSE;
    }
    DBusMessage *message = dbus_message_new_method_call(dest, path, iface, method);
    if G_UNLIKELY(!message) {
        D printf("Dbus message allocation failed\n");
        dbus_connection_unref(connection);
        return FALSE;
    }
    if (param && !dbus_message_append_args(message, DBUS_TYPE_STRING, &param, DBUS_TYPE_INVALID)) {
        D printf("Dbus append args failed\n");
        dbus_message_unref(message);
        dbus_connection_unref(connection);
        return FALSE;
    }
    
    if (reply_msg) {
        dbus_error_init(&error);
        gint timeout_ms = 500;
        DBusMessage *reply = dbus_connection_send_with_reply_and_block(connection, message, timeout_ms, &error);
        if G_UNLIKELY(!reply) {
            D printf("Dbus reply error: %s: %s\n", error.name, error.message);
            dbus_error_free(&error);
            ret = FALSE;
        } else {
            DBusMessageIter args;
            dbus_message_iter_init(reply, &args);
            do {
                if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_STRING) {
                    dbus_message_iter_get_basic(&args, reply_msg);
                    break;
                }
            } while (dbus_message_iter_next(&args));
            dbus_message_unref(reply);
        }
    } else {
        dbus_connection_send(connection, message, NULL);
        dbus_connection_flush(connection);
    }
    dbus_message_unref(message);
    dbus_connection_unref(connection);
    return ret;
}

/**
 * Get screen orientation
 * @return 'U', 'L', 'R' or zero on failure
 */
gchar get_orientation(void) {
    const gchar *dest = "com.lab126.winmgr";
    const gchar *reply_method = "getorientationLockStr";
    gchar *reply = NULL;
    gboolean ret = dbus_send(dest, reply_method, NULL, &reply);
    if G_LIKELY(ret && reply) {
        D printf("screen orientation: %s\n", reply);
        gchar orientation = reply[0];
        if (orientation == 'U' || orientation == 'L' || orientation == 'R') {
            return orientation;
        }
    }
    return 0;
}

/**
 * Set screen orientation
 * @param orientation One of 'U', 'L', 'R'
 * @return True on success, false otherwise
 */
gboolean set_orientation(gchar orientation) {
    if (orientation != 'U' && orientation != 'L' && orientation != 'R') {
        return FALSE;
    }
    gchar param[2] = { 0 };
    param[0] = orientation;
    const gchar *dest = "com.lab126.winmgr";
    const gchar *query_method = "setorientationLockStr";
    gboolean ret = dbus_send(dest, query_method, param, NULL);
    if G_LIKELY(ret) {
        D printf("screen orientation set to %c\n", orientation);
    }
    return ret;
}

#else

gchar get_orientation(void) {
    D printf("dbus support not compiled\n");
    return 0;
}

gboolean set_orientation(gchar orientation) {
    UNUSED(orientation);
    D printf("dbus support not compiled\n");
    return FALSE;
}

#endif

/**
 * Initialize orientation settings, set orientation if needed
 */
void orientation_init(void) {
    // save current screen orientation
    conf->orientation_saved = get_orientation();
    // set orientation from config if any
    if (conf->orientation > 0) {
        if (conf->orientation != conf->orientation_saved) {
            set_orientation(conf->orientation);
        }
    } else {
        conf->orientation = conf->orientation_saved;
    }
}

/**
 * Restore initial screen orientation
 */
void orientation_restore(void) {
    if (conf->orientation_saved > 0 && conf->orientation_saved != conf->orientation) {
        set_orientation(conf->orientation_saved);
    }
}

/**
 * Grab/ungrab keyboard
 * @param window Gdk window
 * @param grab Grab if true, ungrab if false
 * @return True on success, false otherwise
 */
gboolean keyboard_grab(GdkWindow *window, gboolean grab) {
    GdkGrabStatus ret = GDK_GRAB_SUCCESS;
    
#if GTK_CHECK_VERSION(3,20,0)
    GdkDisplay *display = gdk_display_get_default();
    GdkSeat *seat = gdk_display_get_default_seat(display);
    if (!seat) {
        D printf("Getting default seat failed\n");
        return FALSE;
    }
    if (grab) {
        ret = gdk_seat_grab(seat, window, GDK_SEAT_CAPABILITY_KEYBOARD, TRUE, NULL, NULL, NULL, NULL);
    } else {
        gdk_seat_ungrab(seat);
    }
    
#elif GTK_CHECK_VERSION(3,0,0)
    GdkDisplay *display = gdk_display_get_default();
    GdkDeviceManager *manager = gdk_display_get_device_manager(display);
    if (!manager) {
        D printf("Getting display manager failed\n");
        return FALSE;
    }
    GdkDevice *pointer = gdk_device_manager_get_client_pointer(manager);
    if (!pointer) {
        D printf("Getting pointer failed\n");
        return FALSE;
    }
    GdkDevice *keyboard = gdk_device_get_associated_device(pointer);
    if (!keyboard || gdk_device_get_source(keyboard) != GDK_SOURCE_KEYBOARD) {
        D printf("Getting keyboard failed\n");
        return FALSE;
    }
    if (grab) {
        ret = gdk_device_grab(keyboard, window, GDK_OWNERSHIP_NONE, TRUE, GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK, NULL, GDK_CURRENT_TIME);
    } else {
        gdk_device_ungrab(keyboard, GDK_CURRENT_TIME);
    }
    
#else
    if (grab) {
        ret = gdk_keyboard_grab(window, FALSE, GDK_CURRENT_TIME);
    } else {
        gdk_keyboard_ungrab(GDK_CURRENT_TIME);
    }
#endif
    if (ret != GDK_GRAB_SUCCESS) {
        D printf("Keyboard grabbing failed (%d)\n", ret);
        return FALSE;
    }
    return TRUE;
}

/**
 * Grab keyboard callback
 * @param widget Calling widget
 * @param event Gdk event
 * @return Always false to propagate event
 */
gboolean grab_keyboard_cb(GtkWidget *widget, GdkEvent *event, gpointer data) {
    UNUSED(event);
    UNUSED(data);
    keyboard_grab(gtk_widget_get_window(widget), TRUE);
    return FALSE;
}
