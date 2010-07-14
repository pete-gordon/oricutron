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
**  SDL based message box
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#include "system.h"
#include "6502.h"
#include "via.h"
#include "8912.h"
#include "gui.h"
#include "disk.h"
#include "monitor.h"
#include "machine.h"
#include "msgbox.h"

struct msgboxbut
{
  int x, y;
  char *str;
};

extern SDL_Surface *screen;
extern struct textzone *tz[NUM_TZ];
static struct msgboxbut *btns;
static int cbtn;

static struct msgboxbut ynbuts[] = { {  2, 5, "  YES   " },
                                     { 50, 5, "   NO   " },
                                     { -1,-1, NULL } };

static struct msgboxbut ocbuts[] = { {  2, 5, "   OK   " },
                                     { 50, 5, " CANCEL " },
                                     { -1,-1, NULL } };

static struct msgboxbut obuts[]  = { { 26, 5, "   OK   " },
                                     { -1,-1, NULL } };

SDL_bool init_msgbox( struct machine *oric )
{
  if( !alloc_textzone( oric, TZ_MSGBOX, 80, 120, 60, 8, "Oricutron Request" ) ) return SDL_FALSE;

  return SDL_TRUE;
}

void shut_msgbox( struct machine *oric )
{
  free_textzone( oric, TZ_MSGBOX );
}

// Render the filerequester
static void msgbox_render( struct machine *oric )
{
  int i;

  for( i=0; btns[i].x!=-1; i++ )
  {
    if( i==cbtn )
    {
      tz[TZ_MSGBOX]->cfc = 1;
      tz[TZ_MSGBOX]->cbc = 2;
    } else {
      tz[TZ_MSGBOX]->cfc = 2;
      tz[TZ_MSGBOX]->cbc = 5;
    }

    tzstrpos( tz[TZ_MSGBOX], btns[i].x, btns[i].y, btns[i].str );
  }

  oric->render_begin( oric );

  oric->render_video( oric, SDL_TRUE );
  oric->render_textzone( oric, TZ_MSGBOX );

  oric->render_end( oric );
}

static int msgbox_checkover( int x, int y )
{
  int i;

  for( i=0; btns[i].x!=-1; i++ )
  {
    if( ( y == btns[i].y ) &&
        ( x >= btns[i].x ) &&
        ( x < (btns[i].x+strlen(btns[i].str)) ) )
      return i;
  }
  return -1;
}

