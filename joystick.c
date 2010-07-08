/*
**  Oricutron
**  Copyright (C) 2009-2010 Peter Gordon
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
**  Joystick emulation
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "system.h"
#include "6502.h"
#include "via.h"
#include "8912.h"
#include "gui.h"
#include "disk.h"
#include "monitor.h"
#include "machine.h"
#include "joystick.h"

struct keyjoydef
{
  char *id;
  Sint16 sym;
};

static struct keyjoydef keyjoytab[] = { { "BACKSPACE",   SDLK_BACKSPACE },
                                        { "TAB",         SDLK_TAB },
                                        { "CLEAR",       SDLK_CLEAR },
                                        { "RETURN",      SDLK_RETURN },
                                        { "ENTER",       SDLK_RETURN },
                                        { "PAUSE",       SDLK_PAUSE },
                                        { "ESCAPE",      SDLK_ESCAPE },
                                        { "SPACE",       SDLK_SPACE },
                                        { "HELP",        SDLK_HELP },
                                        { "DELETE",      SDLK_DELETE },
                                        { "DEL",         SDLK_DELETE },
                                        { "LSHIFT",      SDLK_LSHIFT },
                                        { "RSHIFT",      SDLK_RSHIFT },
                                        { "RCTRL",       SDLK_RCTRL },
                                        { "LCTRL",       SDLK_LCTRL },
                                        { "RALT",        SDLK_RALT },
                                        { "LALT",        SDLK_LALT },
                                        { "KP0",         SDLK_KP0 },
                                        { "KP1",         SDLK_KP1 },
                                        { "KP2",         SDLK_KP2 },
                                        { "KP3",         SDLK_KP3 },
                                        { "KP4",         SDLK_KP4 },
                                        { "KP5",         SDLK_KP5 },
                                        { "KP6",         SDLK_KP6 },
                                        { "KP7",         SDLK_KP7 },
                                        { "KP8",         SDLK_KP8 },
                                        { "KP9",         SDLK_KP9 },
                                        { "KP_PERIOD",   SDLK_KP_PERIOD },
                                        { "KP_FULLSTOP", SDLK_KP_PERIOD },
                                        { "KP_DIVIDE",   SDLK_KP_DIVIDE },
                                        { "KP_MULTIPLY", SDLK_KP_MULTIPLY },
                                        { "KP_MINUS",    SDLK_KP_MINUS },
                                        { "KP_PLUS",     SDLK_KP_PLUS },
                                        { "KP_ENTER",    SDLK_KP_ENTER },
                                        { "KP_EQUALS",   SDLK_KP_EQUALS },
                                        { "UP",          SDLK_UP },
                                        { "DOWN",        SDLK_DOWN },
                                        { "LEFT",        SDLK_LEFT },
                                        { "RIGHT",       SDLK_RIGHT },
                                        { "INSERT",      SDLK_INSERT },
                                        { "HOME",        SDLK_HOME },
                                        { "END",         SDLK_END },
                                        { "PAGEUP",      SDLK_PAGEUP },
                                        { "PAGEDOWN",    SDLK_PAGEDOWN },
                                        { NULL,          0 } };

static SDL_bool      joysubinited = SDL_FALSE;
static SDL_Joystick *joys[10] = { NULL, NULL, NULL, NULL, NULL,
                                  NULL, NULL, NULL, NULL, NULL };

static SDL_bool is_real_joystick( Sint16 joymode )
{
  if( ( joymode >= JOYMODE_SDL0 ) && ( joymode <= JOYMODE_SDL9 ) )
    return SDL_TRUE;
  return SDL_FALSE;
}

static void close_joysticks( struct machine *oric )
{
  if( joysubinited )
  {
    if( oric->sdljoy_a )
    {
      SDL_JoystickClose( oric->sdljoy_a );
      if( oric->sdljoy_b == oric->sdljoy_a )
        oric->sdljoy_b = NULL;
      oric->sdljoy_a = NULL;
    }
    if( oric->sdljoy_b ) 
    {
      SDL_JoystickClose( oric->sdljoy_b );
      oric->sdljoy_b = NULL;
    }
  }
}

// Return an SDL keysym corresponding to the specified name (or 0)
Sint16 joy_keyname_to_sym( char *name )
{
  Sint32 i;
  char c;

  // Just a single char?
  if( name[1] == 0 )
  {
    c = tolower( name[0] );
    if( ( c >= 32 ) && ( c < 127 ) ) return c;
    return 0;
  }

  // Look for a matching named key
  for( i=0; keyjoytab[i].id; i++ )
  {
    if( strcasecmp( keyjoytab[i].id, name ) == 0 )
      return keyjoytab[i].sym;
  }

  return 0;
}

static void dojoysetup( struct machine *oric, Sint16 mode_a, Sint16 mode_b )
{
  close_joysticks( oric );

  if( (!is_real_joystick( mode_a )) && (!is_real_joystick( mode_b )) )
    return;

  if( !joysubinited )
  {
    if( SDL_InitSubSystem( SDL_INIT_JOYSTICK ) != 0 )
      return;

    joysubinited = SDL_TRUE;
  }

  if( is_real_joystick( mode_a ) )
    oric->sdljoy_a = SDL_JoystickOpen( mode_a - JOYMODE_SDL0 );

  if( is_real_joystick( mode_b ) )
  {
    if( mode_b == mode_a )
    {
      oric->sdljoy_b = oric->sdljoy_a;
    } else {
      oric->sdljoy_b = SDL_JoystickOpen( mode_b - JOYMODE_SDL0 );
    }
  }
}

void joy_setup( struct machine *oric )
{
  if( oric->type == MACH_TELESTRAT )
    dojoysetup( oric, oric->telejoymode_a, oric->telejoymode_b );
  else
    dojoysetup( oric, oric->joymode_a, oric->joymode_b );
}



SDL_bool init_joy( struct machine *oric )
{
  return SDL_TRUE;
}

void shut_joy( struct machine *oric )
{
  close_joysticks( oric );
}
