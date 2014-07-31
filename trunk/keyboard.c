/*
 **  Oricutron
 **  Copyright (C) 2009-2012 Peter Gordon
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
 **  visual keyboard source
 */


#include "system.h"
#include "6502.h"
#include "via.h"
#include "8912.h"
#include "gui.h"
#include "disk.h"
#include "monitor.h"
#include "6551.h"
#include "machine.h"
#include "filereq.h"
#include "render_sw.h"
#include "render_sw8.h"
#include "render_gl.h"
#include "render_null.h"
#include "ula.h"
#include "tape.h"
#include "joystick.h"
#include "snapshot.h"
#include "msgbox.h"
#include "keyboard.h"


static struct kbdkey kbd_atmos[65], kbd_oric1[65], kbd_pravetz[65];

SDL_Surface* CreateSurface( int width , int height )
{
    uint32_t rmask , gmask , bmask , amask;
    SDL_Surface* surface;

    /* SDL interprets each pixel as a 32-bit number, so our masks must depend
     on the endianness (byte order) of the machine */
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    rmask = 0xff000000;
    gmask = 0x00ff0000;
    bmask = 0x0000ff00;
    amask = 0x000000ff;
#else
    rmask = 0x000000ff;
    gmask = 0x0000ff00;
    bmask = 0x00ff0000;
    amask = 0xff000000;
#endif

    surface = SDL_CreateRGBSurface( 0 , width , height , 32 , rmask , gmask , bmask , amask );
    if( surface == NULL )
    {
        ( void )fprintf(stderr, "CreateRGBSurface failed: %s\n", SDL_GetError() );
        exit(EXIT_FAILURE);
    }

	return surface;
}

#define KEYSIM_FLAG ((unsigned short)0x8000)
#define KEYSIM_MASK ((unsigned short)0x7FFF)

static unsigned short keyMap[] = {
    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\\',
    SDLK_ESCAPE, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', SDLK_BACKSPACE,
    SDLK_LCTRL, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', SDLK_RETURN,
    SDLK_LSHIFT, 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', SDLK_RSHIFT,
    SDLK_LEFT, SDLK_DOWN, ' ', SDLK_UP, SDLK_RIGHT, SDLK_LALT, ' ', ' ', ' ', ' ', ' ', ' ' };

static unsigned short keyMapPravetz[] = {
    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', KEYSIM_FLAG|';', '-', ';',
    SDLK_ESCAPE, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', KEYSIM_FLAG|'2', '\\', SDLK_BACKSPACE,
    SDLK_LCTRL, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', '[', ']', SDLK_RETURN,
    SDLK_LSHIFT, 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', SDLK_RSHIFT,
    SDLK_LEFT, SDLK_DOWN, ' ', SDLK_UP, SDLK_RIGHT, SDLK_LALT, KEYSIM_FLAG|'6', ' ', ' ', ' ', ' ', ' ' };

static unsigned short keyMapShiftedPravetz[] = {
    '1', '\'', '3', '4', '5', '7', KEYSIM_FLAG|'\'', '9', '0', '-', '8', KEYSIM_FLAG|'=', '=',
    SDLK_ESCAPE, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '~', '\\', SDLK_BACKSPACE,
    SDLK_LCTRL, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', '[', ']', SDLK_RETURN,
    SDLK_LSHIFT, 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', SDLK_RSHIFT,
    SDLK_LEFT, SDLK_DOWN, ' ', SDLK_UP, SDLK_RIGHT, SDLK_LALT, '`', ' ', ' ', ' ', ' ', ' ' };

enum {
    MOD_CTRL = 0,
    MOD_LSHIFT,
    MOD_RSHIFT,
    MOD_FUNCT,
    MODKEY_MAX
};

static unsigned short modKeys[MODKEY_MAX] = { SDLK_LCTRL, SDLK_LSHIFT, SDLK_RSHIFT, SDLK_LALT };
static char *modKeyNames[MODKEY_MAX] = { "Ctrl", "Left shift", "Right shift", "Funct" };
static const int modKeyMax = MODKEY_MAX;
static SDL_bool modKeyPressed[MODKEY_MAX];
static SDL_bool modKeyFakePressed[MODKEY_MAX];

