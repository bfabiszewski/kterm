/* parse_config.c
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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"

kterm_conf *parse_config(){
  char buf[256];
  FILE *fp;
  kterm_conf *conf;

  D printf("Parsing config file\n");

  char conf_path[4096];
  char self[4096], *s;

  // if kb config is not found
  if(access(CONFIG_FULL_PATH, R_OK) == 0){
    snprintf(conf_path, sizeof(conf_path), "%s", CONFIG_FULL_PATH);
  } else {
    // set path to kterm binary's path
    if (readlink("/proc/self/exe", self, sizeof(self)-1) != -1){
      if((s = strrchr(self, '/')) != NULL){
        *s = '\0';
        snprintf(conf_path, sizeof(conf_path), "%s/%s", self, CONFIG_FILE);
      }
    }
  }
  D printf("config: %s\n", conf_path);

  if((conf = (kterm_conf *) malloc(sizeof(kterm_conf))) == NULL) 
    return NULL;

  // defaults
  conf->kb_on = 1;
  conf->color_scheme = VTE_SCHEME_LIGHT;
  conf->font_size = VTE_FONT_SIZE;
  snprintf(conf->font_family, sizeof(conf->font_family), "%s", VTE_FONT_FAMILY);

  if((fp = fopen(conf_path, "r")) == NULL){
    D printf("No config file\n"); 
    return conf;
  }

  while(fgets(buf, 255, fp)){
    if(buf[0] == '#') continue;
    if(!strncmp(buf, "keyboard", 8)){
      sscanf(buf, "keyboard = %u", &conf->kb_on);
      D printf("kb_on = %u\n", conf->kb_on);
    }
    else if(!strncmp(buf, "color_scheme", 12)){
      sscanf(buf, "color_scheme = %u", &conf->color_scheme);
      D printf("color_scheme = %u\n", conf->color_scheme);
    }
    else if(!strncmp(buf, "font_family", 11)){
      char str[255];
      sscanf(buf, "font_family = \"%[^\"\n\r]\"", str);
      snprintf(conf->font_family, sizeof(conf->font_family), "%s", str);
      D printf("font_family = %s\n", conf->font_family);
    }
    else if(!strncmp(buf, "font_size", 9)){
      sscanf(buf, "font_size = %u", &conf->font_size);
      D printf("font_size = %u\n", conf->font_size);
    }
  }
  
  fclose(fp);
  
  return conf;
} 
