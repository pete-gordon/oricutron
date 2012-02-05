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
#include "msgbox.h"

extern char tapefile[], tapepath[];
extern SDL_bool refreshtape;
char csavename[4096];
extern char filetmp[];

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

void toggletapecap( struct machine *oric, struct osdmenuitem *mitem, int dummy )
{
  char ortheader[] = { 'O', 'R', 'T', 0 };  // Oric Raw Tape, Version 0

  /* If capturing, stop */
  if( oric->tapecap )
  {
    fclose( oric->tapecap );
    oric->tapecap = NULL;
    mitem->name = "Save tape output...";
    refreshtape = SDL_TRUE;
    return;
  }

  /* Otherwise, prompt for the file to capture */
  if( !filerequester( oric, "Capture tape output", tapepath, csavename, FR_TAPESAVEORT ) )
  {
    // Never mind
    return;
  }
  if( csavename[0] == 0 ) return;

  /* If it ends in ".tap", we need to change it to ".ort", because
     we're capturing real signals here folks! */
  if( (strlen(csavename)>3) && (strcasecmp(&csavename[strlen(csavename)-4], ".tap")==0) )
    csavename[strlen(csavename)-4] = 0;

  /* Add .ort extension, if necessary */
  if( (strlen(csavename)<4) || (strcasecmp(&csavename[strlen(csavename)-4], ".ort")!=0) )
    strncat(csavename, ".ort", 4096);
  csavename[4095] = 0;

  joinpath( tapepath, csavename );

  /* Open the file */
  oric->tapecap = fopen( filetmp, "wb" );
  if( !oric->tapecap )
  {
    /* Oh well */
    msgbox( oric, MSGBOX_OK, "Unable to create file" );
    return;
  }

  /* Write header */
  fwrite( ortheader, 4, 1, oric->tapecap );
  
  /* Counter reset */
  oric->tapecapcount = -1;
  oric->tapecaplastbit = (oric->via.orb&oric->via.ddrb)>>7;

  /* Update menu */
  mitem->name = "Stop tape recording";
  refreshtape = SDL_TRUE;
}

/* When we're loading a .tap file (or a non-raw section of a .ort file),
** we have to do 2 things... First, we have to elongate the leader so the ROM
** can see it, and secondly we have to add a delay after the header so that the
** tape isn't several bytes into the program when the rom is ready to start
** reading it.
**
** Only call it when you think you're at a tape leader since it doesn't do many
** sanity checks.
*/
void tape_setup_header( struct machine *oric )
{
  int i;

  oric->tapedupbytes = 0;
  oric->tapehdrend   = 0;

  if( oric->tapebuf[oric->tapeoffs] != 0x16 )
    return;

  /* Does it look like a header? */
  for( i=0; oric->tapebuf[oric->tapeoffs+i]==0x16; i++ )
  {
    if( (oric->tapeoffs+i) >= oric->tapelen )
      return;
  }
  if( ( i < 3 ) || ( oric->tapebuf[oric->tapeoffs+i] != 0x24 ) )
    return;
  if( (oric->tapeoffs+i+9) >= oric->tapelen )
    return;
  i+=9;
  while( oric->tapebuf[oric->tapeoffs+i] != 0 )
  {
    if( (oric->tapeoffs+i) >= oric->tapelen )
      return;
    i++;
  }
  i++;

  oric->tapedupbytes = 80;
  oric->tapehdrend = oric->tapeoffs+i;
}