int kbd_init( struct machine *oric )
{
  int i, j;

  memset(&modKeyPressed[0], 0, sizeof(modKeyPressed));
  memset(&modKeyFakePressed[0], 0, sizeof(modKeyFakePressed));

  oric->keyboard_mapping.nb_map = 0;

  for( i=0; i<62; i++ )
  {
    kbd_atmos[i].w = 43;
    kbd_atmos[i].h = 43;
    kbd_atmos[i].highlight = 0;
    kbd_atmos[i].highlightfade = 0;
    kbd_atmos[i].is_mod_key = 0;
    kbd_atmos[i].keysim = keyMap[i];

    kbd_oric1[i].w = 43;
    kbd_oric1[i].h = 43;
    kbd_oric1[i].highlight = 0;
    kbd_oric1[i].highlightfade = 0;
    kbd_oric1[i].is_mod_key = 0;
    kbd_oric1[i].keysim = keyMap[i];

    kbd_pravetz[i].w = 41;
    kbd_pravetz[i].h = 41;
    kbd_pravetz[i].highlight = 0;
    kbd_pravetz[i].highlightfade = 0;
    kbd_pravetz[i].is_mod_key = 0;
    kbd_pravetz[i].keysim = keyMapPravetz[i];
    kbd_pravetz[i].keysimshifted = keyMapShiftedPravetz[i];


    // mod keys
      for (j = 0; j < modKeyMax; j++) {
          if (kbd_atmos[i].keysim == modKeys[j]) {
              kbd_atmos[i].is_mod_key = 1;
              kbd_oric1[i].is_mod_key = 1;
              kbd_pravetz[i].is_mod_key = 1;
          }
      }
  }



  for( i=0; i<13; i++ )
  {
    kbd_atmos[i].x = i*43+47;
    kbd_atmos[i].y = 10;

    kbd_oric1[i].x = i*43+40;
    kbd_oric1[i].y = 15;

    kbd_pravetz[i].x = i*42+47;
    kbd_pravetz[i].y = 10;
  }
  for( i=13; i<27; i++ )
  {
    kbd_atmos[i].x = (i-13)*43+23;
    kbd_atmos[i].y = 53;

    kbd_oric1[i].x = (i-13)*43+16;
    kbd_oric1[i].y = 58;

    kbd_pravetz[i].x = (i-13)*42+25;
    kbd_pravetz[i].y = 51;
  }
  for( i=27; i<40; i++ )
  {
    kbd_atmos[i].x = (i-27)*43+38;
    kbd_atmos[i].y = 96;

    kbd_oric1[i].x = (i-27)*43+27;
    kbd_oric1[i].y = 101;

    kbd_pravetz[i].x = (i-27)*43+40;
    kbd_pravetz[i].y = 93;
  }
  kbd_atmos[39].w = 65;
  kbd_oric1[39].w = 87;

  kbd_atmos[40].x = 38;
  kbd_atmos[40].y = 139;
  kbd_atmos[40].w = 65;

  for( i=41; i<52; i++ )
  {
    kbd_atmos[i].x = (i-41)*43+103;
    kbd_atmos[i].y = 139;
  }
  kbd_atmos[51].w = 65;

  kbd_atmos[52].x = 60;
  kbd_atmos[52].y = 182;
  kbd_atmos[53].x = 103;
  kbd_atmos[53].y = 182;
  kbd_atmos[54].x = 146;
  kbd_atmos[54].y = 182;
  kbd_atmos[54].w = 344;
  kbd_atmos[55].x = 490;
  kbd_atmos[55].y = 182;
  kbd_atmos[56].x = 533;
  kbd_atmos[56].y = 182;
  kbd_atmos[57].x = 576;
  kbd_atmos[57].y = 182;
  kbd_atmos[58].x = -1;

  for( i=40; i<52; i++ )
  {
    kbd_oric1[i].x = (i-40)*43+49;
    kbd_oric1[i].y = 144;
  }

  kbd_oric1[52].x = 135;
  kbd_oric1[52].y = 187;
  kbd_oric1[53].x = 178;
  kbd_oric1[53].y = 187;
  kbd_oric1[54].x = 221;
  kbd_oric1[54].y = 187;
  kbd_oric1[54].w = 215;
  kbd_oric1[55].x = 436;
  kbd_oric1[55].y = 187;
  kbd_oric1[56].x = 479;
  kbd_oric1[56].y = 187;
  kbd_oric1[57].x = -1;

  for( i=41; i<52; i++ )
  {
    kbd_pravetz[i].x = (i-41)*43+103;
    kbd_pravetz[i].y = 137;
  }
  // backspace
  kbd_pravetz[26].x = 38;
  kbd_pravetz[26].y = 180;
  // enter
  kbd_pravetz[39].w = 65;
  kbd_pravetz[40].x = 38;
  kbd_pravetz[40].y = 137;
  kbd_pravetz[40].w = 65;
  kbd_pravetz[51].w = 65;

  kbd_pravetz[52].x = 82;
  kbd_pravetz[52].y = 180;
  kbd_pravetz[53].x = 125;
  kbd_pravetz[53].y = 180;
  kbd_pravetz[54].x = 168;
  kbd_pravetz[54].y = 180;
  kbd_pravetz[54].w = 215+88;
  kbd_pravetz[55].x = 436+40;
  kbd_pravetz[55].y = 180;
  kbd_pravetz[56].x = 479+40;
  kbd_pravetz[56].y = 180;
  kbd_pravetz[58].x = (13)*42+25;
  kbd_pravetz[58].y = 51;
  kbd_pravetz[59].x = 479+40 +42;
  kbd_pravetz[59].y = 180;
  kbd_pravetz[60].x = -1;

  return 1;
}

