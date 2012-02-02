/*
**  Oricutron
**  Copyright (C) 2009-2011 Peter Gordon
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
#include "filereq.h"
#include "tape.h"

extern char tapefile[], tapepath[];
extern SDL_bool refreshtape;
char csavename[4096];

// Pop-up the name of the currently inserted tape
// image file (or an eject symbol if no tape image).
void tape_popup( struct machine *oric )
{
  char tmp[40];

  if( !oric->tapename[0] )
  {
    // Eject symbol
    do_popup( oric, "\x0f\x10\x13" );
    return;
  }

  // Tape image name
  sprintf( tmp, "\x0f\x10""%c %-16s", oric->tapemotor ? 18 : 17, oric->tapename );
  do_popup( oric, tmp );
}

// Set the tape motor on or off
void tape_setmotor( struct machine *oric, SDL_bool motoron )
{
  if( motoron == oric->tapemotor )
    return;

  // Refresh the tape status icon in the status bar
  refreshtape = SDL_TRUE;

  // "Real" tape emulation?
  if( ( !oric->tapeturbo ) || ( !oric->pch_tt_available ) )
  {
    // If we're stopping part way through a byte, just move
    // the current position on to the start of the next byte
    if( !motoron )
    {
      if( oric->tapebit > 0 )
        oric->tapeoffs++;
      oric->tapebit = 0;
    }

    // If we're starting the tape motor and the tape
    // is at a sync header, generate a bunch of dummy
    // sync bytes. This makes the Oric Atmos welcome tape
    // work without turbo tape enabled, for example.
    if( ( motoron ) &&
        ( oric->tapebuf ) &&
        ( oric->tapeoffs < oric->tapelen ) &&
        ( oric->tapebuf[oric->tapeoffs] == 0x16 ) )
      oric->tapedupbytes = 80;
  }

  // Set the new status and do a popup
  oric->tapemotor = motoron;
  if( oric->tapename[0] ) tape_popup( oric );
}

// Free up the current tape image
void tape_eject( struct machine *oric )
{
  if( oric->tapebuf ) free( oric->tapebuf );
  oric->tapebuf = NULL;
  oric->tapelen = 0;
  oric->tapename[0] = 0;
  tape_popup( oric );
  refreshtape = SDL_TRUE;
}

// Rewind to the start of the tape
void tape_rewind( struct machine *oric )
{
  oric->tapeoffs   = 0;
  oric->tapebit    = 0;
  oric->tapecount  = 2;
  oric->tapeout    = 0;
  oric->tapedupbytes = 80;
  refreshtape = SDL_TRUE;
}

// Insert a new tape image
SDL_bool tape_load_tap( struct machine *oric, char *fname )
{
  FILE *f;

  // First make sure the image file exists
  f = fopen( fname, "rb" );
  if( !f ) return SDL_FALSE;

  // Eject any old image
  tape_eject( oric );

  // Get the image size
  fseek( f, 0, SEEK_END );
  oric->tapelen = ftell( f );
  fseek( f, 0, SEEK_SET );

  if( oric->tapelen <= 0 )
  {
    fclose( f );
    return SDL_FALSE;
  }

  // Allocate memory for the tape image and read it in
  oric->tapebuf = malloc( oric->tapelen+1 );
  if( !oric->tapebuf )
  {
    fclose( f );
    oric->tapelen = 0;
    return SDL_FALSE;
  }

  if( fread( &oric->tapebuf[0], oric->tapelen, 1, f ) != 1 )
  {
    fclose( f );
    tape_eject( oric );
    return SDL_FALSE;
  }

  // Add an extra zero byte to the end. I don't know why this
  // is necessary, but it makes some tapes work that otherwise
  // get stuck at the end.
  oric->tapebuf[oric->tapelen++] = 0;

  fclose( f );
  
  // Rewind the tape
  tape_rewind( oric );

  // Make a displayable version of the image filename
  if( strlen( fname ) > 31 )
  {
    strncpy( oric->tapename, &fname[strlen(fname)-31], 32 );
    oric->tapename[0] = 22;
  } else {
    strncpy( oric->tapename, fname, 32 );
  }
  oric->tapename[31] = 0;

  // Show it in the popup
  tape_popup( oric );
  return SDL_TRUE;
};

void tape_autoinsert( struct machine *oric )
{
  char *odir;

  if( strncmp( (char *)&oric->mem[oric->pch_fd_getname_addr], oric->lasttapefile, 16 ) == 0 )
    oric->mem[oric->pch_fd_getname_addr] = 0;

  // Try and load the tape image
  strcpy( tapefile, oric->lasttapefile );

  odir = getcwd( NULL, 0 );
  chdir( tapepath );
  tape_load_tap( oric, tapefile );
  if( !oric->tapebuf )
  {
    // Try appending .tap
    strcat( tapefile, ".tap" );
    tape_load_tap( oric, tapefile );
  }
  chdir( odir );
  free( odir );
}

// Do the tape patches (must be done after every m6502 setcycles)
void tape_patches( struct machine *oric )
{
  Sint32 i, j;

  if( oric->romon )
  {
    if( oric->pch_fd_available )
    {
      // Patch CLOAD to insert a tape image specified.
      // Unfortunately the way the ROM works means we
      // only have up to 16 chars.
      if( oric->cpu.calcpc == oric->pch_fd_getname_pc )
      {
        // Read in the filename from RAM
        for( i=0; i<16; i++ )
        {
          j = oric->cpu.read( &oric->cpu, oric->pch_fd_getname_addr+i );
          if( !j ) break;
          oric->lasttapefile[i] = j;
        }
        oric->lasttapefile[i] = 0;

        if( ( oric->cpu.read( &oric->cpu, oric->pch_fd_getname_addr ) != 0 ) &&
            ( oric->autoinsert ) )
        {
          // Only do this if there is no tape inserted, or we're at the
          // end of the current tape, or the filename ends in .TAP
          if( ( !oric->tapebuf ) ||
              ( oric->tapeoffs >= oric->tapelen ) ||
              ( ( i > 3 ) && ( strcasecmp( &oric->lasttapefile[i-4], ".tap" ) == 0 ) ) )
            tape_autoinsert( oric );
        }
      }
    }

    if( oric->pch_csave_available )
    {
      int docsave = 0;

      // Patch CSAVE to write to a file
      if( oric->cpu.calcpc == oric->pch_csave_pc )
      {
        oric->pch_csave_stack = oric->cpu.sp;
      }
      else if(( oric->cpu.calcpc == oric->pch_csave_getname_pc ) && ( oric->pch_csave_stack != 0 ))
      {
        // Read the filename from RAM
        for( i=0; i<16; i++ )
        {
          j = oric->cpu.read( &oric->cpu, oric->pch_csave_getname_addr+i );
          if( !j ) break;
          csavename[i] = j;
        }
        csavename[i] = 0;

        // No filename?
        if( csavename[0] == 0)
        {
          if( filerequester( oric, "Save to tape", tapepath, csavename, FR_TAPESAVE ) )
            docsave = 1;
        }
        else
        {
          if ((strlen(csavename)<4) || (strcasecmp(&csavename[strlen(csavename)-4], ".tap")!=0))
            strcat(csavename, ".tap");
          docsave = 1;
        }

        // Do the CSAVE operation?
        if(( docsave ) && ( csavename[0] ))
        {
          FILE *f = NULL;
          char *odir;
          unsigned char tmp[] = { 0x16, 0x16, 0x16, 0x16, 0x24 };

          odir = getcwd( NULL, 0 );
          chdir( tapepath );

          f = fopen(csavename, "wb");
          if( f )
          {
            fwrite( tmp, 5, 1, f ); // Output leader
            for( i=8; i>=0; i-- ) fputc( oric->cpu.read( &oric->cpu, oric->pch_csave_header_addr+i ), f );
            i=0;
            do
            {
              j = oric->cpu.read( &oric->cpu, oric->pch_csave_getname_addr+i );
              if( i >= 16 ) j=0;
              fputc( j, f );
              i++;
            }
            while( j!=0 );

            // Get start address
            i = (oric->cpu.read( &oric->cpu, oric->pch_csave_header_addr+2 )<<8) | oric->cpu.read( &oric->cpu, oric->pch_csave_header_addr+1 );
            // Get end address
            j = (oric->cpu.read( &oric->cpu, oric->pch_csave_header_addr+4 )<<8) | oric->cpu.read( &oric->cpu, oric->pch_csave_header_addr+3 );

            while( i < j )
            {
              fputc( oric->cpu.read(&oric->cpu, i), f );
              i++;
            }

            fclose( f );
          }

          chdir( odir );
          free( odir );

          oric->cpu.calcpc = oric->pch_csave_end_pc;
	        oric->cpu.calcop = oric->cpu.read(&oric->cpu, oric->cpu.calcpc);
          oric->cpu.sp = oric->pch_csave_stack;
        }
      }
    }
  }

// Only do turbotape past this point!

  // No tape? Motor off?
  if( ( !oric->tapebuf ) || ( !oric->tapemotor ) )
  {
    if( ( oric->pch_tt_available ) && ( oric->tapeturbo ) && ( oric->romon ) && ( oric->cpu.calcpc == oric->pch_tt_getsync_pc ) )
      oric->tapeturbo_syncstack = oric->cpu.sp;
    return;
  }

  if( ( oric->tapeturbo_forceoff ) && ( oric->pch_tt_available ) && ( oric->romon ) && ( oric->cpu.calcpc == oric->pch_tt_getsync_end_pc ) )
    oric->tapeturbo_forceoff = SDL_FALSE;

  // Maybe do turbotape
  if( ( oric->pch_tt_available ) && ( oric->tapeturbo ) && ( !oric->tapeturbo_forceoff ) && ( oric->romon ) )
  {
    SDL_bool dosyncpatch;

    dosyncpatch = SDL_FALSE;

    if( oric->cpu.calcpc == oric->pch_tt_getsync_loop_pc )
    {
      // Cassette sync was called when there was no valid
      // tape to sync, so the normal cassette sync hack failed
      // and now we're stuck looking for a signal that will never
      // arrive. We have to recover back to the end of the cassette
      // sync routine.

      // Did we spot the entry into the cassette sync routine?
      if( oric->tapeturbo_syncstack == -1 )
      {
        // No. Give up.
        oric->tapeturbo_forceoff = SDL_TRUE;
        return;
      }

      // Restore the stack
      oric->cpu.sp = oric->tapeturbo_syncstack;
      oric->tapeturbo_syncstack = -1;
      dosyncpatch = SDL_TRUE;
    }

    if( ( oric->cpu.calcpc == oric->pch_tt_getsync_pc ) || ( dosyncpatch ) )
    {
      // Currently at a sync byte?
      if( oric->tapebuf[oric->tapeoffs] != 0x16 )
      {
        // Find the next sync byte
        do
        {
          oric->tapeoffs++;

          // Give up at end of image
          if( oric->tapeoffs >= oric->tapelen )
          {
            refreshtape = SDL_TRUE;
            return;
          }
        } while( oric->tapebuf[oric->tapeoffs] != 0x16 );
      }

      // "Jump" to the end of the cassette sync routine
      oric->cpu.calcpc = oric->pch_tt_getsync_end_pc;
      oric->cpu.calcop = oric->cpu.read(&oric->cpu, oric->cpu.calcpc);
      return;
    }

    if( oric->cpu.calcpc == oric->pch_tt_readbyte_pc )
    {
      // Read the next byte directly into A
      oric->cpu.a = oric->tapebuf[oric->tapeoffs++];

      // Set flags
      oric->cpu.f_z = oric->cpu.a == 0;
      oric->cpu.f_c = oric->pch_tt_readbyte_setcarry ? 1 : 0;

      // Simulate the effects of the read byte routine
      if( oric->pch_tt_readbyte_storebyte_addr != -1 ) oric->cpu.write( &oric->cpu, oric->pch_tt_readbyte_storebyte_addr, oric->cpu.a );
      if( oric->pch_tt_readbyte_storezero_addr != -1 ) oric->cpu.write( &oric->cpu, oric->pch_tt_readbyte_storezero_addr, 0x00 );

      // Jump to the end of the read byte routine
      oric->cpu.calcpc = oric->pch_tt_readbyte_end_pc;
      oric->cpu.calcop = oric->cpu.read( &oric->cpu, oric->cpu.calcpc );
      if( oric->tapeoffs >= oric->tapelen ) refreshtape = SDL_TRUE;
    }
  }
}

// Emulate the specified cpu-cycles time for the tape
void tape_ticktock( struct machine *oric, int cycles )
{
  Sint32 j;

  // The VSync hack is triggered in the video emulation
  // but actually handled here, since the VSync signal
  // is read into the tape input. The video emulation
  // puts half a scanlines worth of cycles into this
  // counter, which we count down here.
  if( oric->vsync > 0 )
  {
    oric->vsync -= cycles;
    if( oric->vsync < 0 )
      oric->vsync = 0;
  }
  
  // If the VSync hack is active, we pull CB1 low
  // while the above counter is active.
  if( oric->vsynchack )
  {
    j = (oric->vsync == 0);
    if( oric->via.cb1 != j )
      via_write_CB1( &oric->via, j );
  }

  // No tape? Motor off?
  if( ( !oric->tapebuf ) || ( !oric->tapemotor ) )
    return;

  // Tape offset outside the tape image limits?
  if( ( oric->tapeoffs < 0 ) || ( oric->tapeoffs >= oric->tapelen ) )
  {
    // Try to autoinsert a tape image
    if( ( oric->lasttapefile[0] ) && ( oric->autoinsert ) )
    {
      tape_autoinsert( oric );
      oric->lasttapefile[0] = 0;
    }
    return;
  }

  // Abort if turbotape is on
  if( ( oric->pch_tt_available ) && ( oric->tapeturbo ) && ( !oric->tapeturbo_forceoff ) && ( oric->romon ) )
    return;

  // No turbotape. Do "real" tape emulation.
  // Count down the cycle counter
  if( oric->tapecount > cycles )
  {
    oric->tapecount -= cycles;
    return;
  }

  // Toggle the tape input
  oric->tapeout ^= 1;
  if( !oric->vsynchack )
  {
    // Update the audio if tape noise is enabled
    if( oric->tapenoise )
    {
      ay_lockaudio( &oric->ay ); // Gets unlocked at the end of each frame
      if( oric->ay.do_logcycle_reset )
      {
        oric->ay.logcycle = oric->ay.newlogcycle;
        oric->ay.do_logcycle_reset = SDL_FALSE;
      }
      if( oric->ay.tlogged < TAPELOG_SIZE )
      {
        oric->ay.tapelog[oric->ay.tlogged  ].cycle = oric->ay.logcycle;
        oric->ay.tapelog[oric->ay.tlogged++].val   = oric->tapeout;
      }
    }

    // Put tape signal onto CB1
    via_write_CB1( &oric->via, oric->tapeout );
  }

  // Tape signal rising edge
  if( oric->tapeout )
  {
    switch( oric->tapebit )
    {
      case 0:      // Start of a new byte. Send a 1 pulse
        oric->tapetime = TAPE_1_PULSE;
        break;

      case 1:     // Then a sync pulse (0)
        oric->tapetime = TAPE_0_PULSE;
        oric->tapeparity = 1;
        break;
      
      default:    // For bit numbers 2 to 9, send actual byte bits 0 to 7
        if( oric->tapebuf[oric->tapeoffs]&(1<<(oric->tapebit-2)) )
        {
          oric->tapetime = TAPE_1_PULSE;
          oric->tapeparity ^= 1;
        } else {
          oric->tapetime = TAPE_0_PULSE;
        }
        break;

      case 10:  // Then a parity bit
        if( oric->tapeparity )
          oric->tapetime = TAPE_1_PULSE;
        else
          oric->tapetime = TAPE_0_PULSE;
        break;

      case 11:  // and then 5 1 pulses.
      case 12:
      case 13:
      case 14:
      case 15:
        oric->tapetime = TAPE_1_PULSE;
        break;
    }

    // Move on to the next bit
    oric->tapebit = (oric->tapebit+1)%16;
    if( !oric->tapebit )
    {
      if( oric->tapedupbytes > 0 )
        oric->tapedupbytes--;
      else
        oric->tapeoffs++;
      if( oric->tapeoffs >= oric->tapelen )
        refreshtape = SDL_TRUE;
    }
  }
  oric->tapecount = oric->tapetime;
}
