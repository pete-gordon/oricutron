/*
**  Oricutron
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

#ifdef __amigaos4__
#include <proto/exec.h>
#include <proto/dos.h>
#endif

#include "system.h"
#include "6502.h"
#include "via.h"
#include "8912.h"
#include "gui.h"
#include "disk.h"
#include "machine.h"
#include "monitor.h"
#include "avi.h"
#include "filereq.h"

#define FRAMES_TO_AVERAGE 15

SDL_bool fullscreen;
extern SDL_bool warpspeed, soundon;
Uint32 lastframetimes[FRAMES_TO_AVERAGE], frametimeave;
extern char mon_bpmsg[];

#ifdef __amigaos4__
int32 timersigbit = -1;
uint32 timersig;
struct Task *maintask;
#endif

SDL_bool init( struct machine *oric, int argc, char *argv[] )
{
  Sint32 i;
  Sint32 start_machine, start_disktype, start_mode;
  char *start_disk, *start_tape, *start_syms;
  char opt_type, *opt_arg, *tmp;

//  struct avi_handle *ah;

//  ah = avi_open( "foo.avi" );
//  if( ah ) avi_close( ah );

  // Defaults
  start_machine  = MACH_ATMOS;
  start_disktype = DRV_NONE;
  start_mode     = EM_RUNNING;
  start_disk     = NULL;
  start_tape     = NULL;
  start_syms     = NULL;
  fullscreen     = SDL_FALSE;

  for( i=1; i<argc; i++ )
  {
    opt_type = 0;
    if( argv[i][0] == '-' )
    {
      switch( argv[i][1] )
      {
        case '-':  // Long argument types
          opt_arg = NULL;
          tmp = &argv[i][2];

          if( strcasecmp( tmp, "fullscreen" ) == 0 ) { opt_type = 'f'; break; }
          if( strcasecmp( tmp, "window"     ) == 0 ) { opt_type = 'w'; break; }
          if( strcasecmp( tmp, "debug"      ) == 0 ) { opt_type = 'b'; break; }

          if( i<(argc-1) )
            opt_arg = argv[i+1];
          i++;

          if( strcasecmp( tmp, "machine"    ) == 0 ) { opt_type = 'm'; break; }
          if( strcasecmp( tmp, "disk"       ) == 0 ) { opt_type = 'd'; break; }
          if( strcasecmp( tmp, "tape"       ) == 0 ) { opt_type = 't'; break; }
          if( strcasecmp( tmp, "drive"      ) == 0 ) { opt_type = 'k'; break; }
          if( strcasecmp( tmp, "symbols"    ) == 0 ) { opt_type = 's'; break; }
          break;
        
        default:
          opt_type = argv[i][1];
          opt_arg = &argv[i][2];
          break;
      }

      switch( opt_type )
      {
        case 'm':  // Machine type
          if( opt_arg )
          {
            if( strcasecmp( opt_arg, "atmos" ) == 0 ) { start_machine = MACH_ATMOS;     break; }
            if( strcasecmp( opt_arg, "a"     ) == 0 ) { start_machine = MACH_ATMOS;     break; }
            if( strcasecmp( opt_arg, "oric1" ) == 0 ) { start_machine = MACH_ORIC1;     break; }
            if( strcasecmp( opt_arg, "1"     ) == 0 ) { start_machine = MACH_ORIC1;     break; }
            if( strcasecmp( opt_arg, "o16k"  ) == 0 ) { start_machine = MACH_ORIC1_16K; break; }
          }
          
          printf( "Invalid machine type\n" );
          break;
        
        case 'd':  // Disk image
          start_disk = opt_arg;
          break;
        
        case 't':  // Tape image
          start_tape = opt_arg;
          break;
   
        case 'k':  // Drive controller type
          if( opt_arg )
          {
            if( strcasecmp( opt_arg, "microdisc" ) == 0 ) { start_disktype = DRV_MICRODISC; break; }
            if( strcasecmp( opt_arg, "m"         ) == 0 ) { start_disktype = DRV_MICRODISC; break; }
            if( strcasecmp( opt_arg, "jasmin"    ) == 0 ) { start_disktype = DRV_JASMIN;    break; }
            if( strcasecmp( opt_arg, "j"         ) == 0 ) { start_disktype = DRV_JASMIN;    break; }
          }

          printf( "Invalid drive type\n" );
          break;
        
        case 's':  // Pre-load symbols file
          start_syms = opt_arg;
          break;
        
        case 'f':
          fullscreen = SDL_TRUE;
          break;
        
        case 'w':
          fullscreen = SDL_FALSE;
          break;
        
        case 'b':
          start_mode = EM_DEBUG;
          break; 
      }        
    }
  }

  if( ( start_disk ) && ( start_disktype == DRV_NONE ) )
    start_disktype = DRV_MICRODISC;

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
  if( !init_filerequester() ) return SDL_FALSE;
  oric->drivetype = start_disktype;
  if( !init_machine( oric, start_machine, SDL_TRUE ) ) return SDL_FALSE;

  if( start_mode != EM_RUNNING )
    setemumode( oric, NULL, start_mode );

  if( start_disk ) diskimage_load( oric, start_disk, 0 );
  if( start_tape )
  {
    if( tape_load_tap( oric, start_tape ) )
      queuekeys( "CLOAD\"\"\x0d" );
  }

  mon_init( oric );

  if( start_syms )
    mon_new_symbols( start_syms, SDL_TRUE );

  return SDL_TRUE;
}

void shut( struct machine *oric )
{
  shut_machine( oric );
  mon_shut();
  shut_filerequester();
  shut_gui();
#ifdef __amigaos4__
  IExec->FreeSignal( timersigbit );
#endif
}

// Stupid 10ms granularity bastard.
static Uint32 time50[] = { 20, 20, 20 };
static Uint32 time60[] = { 20, 20, 10 };
static int tmoffs = 0;

Uint32 systemtiming( Uint32 interval, void *userdata )
{
  struct machine *oric = (struct machine *)userdata;
  if( oric->emu_mode == EM_RUNNING )
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

  tmoffs = (tmoffs+1)%3;
  return oric->vid_freq ? time50[tmoffs] : time60[tmoffs];
}

int main( int argc, char *argv[] )
{
  void *thetimer;
  Sint32 i, foo;
  struct machine oric;
  Uint32 framestart;

  if( init( &oric, argc, argv ) )
  {
    SDL_bool done, needrender, framedone;

    thetimer = SDL_AddTimer( 1000/50, (SDL_NewTimerCallback)systemtiming, (void *)&oric );
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
            for( i=(FRAMES_TO_AVERAGE-1); i>0; i-- )
            {
              lastframetimes[i] = lastframetimes[i-1];
              frametimeave += lastframetimes[i];
            }
            lastframetimes[0] = SDL_GetTicks() - framestart;
            frametimeave = (frametimeave+lastframetimes[0])/FRAMES_TO_AVERAGE;
          }

          framestart = SDL_GetTicks();

          while( ( !framedone ) && ( !needrender ) )
          {
            while( oric.cpu.rastercycles > 0 )
            {
              m6502_set_icycles( &oric.cpu );
              via_clock( &oric.via, oric.cpu.icycles );
              ay_ticktock( &oric.ay, oric.cpu.icycles );
              if( oric.drivetype ) wd17xx_ticktock( &oric.wddisk, oric.cpu.icycles );
              breaky = m6502_inst( &oric.cpu, SDL_TRUE, mon_bpmsg );
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