static SDL_bool defining_key_map = SDL_FALSE;
static struct kbdkey * current_key = NULL;
static SDL_bool release_keys = SDL_FALSE;


// This is the event handler for when you are in the menus
SDL_bool keyboard_event( SDL_Event *ev, struct machine *oric, SDL_bool *needrender )
{
    SDL_bool done = SDL_FALSE;
    SDL_bool lshifted = modKeyPressed[MOD_LSHIFT];
    SDL_bool rshifted = modKeyPressed[MOD_RSHIFT];

    int i, x, y, current_key_num = -1;
    static char tmp[64];
    struct kbdkey * kbd;

    if (release_keys)
    {
      x = 0;
      for (i=0; i<modKeyMax; i++)
      {
        if (modKeyPressed[i])
        {
          modKeyPressed[i] = SDL_FALSE;
          ay_keypress( &oric->ay, modKeys[i], SDL_FALSE );
          snprintf(tmp, sizeof(tmp), "%s released.", modKeyNames[i]);
          do_popup(oric, tmp);
          x++;
        } else if (modKeyFakePressed[i]) {
          modKeyFakePressed[i] = SDL_FALSE;
          ay_keypress( &oric->ay, modKeys[i], SDL_FALSE );
        }
      }

      if (x > 1)
        do_popup(oric, "All keys released");

      release_keys = SDL_FALSE;
    }

    x = -1;
    y = -1;
    switch( ev->type )
    {
        case SDL_MOUSEBUTTONDOWN:
            x = ev->button.x;
            y = ev->button.y - 480;

            switch( oric->type )
            {
                case MACH_PRAVETZ:
                    kbd = kbd_pravetz;
                    break;
                case MACH_ORIC1:
                case MACH_ORIC1_16K:
                    kbd = kbd_oric1;
                    break;
                default:
                    kbd = kbd_atmos;
                    break;
            }

            // find the visual key under the mouse pointer
            current_key = NULL;
            for(i=0; i<62; i++) {
                if ((x > kbd[i].x) && (x < kbd[i].x + kbd[i].w) &&
                    (y > kbd[i].y) && (y < kbd[i].y + kbd[i].h)) {
                    current_key = &(kbd[i]);
                    current_key_num = i;
                    //if (ev->type == SDL_MOUSEBUTTONDOWN)
                    //   printf("Key %d pressed : keysim %d (%c)\n",
                    //          i, current_key->keysim, (char)(current_key->keysim));
                    break;
                }
            }

            if (current_key != NULL) {
                // check which button was pressed
                if( ev->button.button == SDL_BUTTON_LEFT )
                {
                    if(oric->define_mapping) {
                        do_popup( oric, "Press the key you want to use." );
                        defining_key_map = SDL_TRUE;
                    } else {
                        // manage mod keys
                        if (current_key->is_mod_key && oric->sticky_mod_keys) {
                          for (i=0; i<modKeyMax; i++) {
                            if (current_key->keysim == modKeys[i])
                              break;
                          }

                          if (i < modKeyMax) {
                            if (modKeyPressed[i]) {
                              modKeyPressed[i] = SDL_FALSE;
                              snprintf(tmp, sizeof(tmp), "%s released.", modKeyNames[i]);
                            } else {
                              modKeyPressed[i] = SDL_TRUE;
                              snprintf(tmp, sizeof(tmp), "%s pressed.", modKeyNames[i]);
                            }
                            do_popup(oric, tmp);
                            ay_keypress( &oric->ay, current_key->keysim, modKeyPressed[i] );
                            current_key = NULL;
                            return done;
                          }
                        }

                        // send the key to the Oric
                        switch( oric->type )
                        {
                            case MACH_PRAVETZ:
                                if (lshifted) {
                                    if (current_key_num == 24)
                                        queuekeys("\x60");
                                    if (KEYSIM_FLAG & current_key->keysimshifted)
                                        ay_keypress( &oric->ay, modKeys[MOD_LSHIFT], SDL_FALSE );
                                    ay_keypress( &oric->ay, KEYSIM_MASK & current_key->keysimshifted, SDL_TRUE );
                                } else if (rshifted) {
                                    if (current_key_num == 24)
                                        queuekeys("\x60");
                                    if (KEYSIM_FLAG & current_key->keysimshifted)
                                        ay_keypress( &oric->ay, modKeys[MOD_RSHIFT], SDL_FALSE );
                                    ay_keypress( &oric->ay, KEYSIM_MASK & current_key->keysimshifted, SDL_TRUE );
                                } else {
                                    if (current_key_num == 59) {
                                        queuekeys("\x14");
                                    } else if (KEYSIM_FLAG & current_key->keysim) {
                                        ay_keypress( &oric->ay, modKeys[MOD_LSHIFT], SDL_TRUE );
                                        ay_keypress( &oric->ay, KEYSIM_MASK & current_key->keysim, SDL_TRUE );
                                        modKeyFakePressed[MOD_LSHIFT] = SDL_TRUE;
                                    } else {
                                        ay_keypress( &oric->ay, KEYSIM_MASK & current_key->keysim, SDL_TRUE );
                                    }
                                }
                                break;
                            case MACH_ORIC1:
                            case MACH_ORIC1_16K:
                            case MACH_ATMOS:
                            default:
                                ay_keypress( &oric->ay, current_key->keysim, SDL_TRUE );
                                break;
                        }

                        // start releasing mod keys if need be
                        release_keys = SDL_TRUE;
                    }
                }
            }
            break;

        case SDL_MOUSEBUTTONUP:
            if ((current_key == NULL) || (defining_key_map))
                break;

            // send the key to the Oric
            switch( oric->type )
            {
                case MACH_PRAVETZ:
                    if (lshifted || rshifted) {
                        ay_keypress( &oric->ay, KEYSIM_MASK & current_key->keysimshifted, SDL_FALSE );
                    } else {
                        ay_keypress( &oric->ay, KEYSIM_MASK & current_key->keysim, SDL_FALSE );
                    }
                    break;
                case MACH_ORIC1:
                case MACH_ORIC1_16K:
                case MACH_ATMOS:
                default:
                    ay_keypress( &oric->ay, current_key->keysim, SDL_FALSE );
                    break;
            }

            current_key = NULL;
            break;

        case SDL_KEYUP:
            if (defining_key_map) {
                SDL_KeyboardEvent *kbd_evt = (SDL_KeyboardEvent *) ev;
                add_to_keyboard_mapping( &(oric->keyboard_mapping), kbd_evt->keysym.sym, current_key->keysim );
                do_popup( oric, "Key mapping done.");
                defining_key_map = SDL_FALSE;
                current_key = NULL;
            }
            break;

    }
    return done;
}

