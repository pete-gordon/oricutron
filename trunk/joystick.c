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
#include "6551.h"
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

static SDL_bool joysubinited = SDL_FALSE;
Uint8 joystate_a[6], joystate_b[6];

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

    SDL_JoystickEventState( SDL_FALSE );
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

static SDL_bool dojoyevent( SDL_Event *ev, struct machine *oric, Sint16 mode, Uint8 *joystate, SDL_Joystick *sjoy )
{
  Sint32 i;
  Sint16 *kbtab;
  SDL_bool swallowit = SDL_FALSE;

  kbtab = oric->kbjoy2;
  switch( mode )
  {
    case JOYMODE_NONE:
    case JOYMODE_MOUSE:   // Telestrat only
      return SDL_FALSE;

    case JOYMODE_KB1:
      kbtab = oric->kbjoy1;
    case JOYMODE_KB2:
      switch( ev->type )
      {
        case SDL_KEYDOWN:
          for( i=0; i<6; i++ )
          {
            if( ev->key.keysym.sym == kbtab[i] )
            {
              joystate[i] = 1;
              swallowit = SDL_TRUE;
            }
          }
          break;
        
        case SDL_KEYUP:
          for( i=0; i<6; i++ )
          {
            if( ev->key.keysym.sym == kbtab[i] )
            {
              joystate[i] = 0;
              swallowit = SDL_TRUE;
            }
          }
          break;
      }
      break;
    
    case JOYMODE_SDL0:
    case JOYMODE_SDL1:
    case JOYMODE_SDL2:
    case JOYMODE_SDL3:
    case JOYMODE_SDL4:
    case JOYMODE_SDL5:
    case JOYMODE_SDL6:
    case JOYMODE_SDL7:
    case JOYMODE_SDL8:
    case JOYMODE_SDL9:
      if( !sjoy ) return SDL_FALSE;

      switch( ev->type )
      {
        case SDL_JOYAXISMOTION:
          if( ev->jaxis.which != (mode-JOYMODE_SDL0) ) return SDL_FALSE;
          swallowit = SDL_TRUE;
          switch( ev->jaxis.axis )
          {
            case 0: // left/right
              if( ev->jaxis.value < -3200 )
              {
                joystate[2] = 1;  // left
                joystate[3] = 0;
              } else if( ev->jaxis.value > 3200 ) {
                joystate[2] = 0;  // right
                joystate[3] = 1;
              } else {
                joystate[2] = 0;
                joystate[3] = 0;
              }
              break;
            
            case 1: // up/down
              if( ev->jaxis.value < -3200 )
              {
                joystate[0] = 1;  // up
                joystate[1] = 0;
              } else if( ev->jaxis.value > 3200 ) {
                joystate[0] = 0;  // down
                joystate[1] = 1;
              } else {
                joystate[0] = 0;
                joystate[0] = 0;
              }
              break;
          }
          break;

        case SDL_JOYBUTTONDOWN:
          if( ev->jbutton.which != (mode-JOYMODE_SDL0) ) return SDL_FALSE;
          joystate[4+(ev->jbutton.button&1)] = 1;
          swallowit = SDL_TRUE;
          break;

        case SDL_JOYBUTTONUP:
          if( ev->jbutton.which != (mode-JOYMODE_SDL0) ) return SDL_FALSE;
          joystate[4+(ev->jbutton.button&1)] = 0;
          swallowit = SDL_TRUE;
          break;
      }
      break;
  }

  return swallowit;
}

