/*
 **  Oricutron
 **  Copyright (C) 2009-2014 Peter Gordon
 **
 **  This program is free software; you can redistribute it and/or
 **  modify it under the terms of the GNU General Public License
 **  as published by the Free Software Foundation, version 2
 **  of the License.
 **
 **  This program is distributed in the hope that it will be useful,
 **  but WITHOUT ANY WARRANTY; without even the implied warranty of
 **  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 **  GNU General Public License for more details.
 **
 **  You should have received a copy of the GNU General Public License
 **  along with this program; if not, write to the Free Software
 **  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 **
 **  visual keyboard header
 */
#ifndef KEYBOARD_H
#define KEYBOARD_H

struct kbdkey
{
  int x, y, w, h;
  int highlight;
  int highlightfade;
  SDL_COMPAT_KEY keysim;
  SDL_COMPAT_KEY keysimshifted;
  int is_mod_key;
};

struct keyboard_mapping
{
  SDL_COMPAT_KEY host_keys[65];
  SDL_COMPAT_KEY oric_keys[65];
  SDL_COMPAT_KEY nb_map;
};


int kbd_init( struct machine *oric );

SDL_bool keyboard_event( SDL_Event *ev, struct machine *oric, SDL_bool *needrender );

void release_sticky_keys();

void add_to_keyboard_mapping( struct keyboard_mapping *map, SDL_COMPAT_KEY host_key, SDL_COMPAT_KEY oric_key );

void reset_keyboard_mapping( struct keyboard_mapping *map );

SDL_bool save_keyboard_mapping( struct machine *oric, char *filename );

SDL_bool load_keyboard_mapping( struct machine *oric, char *filename );

#endif
