
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <SDL/SDL.h>

#include "6502.h"
#include "via.h"
#include "8912.h"
#include "gui.h"
#include "disk.h"
#include "machine.h"
#include "monitor.h"

extern struct machine oric;
extern SDL_bool warpspeed, soundon;
Uint32 lastframetimes[8], frametimeave;

SDL_bool init( void )
{
  Sint32 i;

  for( i=0; i<8; i++ ) lastframetimes[i] = 0;
  frametimeave = 0;
  preinit_machine();
  preinit_gui();

  if( !init_gui() ) return SDL_FALSE;
  if( !init_machine( MACH_ATMOS ) ) return SDL_FALSE;
  mon_init( &oric );

  return SDL_TRUE;
}

void shut( void )
{
  shut_machine();
  shut_gui();
}

Uint32 nosoundtiming( Uint32 interval, void *dummy )
{
  if( ( oric.emu_mode == EM_RUNNING ) &&
      ( !soundon ) )
  {
    SDL_Event     event;
    SDL_UserEvent userevent;

    userevent.type  = SDL_USEREVENT;
    userevent.code  = 0;
    userevent.data1 = NULL;
    userevent.data2 = NULL;
    
    event.type = SDL_USEREVENT;
    event.user = userevent;
    
    SDL_PushEvent( &event );
  }

  return (oric.cyclesperraster == 64) ? 1000/50 : 1000/60;
}


int main( int argc, char *argv[] )
{
  void *thetimer;
  Sint32 i;

  if( init() )
  {
    SDL_bool done, needrender, framedone;

    thetimer = SDL_AddTimer( 1000/50, (SDL_NewTimerCallback)nosoundtiming, 0 );

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
        render();
        needrender = SDL_FALSE;
      }
      
      if( oric.emu_mode == EM_RUNNING )
      {
        if( !framedone )
        {
          SDL_bool breaky;
          Uint32 framestart;

          framestart = SDL_GetTicks();
          while( ( !framedone ) && ( !needrender ) )
          {
            while( oric.cpu.rastercycles > 0 )
            {
              breaky = m6502_inst( &oric.cpu, SDL_TRUE );
              via_clock( &oric.via, oric.cpu.icycles );
              ay_ticktock( &oric.ay, oric.cpu.icycles );

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
          frametimeave = 0;
          for( i=7; i>0; i-- )
          {
            lastframetimes[i] = lastframetimes[i-1];
            frametimeave += lastframetimes[i];
          }
          lastframetimes[0] = SDL_GetTicks() - framestart;
          frametimeave = (frametimeave+lastframetimes[0])/8;
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
            if( !SDL_WaitEvent( &event ) ) break;
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
  shut();

	return 0;
}