void release_sticky_keys()
{
    release_keys = SDL_TRUE;
}

void add_to_keyboard_mapping( struct keyboard_mapping *map, unsigned short host_key, unsigned short oric_key )
{
    // search for a matching host key
    int index = 0;
    while (index < map->nb_map) {
        if (map->host_keys[index] == host_key) {
            // found one: update it
            map->oric_keys[index] = oric_key;
            return;
        } else {
            // move through the list
            index++;
        }
    }
    // add a new mapping
    map->host_keys[map->nb_map] = host_key;
    map->oric_keys[map->nb_map] = oric_key;
    map->nb_map++;
}

void reset_keyboard_mapping( struct keyboard_mapping *map )
{
    map->nb_map = 0;
}

SDL_bool save_keyboard_mapping( struct machine *oric, char *filename )
{
    SDL_bool ok = SDL_TRUE;
    FILE *f = NULL;

    f = fopen(filename, "wt");
    if (!f)
    {
        msgbox(oric, MSGBOX_OK, "Unable to create keyboard mapping file (1)");
        return SDL_FALSE;
    }

    if (fputs("# SDL_key_symbol_for_host : SDL_key_symbol_for_oric\n", f) < 0)
    {
        msgbox(oric, MSGBOX_OK, "Unable to write to keyboard mapping file (2)");
        return SDL_FALSE;
    }
    if (fputs("# see http://www.libsdl.org/release/SDL-1.2.15/docs/html/sdlkey.html \n", f)  < 0)
    {
        msgbox(oric, MSGBOX_OK, "Unable to write to keyboard mapping file (2)");
        return SDL_FALSE;
    }

    // if we have any definition to save
    if (oric->keyboard_mapping.nb_map != 0) {
        int i;
        // loop through them
        for(i=0; i < oric->keyboard_mapping.nb_map; i++) {
            // print them in the file
            if (fprintf(f, "%d : %d\n", oric->keyboard_mapping.host_keys[i], oric->keyboard_mapping.oric_keys[i]) < 0)
            {
                msgbox(oric, MSGBOX_OK, "Unable to write to keyboard mapping file (3)");
                return SDL_FALSE;
            }
        }
    }

    if(fclose(f) == EOF) {
        msgbox(oric, MSGBOX_OK, "Unable to close keyboard mapping file (4)");
        return SDL_FALSE;
    }

    return ok;
}

