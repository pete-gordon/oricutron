/*
**  Oriculator
**  Copyright (C) 2009 Peter Gordon
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
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef __SPECIFY_SDL_DIR__
#include <SDL/SDL.h>
#else
#include <SDL.h>
#endif

#ifdef __amigaos4__
#include <proto/exec.h>
#include <proto/dos.h>
#endif

#include "6502.h"
#include "via.h"
#include "8912.h"
#include "gui.h"
#include "disk.h"
#include "machine.h"
#include "monitor.h"

extern SDL_bool warpspeed, soundon;
Uint32 lastframetimes[8], frametimeave;
extern char mon_bpmsg[];

#ifdef __amigaos4__
int32 timersigbit = -1;
uint32 timersig;
struct Task *maintask;
#endif

SDL_bool init( struct machine *oric )
{
  Sint32 i;

  for( i=0; i<8; i++ ) lastframetimes[i] = 0;
  frametimeave = 0;
  preinit_machine( oric );
  preinit_gui();

#ifdef __amigaos4__
  timersigbit = IExec->AllocSignal( -1 );
  if( timersigbit == -1 ) return SDL_FALSE;
  timersig = 1L<<timersigbit;
  
  maintask = IExec->FindTask( NULL );
#endif

  if( !init_gui( oric ) ) return SDL_FALSE;
  if( !init_machine( oric, MACH_ATMOS, SDL_TRUE ) ) return SDL_FALSE;

  mon_init( oric );

  return SDL_TRUE;
}

void shut( struct machine *oric )
{
  shut_machine( oric );
  mon_shut();
  shut_gui();
#ifdef __amigaos4__
  IExec->FreeSignal( timersigbit );
#endif
}

Uint32 nosoundtiming( Uint32 interval, void *userdata )
{
  struct machine *oric = (struct machine *)userdata;
  if( ( oric->emu_mode == EM_RUNNING ) &&
      ( !soundon ) )
  {
#if __amigaos4__
    IExec->Signal( maintask, timersig );
#else
    SDL_Event     event;
    SDL_UserEvent userevent;

    userevent.type  = SDL_USEREVENT;
    userevent.code  = 0;
    userevent.data1 = NULL;
    userevent.data2 = NULL;
    
    event.type = SDL_USEREVENT;
    event.user = userevent;
    
    SDL_PushEvent( &event );
#endif
  }

  return (oric->cyclesperraster == 64) ? 1000/50 : 1000/60;
}

int main( int argc, char *argv[] )
{
  void *thetimer;
  Sint32 i, foo;
  struct machine oric;
  Uint32 framestart;

  if( init( &oric ) )
  {
    SDL_bool done, needrender, framedone;

    thetimer = SDL_AddTimer( 1000/50, (SDL_NewTimerCallback)nosoundtiming, (void *)&oric );
    foo = 0;
    framestart = 0;

    done = SDL_FALSE;
    needrender = SDL_TRUE;
    framedone = SDL_FALSE;
    while( !done )
    {
      SDL_Event event;

      if( oric.emu_mode == EM_PLEASEQUIT )
        break;

      if( needrender )
      {
        render( &oric );
        needrender = SDL_FALSE;
      }
      
      if( oric.emu_mode == EM_RUNNING )
      {
        if( !framedone )
        {
          SDL_bool breaky;

          if( framestart != 0 )
          {
            frametimeave = 0;
            for( i=7; i>0; i-- )
            {
              lastframetimes[i] = lastframetimes[i-1];
              frametimeave += lastframetimes[i];
            }
            lastframetimes[0] = SDL_GetTicks() - framestart;
            frametimeave = (frametimeave+lastframetimes[0])/8;
          }

          framestart = SDL_GetTicks();
          while( ( !framedone ) && ( !needrender ) )
          {
            while( oric.cpu.rastercycles > 0 )
            {
              breaky = m6502_inst( &oric.cpu, SDL_TRUE, mon_bpmsg );
              via_clock( &oric.via, oric.cpu.icycles );
              ay_ticktock( &oric.ay, oric.cpu.icycles );
              if( oric.drivetype ) wd17xx_ticktock( &oric.wddisk, oric.cpu.icycles );

              if( oric.emu_mode != EM_RUNNING )
              {
                needrender = SDL_TRUE;
                break;
              }

              if( breaky )
              {
                setemumode( &oric, NULL, EM_DEBUG );
                needrender = SDL_TRUE;
                break;
              }
            }

            if( oric.cpu.rastercycles <= 0 )
            {
              framedone = video_doraster( &oric );
              oric.cpu.rastercycles += oric.cyclesperraster;
            }
          }
        }

        if( warpspeed )
        {
          if( framedone )
          {
            needrender = SDL_TRUE;
            framedone  = SDL_FALSE;
          }
          if( !SDL_PollEvent( &event ) ) continue;
        } else {
          if( needrender )
          {
            if( !SDL_PollEvent( &event ) ) continue;
          } else {
#ifdef __amigaos4__
            uint32 gotsigs;

            gotsigs = IExec->Wait( timersig | SIGBREAKF_CTRL_C );
            if( gotsigs & SIGBREAKF_CTRL_C ) done = TRUE;
            if( gotsigs & timersig )
            {
              needrender = SDL_TRUE;
              framedone  = SDL_FALSE;
              if( !SDL_PollEvent( &event ) ) continue;
            }
#else
            if( !SDL_WaitEvent( &event ) ) break;
#endif
          }
        }
      } else {
        if( !SDL_WaitEvent( &event ) ) break;
      }

      switch( event.type )
      {
        case SDL_USEREVENT:
          needrender = SDL_TRUE;
          framedone  = SDL_FALSE;
          break;

        case SDL_QUIT:
          done = SDL_TRUE;
          break;
        
        default:
          switch( oric.emu_mode )
          {
            case EM_MENU:
              done |= menu_event( &event, &oric, &needrender );
              break;
 
            case EM_RUNNING:
              done |= emu_event( &event, &oric, &needrender );
              break;

            case EM_DEBUG:
              done |= mon_event( &event, &oric, &needrender );
              break;
          }
      }
    }
  }
  shut( &oric );

  return 0;
}