void tape_orbchange( struct via *via )
{
  struct machine *oric = via->oric;
  unsigned char buffer[4], tapebit, bufwrite;
  unsigned int count;

  /* Capturing tape output? */
  if( !oric->tapecap ) return;
  
  tapebit = (via->orb & via->ddrb) >> 7;
  if( tapebit == oric->tapecaplastbit ) return;

  oric->tapecaplastbit = tapebit;

  /* Were we waiting for the first toggle? */
  if( oric->tapecapcount < 0 )
  {
    /* Well, we found it */
    oric->tapecapcount = 0;
    fputc( tapebit, oric->tapecap );
    return;
  }

  count = (oric->tapecapcount>>1);

  if( count < 0xfc )
  {
    buffer[0] = count;
    bufwrite = 1;
  }
  else if( count < 0x100 )
  {
    buffer[0] = 0xfc;
    buffer[1] = count;
    bufwrite = 2;
  }
  else if( count < 0x10000 )
  {
    buffer[0] = 0xfd;
    buffer[1] = (count>>8)&0xff;
    buffer[2] = count&0xff;
    bufwrite = 3;
  }
  else
  {
    buffer[0] = 0xfd;
    buffer[1] = 0xff;
    buffer[2] = 0xff;
    bufwrite = 3;
  }

  fwrite( buffer, bufwrite, 1, oric->tapecap );
  oric->tapecapcount = 0;
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
    if( ( !oric->rawtape ) || ( oric->tapeoffs < oric->nonrawend ) )
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
      {
        tape_setup_header(oric);
      }
    }
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

void tape_next_raw_count( struct machine *oric )
{
  unsigned int n;
  int nonrawend;

  if( oric->tapeoffs >= oric->tapelen )
  {
    if( oric->nonrawend != oric->tapelen )
      oric->tapehitend = 3;
    return;
  }

  n = oric->tapebuf[oric->tapeoffs++];
  if (n < 0xfc)
  {
    oric->tapecount = n<<1;
    return;
  }

  switch (n)
  {
    case 0xff: // non-raw section
      if( oric->tapeoffs >= (oric->tapelen-1) )
      {
        oric->tapeoffs = oric->tapelen;
        oric->tapehitend = 3;
        return;
      }

      nonrawend = ((oric->tapebuf[oric->tapeoffs]<<8)|oric->tapebuf[oric->tapeoffs+1]) + oric->tapeoffs+2;
      if( nonrawend > oric->tapelen )
      {
        oric->tapeoffs = oric->tapelen;
        oric->tapehitend = 3;
        return;
      }

      oric->nonrawend    = nonrawend;
      oric->tapeoffs    += 2;
      oric->tapebit      = 0;
      oric->tapecount    = 2;
      oric->tapeout      = 0;
      tape_setup_header( oric );
      return;

    case 0xfc:
      if( oric->tapeoffs >= oric->tapelen )
      {
        oric->tapehitend = 3;
        return;
      }

      oric->tapecount = oric->tapebuf[oric->tapeoffs++] << 1;
      return;
    
    case 0xfd:
      if( oric->tapeoffs >= (oric->tapelen-1) )
      {
        oric->tapeoffs = oric->tapelen;
        oric->tapehitend = 3;
        return;
      }
      
      oric->tapecount = (oric->tapebuf[oric->tapeoffs]<<9)|(oric->tapebuf[oric->tapeoffs+1]<<1);
      oric->tapeoffs += 3;
      return;
  }

  // Invalid data
  oric->tapeoffs = oric->tapelen;
  oric->tapehitend = 3;
}

