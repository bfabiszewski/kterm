/* parse_config.c
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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"

/**
 * Parse kterm config
 * @return KTconf structure or NULL
 */
KTconf *parse_config(void) {
    
    D printf("Parsing config file\n");
    
    gchar conf_path[PATH_MAX];
    conf_path[0] = '\0';
    
    // if kterm config is not found
    if (access(CONFIG_FULL_PATH, R_OK) == 0) {
        snprintf(conf_path, sizeof(conf_path), "%s", CONFIG_FULL_PATH);
    } else {
        // set path to kterm binary's path
        gchar self[PATH_MAX], *s;
        gssize len;
        if ((len = readlink("/proc/self/exe", self, sizeof(self) - 1)) != -1) {
            self[len] = '\0';
            if ((s = strrchr(self, '/')) != NULL) {
                *s = '\0';
                snprintf(conf_path, sizeof(conf_path), "%s/%s", self, CONFIG_FILE);
            }
        }
    }
    D printf("config: %s\n", conf_path);
    
    KTconf *conf = NULL;
    if ((conf = g_malloc0(sizeof(KTconf))) == NULL) {
        D printf("Memory allocation failed\n");
        exit(1);
    }
    
    // defaults
    conf->kb_on = 1;
    conf->color_reversed = FALSE;
    conf->font_size = VTE_FONT_SIZE;
    snprintf(conf->font_family, sizeof(conf->font_family), "%s", VTE_FONT_FAMILY);
    snprintf(conf->encoding, sizeof(conf->encoding), "%s", VTE_ENCODING);
    snprintf(conf->kb_conf_path, sizeof(conf->kb_conf_path), "%s", KB_FULL_PATH);
    conf->orientation = 0;
    
    FILE *fp;
    if ((fp = fopen(conf_path, "r")) == NULL) {
        D printf("No config file\n");
        return conf;
    }
    
    gchar buf[PATH_MAX];
    while (fgets(buf, sizeof(buf), fp)) {
        if (buf[0] == '#' || buf[0] == '\n') { continue; }
        if (!strncmp(buf, "keyboard", 8)) {
            gint kb_on = -1;
            sscanf(buf, "keyboard = %i", &kb_on);
            if (kb_on == 0 || kb_on == 1) {
                conf->kb_on = kb_on;
                D printf("kb_on = %i\n", conf->kb_on);
            }
        }
        else if (!strncmp(buf, "color_scheme", 12)) {
            gint color_reversed = -1;
            sscanf(buf, "color_scheme = %i", &color_reversed);
            if (color_reversed == 0 || color_reversed == 1) {
                conf->color_reversed = color_reversed;
                D printf("color_scheme = %i\n", conf->color_reversed);
            }
        }
        else if (!strncmp(buf, "font_family", 11)) {
            gchar str[256];
            sscanf(buf, "font_family = \"%[^\"\n\r]\"", str);
            snprintf(conf->font_family, sizeof(conf->font_family), "%s", str);
            D printf("font_family = %s\n", conf->font_family);
        }
        else if (!strncmp(buf, "font_size", 9)) {
            guint font_size = 0;
            sscanf(buf, "font_size = %u", &font_size);
            if (font_size > 0) {
                conf->font_size = font_size;
                D printf("font_size = %u\n", conf->font_size);
            }
        }
        else if (!strncmp(buf, "encoding", 8)) {
            gchar str[256];
            sscanf(buf, "encoding = \"%[^\"\n\r]\"", str);
            snprintf(conf->encoding, sizeof(conf->encoding), "%s", str);
            D printf("encoding = %s\n", conf->encoding);
        }
        else if (!strncmp(buf, "kb_conf_path", 12)) {
            gchar str2[PATH_MAX];
            sscanf(buf, "kb_conf_path = \"%[^\"\n\r]\"", str2); // need double quotes around path
            snprintf(conf->kb_conf_path, sizeof(conf->kb_conf_path), "%s", str2);
            D printf("kb_conf_path = %s\n", conf->kb_conf_path);
        }
        else if (!strncmp(buf, "orientation", 11)) {
            gchar orientation = 0;
            sscanf(buf, "orientation = %c", &orientation);
            if (orientation == 'U' || orientation == 'R' || orientation == 'L') {
                conf->orientation = orientation;
                D printf("orientation = %c\n", conf->orientation);
            }
        }
        else if (!strncmp(buf, "cursor_shape", 12)) {
            gchar cursor_shape = 0;
            sscanf(buf, "cursor_shape = %c", &cursor_shape);
            if (cursor_shape == 'B' || cursor_shape == 'I' || cursor_shape == 'U') {
                conf->cursor_shape = cursor_shape;
                D printf("cursor_shape = %c\n", conf->cursor_shape);
            }
        }
    }
    
    fclose(fp);
    
    return conf;
} 