SDL_bool msgbox( struct machine *oric, int type, char *msg )
{
  int i, j, k, mx, my, presson;
  int lines[] = { 2*60, 3*60, 5*60 };
  char *msg2;
  SDL_Event event;
  SDL_bool wasunicode;

  wasunicode = SDL_EnableUNICODE( SDL_TRUE );
  SDL_EnableKeyRepeat( SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL );

  // Clear any old message
  for( i=1; i<59; i++ )
  {
    for( j=0; j<3; j++ )
    {
      tz[TZ_MSGBOX]->fc[lines[j]+i] = 2;
      tz[TZ_MSGBOX]->bc[lines[j]+i] = 3;
      tz[TZ_MSGBOX]->tx[lines[j]+i] = 32;
    }
  }

  for( i=0; msg[i]; i++ ) if( (msg[i]=='\r')||(msg[i]=='\n') ) break;

  for( j=0, k=30-i/2; j<i; j++, k++ )
    tz[TZ_MSGBOX]->tx[lines[0]+k] = msg[j];
  
  while( (msg[i]=='\r')||(msg[i]=='\n') ) i++;
  if( msg[i] )
  {
    msg2 = &msg[i];
    i = strlen( msg2 );
    for( j=0, k=30-i/2; j<i; j++, k++ )
      tz[TZ_MSGBOX]->tx[lines[1]+k] = msg2[j];
  }

  switch( type )
  {
    case MSGBOX_YES_NO:
      btns = ynbuts;
      cbtn = 0;
      break;

    case MSGBOX_OK_CANCEL:
      btns = ocbuts;
      cbtn = 1;
      break;
    
    case MSGBOX_OK:
      btns = obuts;
      cbtn = 0;
      break;
  }

  msgbox_render( oric );

  presson = -1;
  for( ;; )
  {
    if( !SDL_WaitEvent( &event ) ) 
    {
      SDL_EnableUNICODE( wasunicode );
      SDL_EnableKeyRepeat( wasunicode ? SDL_DEFAULT_REPEAT_DELAY : 0, wasunicode ? SDL_DEFAULT_REPEAT_INTERVAL : 0 );
      return SDL_FALSE;
    }

    switch( event.type )
    {
      case SDL_ACTIVEEVENT:
        switch( event.active.type )
        {
          case SDL_APPINPUTFOCUS:
          case SDL_APPACTIVE:
            filereq_render( oric );
            break;
        }
        break;

      case SDL_MOUSEMOTION:
        if( presson != -1 ) break;
        mx = (event.motion.x - tz[TZ_MSGBOX]->x)/8;
        my = (event.motion.y - tz[TZ_MSGBOX]->y)/12;
        i = msgbox_checkover( mx, my );
        if( i == -1 ) break;
        if( i != cbtn )
        {
          cbtn = i;
          msgbox_render( oric );
        }
        break;
      
      case SDL_MOUSEBUTTONDOWN:
        if( event.button.button == SDL_BUTTON_LEFT )
        {
          mx = (event.motion.x - tz[TZ_MSGBOX]->x)/8;
          my = (event.motion.y - tz[TZ_MSGBOX]->y)/12;
          i = msgbox_checkover( mx, my );
          if( i == -1 ) break;
          cbtn = i;
          presson = i;
          msgbox_render( oric );
        }
        break;
      
      case SDL_MOUSEBUTTONUP:
        if( event.button.button == SDL_BUTTON_LEFT )
        {
          if( presson == -1 ) break;
          mx = (event.motion.x - tz[TZ_MSGBOX]->x)/8;
          my = (event.motion.y - tz[TZ_MSGBOX]->y)/12;
          i = msgbox_checkover( mx, my );
          if( i == presson )
          {
            switch( type )
            {
              case MSGBOX_YES_NO:
              case MSGBOX_OK_CANCEL:
                SDL_EnableUNICODE( wasunicode );
                SDL_EnableKeyRepeat( wasunicode ? SDL_DEFAULT_REPEAT_DELAY : 0, wasunicode ? SDL_DEFAULT_REPEAT_INTERVAL : 0 );
                return (presson==0);

              default:
                SDL_EnableUNICODE( wasunicode );
                SDL_EnableKeyRepeat( wasunicode ? SDL_DEFAULT_REPEAT_DELAY : 0, wasunicode ? SDL_DEFAULT_REPEAT_INTERVAL : 0 );
                return SDL_TRUE;
            }
            break;
          }

          presson = -1;
        }
        break;
      
      case SDL_KEYDOWN:
        switch( event.key.keysym.sym )
        {
          case SDLK_TAB:
          case SDLK_RIGHT:
            cbtn = cbtn+1;
            if( btns[cbtn].x == -1 ) cbtn = 0;
            msgbox_render( oric );
            break;
          
          case SDLK_LEFT:
            cbtn--;
            if( cbtn < 0 )
            {
              for( cbtn=0; btns[cbtn].x!=-1; cbtn++ ) ;
              cbtn--;
            }
            msgbox_render( oric );
            break;
          
          case SDLK_RETURN:
            switch( type )
            {
              case MSGBOX_YES_NO:
              case MSGBOX_OK_CANCEL:
                SDL_EnableUNICODE( wasunicode );
                SDL_EnableKeyRepeat( wasunicode ? SDL_DEFAULT_REPEAT_DELAY : 0, wasunicode ? SDL_DEFAULT_REPEAT_INTERVAL : 0 );
                return (cbtn==0);

              default:
                SDL_EnableUNICODE( wasunicode );
                SDL_EnableKeyRepeat( wasunicode ? SDL_DEFAULT_REPEAT_DELAY : 0, wasunicode ? SDL_DEFAULT_REPEAT_INTERVAL : 0 );
                return SDL_TRUE;
            }
            break;
          
          default:
            break;
        }
        break;
    }
  }

  SDL_EnableUNICODE( wasunicode );
  SDL_EnableKeyRepeat( wasunicode ? SDL_DEFAULT_REPEAT_DELAY : 0, wasunicode ? SDL_DEFAULT_REPEAT_INTERVAL : 0 );
  return SDL_FALSE;
}