void joy_buildmask( struct machine *oric )
{
  Uint8 mkmask = 0xff;
  Uint8 joysel = oric->via.read_port_a( &oric->via );
  SDL_bool gimme_port_a = SDL_FALSE;

  switch( oric->joy_iface )
  {
    case JOYIFACE_ALTAI:
      if( joysel & 0x80 )
      {
        if( joystate_a[0] ) mkmask &= 0xef;
        if( joystate_a[1] ) mkmask &= 0xf7;
        if( joystate_a[2] ) mkmask &= 0xfe;
        if( joystate_a[3] ) mkmask &= 0xfd;
        if( joystate_a[4] ) mkmask &= 0xdf;
        gimme_port_a = SDL_TRUE;
      }

      if( joysel & 0x40 )
      {
        if( joystate_b[0] ) mkmask &= 0xef;
        if( joystate_b[1] ) mkmask &= 0xf7;
        if( joystate_b[2] ) mkmask &= 0xfe;
        if( joystate_b[3] ) mkmask &= 0xfd;
        if( joystate_b[4] ) mkmask &= 0xdf;
        gimme_port_a = SDL_TRUE;
      }
      break;
    
    case JOYIFACE_IJK:
      mkmask &= 0xdf;

      if( ((oric->via.ddrb & 0x10)==0) ||
          ((oric->via.read_port_b( &oric->via )&0x10)!=0) )
        break;
      
      gimme_port_a = SDL_TRUE;

      if( ( joysel & 0xc0 ) == 0xc0 ) break;

      if( joysel & 0x40 )
      {
        if( joystate_a[0] ) mkmask &= 0xef;
        if( joystate_a[1] ) mkmask &= 0xf7;
        if( joystate_a[2] ) mkmask &= 0xfd;
        if( joystate_a[3] ) mkmask &= 0xfe;
        if( joystate_a[4] ) mkmask &= 0xfb;
      }

      if( joysel & 0x80 )
      {
        if( joystate_b[0] ) mkmask &= 0xef;
        if( joystate_b[1] ) mkmask &= 0xf7;
        if( joystate_b[2] ) mkmask &= 0xfd;
        if( joystate_b[3] ) mkmask &= 0xfe;
        if( joystate_b[4] ) mkmask &= 0xfb;
      }
      break;
  }

  oric->porta_joy = mkmask;
  if( gimme_port_a )
  {
    oric->via.write_port_a( &oric->via, 0xff, mkmask );
    oric->porta_is_ay = SDL_FALSE;
  } else {
    if( !oric->porta_is_ay )
    {
      oric->via.write_port_a( &oric->via, 0xff, oric->porta_ay );
      oric->porta_is_ay = SDL_TRUE;
    }
  }
}

SDL_bool joy_filter_event( SDL_Event *ev, struct machine *oric )
{
  SDL_bool swallow_event;

  swallow_event  = dojoyevent( ev, oric, (oric->type==MACH_TELESTRAT) ? oric->telejoymode_a : oric->joymode_a, joystate_a, oric->sdljoy_a );
  swallow_event |= dojoyevent( ev, oric, (oric->type==MACH_TELESTRAT) ? oric->telejoymode_b : oric->joymode_b, joystate_b, oric->sdljoy_b );

  if( swallow_event )
  {
//    char testytesttest[64];

    joy_buildmask( oric );
/*
    sprintf( testytesttest, "A: %d%d%d%d-%d%d B: %d%d%d%d-%d%d",
      joystate_a[0], joystate_a[1], joystate_a[2], joystate_a[3],
      joystate_a[4], joystate_a[5],
      joystate_b[0], joystate_b[1], joystate_b[2], joystate_b[3],
      joystate_b[4], joystate_b[5] );
    SDL_WM_SetCaption( testytesttest, testytesttest );
*/
  }

  return swallow_event;
}

static void dojoysetup( struct machine *oric, Sint16 mode_a, Sint16 mode_b )
{
  Sint32 i;

  close_joysticks( oric );

  for( i=0; i<6; i++ )
  {
    joystate_a[i] = 0;
    joystate_b[i] = 0;
  }

  if( (!is_real_joystick( mode_a )) && (!is_real_joystick( mode_b )) )
    return;

  if( !joysubinited )
  {
    //dbg_printf( "Initialising joysubsystem" );
    if( SDL_InitSubSystem( SDL_INIT_JOYSTICK ) != 0 )
      return;

    //dbg_printf( "Success!" );
    joysubinited = SDL_TRUE;
  }

  if( is_real_joystick( mode_a ) )
  {
    oric->sdljoy_a = SDL_JoystickOpen( mode_a - JOYMODE_SDL0 );
    //dbg_printf( "Joy0 = %p", oric->sdljoy_a );
    SDL_JoystickEventState( SDL_TRUE );
  }

  if( is_real_joystick( mode_b ) )
  {
    if( mode_b == mode_a )
    {
      oric->sdljoy_b = oric->sdljoy_a;
    } else {
      oric->sdljoy_b = SDL_JoystickOpen( mode_b - JOYMODE_SDL0 );
      SDL_JoystickEventState( SDL_TRUE );
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