SDL_bool load_keyboard_mapping( struct machine *oric, char *filename )
{
    SDL_bool ok = SDL_TRUE;
    FILE *f = NULL;
    char buf[4096];
    int host_key, oric_key;

    f = fopen(filename, "rt");
    if (!f)
    {
        msgbox(oric, MSGBOX_OK, "Unable to read keyboard mapping file (1)");
        return SDL_FALSE;
    }

    // first reset the mapping
    reset_keyboard_mapping(&(oric->keyboard_mapping));

    // while we are not finished reading the file
    while (!feof(f) && !ferror(f)) {
        // if we can read something
        if (fgets(buf, 4096, f)>0) {
            // if it's not a comment
            if (buf[0] != '#') {
                // if we can read a definition
                if (sscanf(buf, "%d : %d", &host_key, &oric_key)==2) {
                    // add it to the keyboard mapping
                    add_to_keyboard_mapping( &(oric->keyboard_mapping), host_key, oric_key);
                }
            }
        }
    }

    if(ferror(f)) {
        msgbox(oric, MSGBOX_OK, "Problem while reading from keyboard mapping file (2)");
        return SDL_FALSE;
    }

    if (sprintf(buf, "Read %d key mappings", oric->keyboard_mapping.nb_map) > 0)
        do_popup(oric, buf);

    if(fclose(f) == EOF) {
        msgbox(oric, MSGBOX_OK, "Unable to close keyboard mapping file (3)");
        return SDL_FALSE;
    }

    return ok;
}