// Rewind to the start of the tape
void tape_rewind( struct machine *oric )
{
  oric->nonrawend = 0;
  if( oric->rawtape )
  {
    oric->tapeout = oric->tapebuf[4];
    via_write_CB1( &oric->via, oric->tapeout );

    oric->tapeoffs = 5;
    tape_next_raw_count( oric );
  }
  else
  {
    oric->tapeoffs   = 0;
    oric->tapebit    = 0;
    oric->tapecount  = 2;
    oric->tapeout    = 0;
    oric->tapedupbytes = 0;

    if( oric->tapebuf )
      tape_setup_header( oric );
  }
  oric->tapehitend = 0;
  oric->tapedelay = 0;
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

  if( oric->tapelen <= 4 )   // Even worth loading it?
  {
    fclose( f );
    return SDL_FALSE;
  }

  // Allocate memory for the tape image and read it in
  oric->tapebuf = malloc( oric->tapelen );
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

  // RAW tape?
  if (memcmp(oric->tapebuf, "ORT\0", 4) == 0)
  {
    oric->rawtape = SDL_TRUE;
  }
  else
  {
    oric->rawtape = SDL_FALSE;
  }


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
  int i;

  if( strncmp( (char *)&oric->mem[oric->pch_fd_getname_addr], oric->lasttapefile, 16 ) == 0 )
    oric->mem[oric->pch_fd_getname_addr] = 0;

  // Try and load the tape image
  strcpy( tapefile, oric->lasttapefile );
  i = strlen(tapefile);

  odir = getcwd( NULL, 0 );
  chdir( tapepath );
  tape_load_tap( oric, tapefile );
  if( !oric->tapebuf )
  {
    // Try appending .tap
    strcpy( &tapefile[i], ".tap" );
    tape_load_tap( oric, tapefile );
  }
  if( !oric->tapebuf )
  {
    // Try appending .ort
    strcpy( &tapefile[i], ".ort" );
    tape_load_tap( oric, tapefile );
  }
  
  if( oric->tapebuf )
  {
    // We already inserted this one. Don't re-insert it when we get to the end. */
    oric->lasttapefile[0] = 0;
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
          // end of the current tape, or the filename ends in .TAP, .ORT or .WAV
          if( ( !oric->tapebuf ) ||
              ( oric->tapeoffs >= oric->tapelen ) ||
              ( ( i > 3 ) && ( strcasecmp( &oric->lasttapefile[i-4], ".tap" ) == 0 ) ) ||
              ( ( i > 3 ) && ( strcasecmp( &oric->lasttapefile[i-4], ".ort" ) == 0 ) ) /*||
              ( ( i > 3 ) && ( strcasecmp( &oric->lasttapefile[i-4], ".wav" ) == 0 ) )*/ )
          {
            tape_autoinsert( oric );
            oric->cpu.write( &oric->cpu, oric->pch_fd_getname_addr, 0 );
          }
        }
      }
    }

    if( ( oric->pch_csave_available ) && ( ( !oric->tapecap ) || ( oric->tapeturbo ) ) )
    {
      int docsave = 0;

      // Patch CSAVE to write to a file
      if( oric->cpu.calcpc == oric->pch_csave_pc )
      {
        oric->pch_csave_stack = oric->cpu.sp;
      }
      else if(( oric->cpu.calcpc == oric->pch_csave_getname_pc ) && ( oric->pch_csave_stack != 0 ))
      {
        int start, end, len, headerlen;

        // Get start address
        start = (oric->cpu.read( &oric->cpu, oric->pch_csave_header_addr+2 )<<8) | oric->cpu.read( &oric->cpu, oric->pch_csave_header_addr+1 );
        // Get end address
        end   = (oric->cpu.read( &oric->cpu, oric->pch_csave_header_addr+4 )<<8) | oric->cpu.read( &oric->cpu, oric->pch_csave_header_addr+3 );
        len = end-start;

        i=0;
        do
        {
          j = oric->cpu.read( &oric->cpu, oric->pch_csave_getname_addr+i );
          if( i >= 16 ) j=0;
          i++;
        }
        while( j!=0 );
        headerlen = 5+9+i;

        if( len > 0)
        {
          if( !oric->tapecap )
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
              if( filerequester( oric, "Save to tape", tapepath, csavename, FR_TAPESAVETAP ) )
                docsave = csavename[0]!=0;
            }
            else
            {
              if ((strlen(csavename)<4) || (strcasecmp(&csavename[strlen(csavename)-4], ".tap")!=0))
                strcat(csavename, ".tap");
              docsave = 1;
            }
          }
          else
          {
            docsave = 1;
          }

          // Do the CSAVE operation?
          if( docsave )
          {
            FILE *f = NULL;
            char *odir = NULL;
            unsigned char tmp[] = { 0x16, 0x16, 0x16, 0x16, 0x24 };

            if( !oric->tapecap )
            {
              odir = getcwd( NULL, 0 );
              chdir( tapepath );

              f = fopen(csavename, "wb");
            }
            else
            {
              f = oric->tapecap;

              // Straight into a CSAVE at the start?
              if( oric->tapecapcount < 0 )
              {
                fputc( 0, f );
              }

              fputc( 0xff, f );
              fputc( ((len+headerlen+1)>>8)&0xff, f );
              fputc( (len+headerlen+1)&0xff, f );
            }

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

              i = start;
              while( i <= end )
              {
                fputc( oric->cpu.read(&oric->cpu, i), f );
                i++;
              }

              if( f != oric->tapecap ) fclose( f );
            }

            oric->cpu.calcpc = oric->pch_csave_end_pc;
            oric->cpu.calcop = oric->cpu.read(&oric->cpu, oric->cpu.calcpc);
            oric->cpu.sp = oric->pch_csave_stack;

            if( !oric->tapecap )
            {
              chdir( odir );
              free( odir );
              snprintf( filetmp, 32, "\x0f\x10 Saved to %s", csavename );
              filetmp[31] = 0;
              if (strlen(csavename) > 20)
              {
                filetmp[30] = '\x16';
              }
              do_popup( oric, filetmp );
            }
          }
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

  // Don't do turbotape if we have a raw tape
  // Turbotape can't work with rawtape. TODO: Auto enable warpspeed when CLOADing/CSAVEing rawtape
  if( oric->rawtape ) return;

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

  // Update the counter since last PB7 toggle
  if( ( oric->tapecap ) && ( oric->tapecapcount != -1 ) )
    oric->tapecapcount += cycles;

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
    if( oric->tapehitend > 2 )
    {
      // Try to autoinsert a tape image
      if( ( oric->lasttapefile[0] ) && ( oric->autoinsert ) )
      {
        tape_autoinsert( oric );
        oric->lasttapefile[0] = 0;
      }
    }
  }

  // Abort if turbotape is on
  if( ( oric->pch_tt_available ) && ( oric->tapeturbo ) && ( !oric->tapeturbo_forceoff ) && ( oric->romon ) && ( !oric->rawtape ) )
    return;

  if( oric->tapehitend > 2 )
    return;

  if( ( oric->tapehdrend != 0 ) && ( oric->tapeoffs == oric->tapehdrend ) )
  {
    oric->tapedelay = 1285;
    oric->tapehdrend = 0;
  }

  // No turbotape. Do "real" tape emulation.
  // Count down the cycle counter
  if( oric->tapecount > cycles )
  {
    oric->tapecount -= cycles;
    if( oric->tapedelay > cycles )
      oric->tapedelay -= cycles;
    else
      oric->tapedelay = 0;
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

  if( oric->tapedelay > cycles )
  {
    oric->tapedelay -= cycles;
    oric->tapecount = TAPE_1_PULSE;
    dbg_printf("toggle");
    return;
  }
  oric->tapedelay = 0;

  if( oric->tapeoffs >= oric->tapelen )
  {
    switch( oric->tapehitend )
    {
      case 0: oric->tapecount = TAPE_1_PULSE; break;
      case 1: oric->tapecount = 0x36*2; break;
    }
    oric->tapehitend++;
    refreshtape = SDL_TRUE;
    return;
  }

  // Raw tape
  if( oric->rawtape )
  {
    // In a non-raw section?
    if( oric->tapeoffs >= oric->nonrawend )
    {
      tape_next_raw_count( oric );
      if( oric->tapehitend > 2 )
        refreshtape = SDL_TRUE;
      return;
    }
  }

  // .TAP real tape simulation
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

      case 11:
      case 12:
      case 13:
        oric->tapetime = TAPE_1_PULSE;
        break;
    }

    // Move on to the next bit
    oric->tapebit = (oric->tapebit+1)%14;
    if( !oric->tapebit )
    {
      if( oric->tapedupbytes > 0 )
        oric->tapedupbytes--;
      else
        oric->tapeoffs++;
    }
  }
  oric->tapecount = oric->tapetime;
}

