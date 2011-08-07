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

/* VIA 6522 */
/* Written from the MOS6522 datasheet */
/* TODO: shift register */

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

extern char tapefile[], tapepath[];
extern SDL_bool refreshtape;

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

// Emulate the specified cpu-cycles time for the tape
void tape_ticktock( struct machine *oric, int cycles )
{
  Sint32 i, j;
  SDL_bool romon;

  // Determine if the ROM is currently active
  // (for turbotape purposes)
  romon = SDL_TRUE;
  if( oric->drivetype == DRV_JASMIN )
  {
    if( oric->jasmin.olay == 0 )
    {
      romon = !oric->romdis;
    } else {
      romon = SDL_FALSE;
    }
  } else {
    romon = !oric->romdis;
  }

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

  if( ( oric->pch_fd_available ) && ( romon ) )
  {
    // Patch CLOAD to insert a tape image specified.
    // Unfortunately the way the ROM works means we
    // only have up to 16 chars.
    if( oric->cpu.pc == oric->pch_fd_getname_pc )
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

  // No tape? Motor off?
  if( ( !oric->tapebuf ) || ( !oric->tapemotor ) )
  {
    if( ( oric->pch_tt_available ) && ( oric->tapeturbo ) && ( romon ) && ( oric->cpu.pc == oric->pch_tt_getsync_pc ) )
      oric->tapeturbo_syncstack = oric->cpu.sp;
    return;
  }

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

  if( ( oric->tapeturbo_forceoff ) && ( oric->pch_tt_available ) && ( romon ) && ( oric->cpu.pc == oric->pch_tt_getsync_end_pc ) )
    oric->tapeturbo_forceoff = SDL_FALSE;

  // Maybe do turbotape
  if( ( oric->pch_tt_available ) && ( oric->tapeturbo ) && ( !oric->tapeturbo_forceoff ) && ( romon ) )
  {
    SDL_bool dosyncpatch;

    dosyncpatch = SDL_FALSE;

    if( oric->cpu.pc == oric->pch_tt_getsync_loop_pc )
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

    if( ( oric->cpu.pc == oric->pch_tt_getsync_pc ) || ( dosyncpatch ) )
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
      oric->cpu.pc = oric->pch_tt_getsync_end_pc;
      return;
    }

    if( oric->cpu.pc == oric->pch_tt_readbyte_pc )
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
      oric->cpu.pc = oric->pch_tt_readbyte_end_pc;
      if( oric->tapeoffs >= oric->tapelen ) refreshtape = SDL_TRUE;
    }
    return;
  }

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

// Send a byte to the printer. It sets up
// two timers; one to count down and then
// do the response from the printer, the
// other to close the printer_out.txt filehandle
// after the printer is idle for a while.
void lprintchar( struct machine *oric, char c )
{
  // If the printer handle isn't currently open,
  // open it and do a popup to tell the user.
  if( !oric->prf )
  {
    oric->prf = fopen( "printer_out.txt", "a" );
    if( !oric->prf )
    {
      do_popup( oric, "Printing failed :-(" );
      return;
    }
    
    do_popup( oric, "Printing to 'printer_out.txt'" );
  }
  
  // Put the char to the file, set up the timers
  fputc( c, oric->prf );
  oric->prclock = 40;
  oric->prclose = 64*312*50*5;
  via_write_CA1( &oric->via, 1 );
}


// Update the CPU IRQ flags depending on the state of the VIA
static inline void via_check_irq( struct via *v )
{
  if( ( v->ier & v->ifr )&0x7f )
  {
    v->oric->cpu.irq  |= v->irqbit;
    v->ifr |= 0x80;
  } else {
    v->oric->cpu.irq &= ~v->irqbit;
    v->ifr &= 0x7f;
  }
}

// Set a bit in the IFR
static inline void via_set_irq( struct via *v, unsigned char bit )
{
  v->ifr |= bit;
  if( ( v->ier & v->ifr )&0x7f )
    v->ifr |= 0x80;

  if( ( v->ier & bit ) == 0 ) return;
  v->oric->cpu.irq |= v->irqbit;
}

// Clear a bit in the IFR
static inline void via_clr_irq( struct via *v, unsigned char bit )
{
  if( bit & VIRQF_CA1 )
    v->iral = v->ira;

  if( bit & VIRQF_CA2 )
    v->irbl = v->irb;

  v->ifr &= ~bit;
  if( (( v->ier & v->ifr )&0x7f) == 0 )
  {
    v->oric->cpu.irq &= ~v->irqbit;
    v->ifr &= 0x7f;
  }
}


void via_main_w_iorb( struct via *v, unsigned char oldorb )
{
  // Look for negative edge on printer strobe
  if( ( (oldorb&v->ddrb&0x10) != 0 ) &&
      ( (v->orb&v->ddrb&0x10) == 0 ) )
    lprintchar( v->oric, v->ora );

  tape_setmotor( v->oric, (v->orb&v->ddrb&0x40) != 0 );

  if( (v->pcr&PCRF_CB2CON) == 0x80 )
    ay_set_bdir( &v->oric->ay, 0 );

  ay_update_keybits( &v->oric->ay );
}

void via_main_w_iora( struct via *v )
{
  if( (v->pcr&PCRF_CA2CON) == 0x08 )
    ay_set_bc1( &v->oric->ay, 0 );

  ay_modeset( &v->oric->ay );
  joy_buildmask( v->oric );
}

void via_main_w_iora2( struct via *v )
{
  ay_modeset( &v->oric->ay );
  joy_buildmask( v->oric );
}

void via_main_w_ddra( struct via *v )
{
  joy_buildmask( v->oric );
}

void via_main_w_ddrb( struct via *v )
{
  joy_buildmask( v->oric );
  ay_update_keybits( &v->oric->ay );
}

void via_main_w_pcr( struct via *v )
{
  SDL_bool updateay = SDL_FALSE;

  switch( v->pcr & PCRF_CA2CON )
  {
    case 0x0c:
      updateay = SDL_TRUE;
      break;
  }

//  if( (v->acr&ACRF_SRCON) == 0 )
  {
    switch( v->pcr & PCRF_CB2CON )
    {
      case 0xc0:
      case 0xe0:
        updateay = SDL_TRUE;
        break;
    }
  }
      
  if( updateay ) ay_set_bcmode( &v->oric->ay, v->ca2, v->cb2 );
}

void via_main_w_ca2ext( struct via *v )
{
  switch( v->pcr & PCRF_CA2CON )
  {
    case 0x00:  // Interrupt on negative transision
    case 0x02:  // Independant negative transition
    case 0x04:  // Interrupt on positive transition
    case 0x06:  // Independant positive transition
      ay_set_bc1( &v->oric->ay, v->ca2 );
      break;
    
    // 0x08, 0x0a, 0x0c, 0x0e: output modes. Nothing to do :)
  }
}

void via_main_w_cb2ext( struct via *v )
{
  switch( v->pcr & PCRF_CB2CON )
  {
    case 0x00:
    case 0x20:
    case 0x40:
    case 0x60:
      ay_set_bdir( &v->oric->ay, v->cb2 );
      break;
  }
}

void via_main_r_iora( struct via *v )
{
  ay_modeset( &v->oric->ay );
  if( (v->pcr&PCRF_CA2CON) == 0x08 )
    ay_set_bc1( &v->oric->ay, 0 );
}

void via_main_r_iora2( struct via *v )
{
  ay_modeset( &v->oric->ay );
}

void via_main_r_iorb( struct via *v )
{
  if( (v->pcr&PCRF_CB2CON) == 0x80 )
    ay_set_bdir( &v->oric->ay, 0 );
}

void via_main_ca2pulsed( struct via *v )
{
  ay_set_bc1( &v->oric->ay, 1 );
}

void via_main_cb2pulsed( struct via *v )
{
  ay_set_bdir( &v->oric->ay, v->cb2 );
}

void via_tele_w_iora( struct via *v )
{
  int invddra = (v->ddra&7)^7;
  v->oric->tele_currbank = (v->oric->tele_currbank&invddra)|(v->ora&v->ddra&0x07);
  v->oric->tele_banktype = v->oric->tele_bank[v->oric->tele_currbank].type;
  v->oric->rom           = v->oric->tele_bank[v->oric->tele_currbank].ptr;
}

void via_tele_w_iora2( struct via *v )
{
  int invddra = (v->ddra&7)^7;
  v->oric->tele_currbank = (v->oric->tele_currbank&invddra)|(v->ora&v->ddra&0x07);
  v->oric->tele_banktype = v->oric->tele_bank[v->oric->tele_currbank].type;
  v->oric->rom           = v->oric->tele_bank[v->oric->tele_currbank].ptr;
}

void via_tele_w_ddra( struct via *v )
{
  int invddra = (v->ddra&7)^7;
  v->oric->tele_currbank = (v->oric->tele_currbank&invddra)|(v->ora&v->ddra&0x07);
  v->oric->tele_banktype = v->oric->tele_bank[v->oric->tele_currbank].type;
  v->oric->rom           = v->oric->tele_bank[v->oric->tele_currbank].ptr;
}

// Read ports from external device
unsigned char via_read_porta( struct via *v )
{
  return v->ora & v->ddra;
}

unsigned char via_read_portb( struct via *v )
{
  return v->orb & v->ddrb;
}

// Write ports from external device
void via_main_write_porta( struct via *v, unsigned char mask, unsigned char data )
{
  v->ira = (v->ira&~mask)|(data&mask);

  if( (!(v->acr&ACRF_PALATCH)) ||
      (!(v->ifr&VIRQF_CA1)) )
    v->iral = v->ira;
}

void via_tele_write_porta( struct via *v, unsigned char mask, unsigned char data )
{
  v->ira = (v->ira&~mask)|(data&mask);

  if( (!(v->acr&ACRF_PALATCH)) ||
      (!(v->ifr&VIRQF_CA1)) )
    v->iral = v->ira;
}

void via_write_portb( struct via *v, unsigned char mask, unsigned char data )
{
  unsigned char lastpb6;

  lastpb6 = v->irb&0x40;
  v->irb = (v->irb&~mask)|(data&mask);

  if( (!(v->acr&ACRF_PBLATCH)) ||
      (!(v->acr&VIRQF_CB1)) )
    v->irbl = v->irb;

  // Is timer 2 counting PB6 negative transisions?
  if( ( v->acr & ACRF_T2CON ) == 0 ) return;

  // Was there a negative transision?
  if( ( !lastpb6 ) || ( v->irb&0x40 ) ) return;

  // Count it
  v->t2c--;
  if( v->t2run )
  {
    if( v->t2c == 0 )
    {
      via_set_irq( v, VIRQF_T2 );
      v->t2run = SDL_FALSE;
    }
  }
}

// Init/Reset VIA
void via_init( struct via *v, struct machine *oric, int viatype )
{
  v->orb = 0;
  v->ora = 0;
  v->irb = 0;
  v->ira = 0;
  v->irbl = 0;
  v->iral = 0;
  v->ddra = 0;
  v->ddrb = 0;
  v->t1l_l = 0;
  v->t1l_h = 0;
  v->t1c = 0;
  v->t2l_l = 0;
  v->t2l_h = 0;
  v->t2c = 0;
  v->sr = 0;
  v->acr = 0;
  v->pcr = 0;
  v->ifr = 0;
  v->ier = 0;
  v->ca1 = 0;
  v->ca2 = 0;
  v->cb1 = 0;
  v->cb2 = 0;
  v->srcount = 0;
  v->srtime = 0;
  v->srtrigger = SDL_FALSE;
  v->t1run = SDL_FALSE;
  v->t2run = SDL_FALSE;
  v->ca2pulse = SDL_FALSE;
  v->cb2pulse = SDL_FALSE;
  v->oric = oric;

  switch( viatype )
  {
    case VIA_MAIN:
      v->w_iorb     = via_main_w_iorb;
      v->w_iora     = via_main_w_iora;
      v->w_iora2    = via_main_w_iora2;
      v->w_ddra     = via_main_w_ddra;
      v->w_ddrb     = via_main_w_ddrb;
      v->w_pcr      = via_main_w_pcr;
      v->w_ca2ext   = via_main_w_ca2ext;
      v->w_cb2ext   = via_main_w_cb2ext;
      v->r_iora     = via_main_r_iora;
      v->r_iora2    = via_main_r_iora2;
      v->r_iorb     = via_main_r_iorb;
      v->ca2pulsed  = via_main_ca2pulsed;
      v->cb2pulsed  = via_main_cb2pulsed;
      v->cb2shifted = via_main_cb2pulsed;  // Does the same (for now)
      v->irqbit     = IRQF_VIA;
      v->read_port_a  = via_read_porta;
      v->read_port_b  = via_read_portb;
      v->write_port_a = via_main_write_porta;
      v->write_port_b = via_write_portb;
      break;

    case VIA_TELESTRAT:
      v->w_iorb     = NULL;
      v->w_iora     = via_tele_w_iora;
      v->w_iora2    = via_tele_w_iora2;
      v->w_ddra     = via_tele_w_ddra;
      v->w_ddrb     = NULL;
      v->w_pcr      = NULL;
      v->w_ca2ext   = NULL;
      v->w_cb2ext   = NULL;
      v->r_iora     = NULL;
      v->r_iora2    = NULL;
      v->r_iorb     = NULL;
      v->ca2pulsed  = NULL;
      v->cb2pulsed  = NULL;
      v->cb2shifted = NULL;
      v->irqbit     = IRQF_VIA2;
      v->read_port_a  = via_read_porta;
      v->read_port_b  = via_read_portb;
      v->write_port_a = via_tele_write_porta;
      v->write_port_b = via_write_portb;
      break;
  }
}

// Move timers on etc.
void via_clock( struct via *v, unsigned int cycles )
{
  unsigned int crem;

  // Move on the tape emulation
  tape_ticktock( v->oric, cycles );

  if( !cycles ) return;

  // Simulate the feedback from the printer
  if( v->oric->prclock > 0 )
  {
    v->oric->prclock -= cycles;
    if( v->oric->prclock <= 0 )
    {
      v->oric->prclock = 0;
      via_write_CA1( v, 0 );
    }
  }

  // This is just a timer to close the
  // "printer_out.txt" filehandle if the
  // oric printer is idle for a while
  if( v->oric->prclose > 0 )
  {
    v->oric->prclose -= cycles;
    if( v->oric->prclose <= 0 )
    {
      v->oric->prclose = 0;
      if( v->oric->prf )
        fclose( v->oric->prf );
      v->oric->prf = NULL;
    }
  }

  if( v->ca2pulse )
  {
    v->ca2 = 1;
    if( v->ca2pulsed ) v->ca2pulsed( v );
    v->ca2pulse = SDL_FALSE;
  }

  if( v->cb2pulse )
  {
//    if( (v->acr&ACRF_SRCON) == 0 )
    {
      v->cb2 = 1;
      if( v->cb2pulsed ) v->cb2pulsed( v );
    }
    v->cb2pulse = SDL_FALSE;
  }

  // Timer 1 tick-tock
  switch( v->acr&ACRF_T1CON )
  {
    case 0x00:  // One-shot, no output
    case 0x80:  // One-shot, PB7 low while counting
      if( v->t1run ) // Doing the one-shot still?
      {
        if( cycles > v->t1c )  // Going to end the timer run?
        {
          via_set_irq( v, VIRQF_T1 );
          if( v->acr&0x80 ) v->orb |= 0x80;   // PB7 high now that the timer has finished
          v->t1run = SDL_FALSE;
        }
      }
      v->t1c -= cycles;
      break;
    
    case 0x40: // Continuous, no output
    case 0xc0: // Continuous, output on PB7
      crem = cycles;

      if( v->t1reload )
      {
        crem--;
        v->t1c = (v->t1l_h<<8)|v->t1l_l;       // Reload timer
        v->t1reload = SDL_FALSE;
      }

      while( crem > v->t1c )
      {
        crem -= (v->t1c+1); // Clock down to 0xffff
        v->t1c = 0xffff;
        via_set_irq( v, VIRQF_T1 );
        if( v->acr&0x80 ) v->orb ^= 0x80;     // PB7 low now that the timer has finished

        if( !crem )
        {
          v->t1reload = SDL_TRUE;
          break;
        }

        crem--;
        v->t1c = (v->t1l_h<<8)|v->t1l_l;       // Reload timer
      }

      if( v->t1c >= crem )
        v->t1c -= crem;
      break;
  }

  // Timer 2 tick-tock
  if( ( v->acr & ACRF_T2CON ) == 0 )
  {
    // Timer 2 is one-shot
    if( v->t2run )
    {
      if( cycles > v->t2c )
      {
        via_set_irq( v, VIRQF_T2 );
        v->t2run = SDL_FALSE;
      }
    }
    v->t2c -= cycles;
  }

  switch( v->acr & ACRF_SRCON )
  {
    case 0x00:    // Disabled
    case 0x04:    // Shift in under T2 control (not implemented)
    case 0x08:    // Shift in under O2 control (not implemented)
    case 0x0c:    // Shift in under control of external clock (not implemented)
    case 0x1c:    // Shift out under control of external clock (not implemented)
      if( v->ifr&VIRQF_SR )
        via_clr_irq( v, VIRQF_SR );
      v->srcount = 0;
      v->srtrigger = SDL_FALSE;
      break;

    case 0x10:    // Shift out free-running at T2 rate
      if( !v->srtrigger ) break;

      // Count down the SR timer
      v->srtime -= cycles;
      while( v->srtime <= 0 )
      {
        // SR timer elapsed! Reload it
        v->srtime += v->t2l_l+1;

        // Toggle the clock!
        v->cb1 ^= 1;

        // Need to change cb2?
        if( !v->cb1 )
        {
          v->cb2 = (v->sr & 0x80) ? 1 : 0;
          v->sr = (v->sr<<1)|v->cb2;
          if( v->cb2shifted ) v->cb2shifted( v );

          v->srcount = (v->srcount+1)%8;
        }        
      }
      break;

    case 0x14:    // Shift out under T2 control
      if( !v->srtrigger ) break;
      if( v->srcount == 8 ) break;

      // Count down the SR timer
      v->srtime -= cycles;
      while( v->srtime <= 0 )
      {
        // SR timer elapsed! Reload it
        v->srtime += v->t2l_l+1;

        // Toggle the clock!
        v->cb1 ^= 1;

        // Need to change cb2?
        if( !v->cb1 )
        {
          v->cb2 = (v->sr & 0x80) ? 1 : 0;
          v->sr = (v->sr<<1)|v->cb2;
          if( v->cb2shifted ) v->cb2shifted( v );

          v->srcount++;
          if( v->srcount == 8 )
          {
            via_set_irq( v, VIRQF_SR );
            v->srtime = 0;
            v->srtrigger = SDL_FALSE;
            break;
          }
        }
      }
      break;

    case 0x18:    // Shift out under O2 control
      if( !v->srtrigger ) break;
      if( v->srcount == 8 ) break;

      // Toggle the clock!
      v->cb1 ^= 1;

      // Need to change cb2?
      if( !v->cb1 )
      {
        v->cb2 = (v->sr & 0x80) ? 1 : 0;
        v->sr = (v->sr<<1)|v->cb2;
        if( v->cb2shifted ) v->cb2shifted( v );

        v->srcount++;
        if( v->srcount == 8 )
        {
          via_set_irq( v, VIRQF_SR );
          v->srtrigger = SDL_FALSE;
        }
      }
      break;    
  }   
}

// Write VIA from CPU
void via_write( struct via *v, int offset, unsigned char data )
{
  unsigned char tmp;
  offset &= 0xf;

  switch( offset )
  {
    case VIA_IORB:
      tmp = v->orb;    
      v->orb = data;

      via_clr_irq( v, VIRQF_CB1 );
      switch( v->pcr & PCRF_CB2CON )
      {
        case 0x00:
        case 0x40:
          via_clr_irq( v, VIRQF_CB2 );
          break;
        
        case 0xa0:
          v->cb2pulse = SDL_TRUE;
        case 0x80:
//          if( (v->acr&ACRF_SRCON) != 0 )
            v->cb2 = 0;
          break;
      }
      if( v->w_iorb ) v->w_iorb( v, tmp );
      break;
    case VIA_IORA:
      v->ora = data;
      via_clr_irq( v, VIRQF_CA1 );
      switch( v->pcr & PCRF_CA2CON )
      {
        case 0x00:
        case 0x04:
          via_clr_irq( v, VIRQF_CA2 );
          break;
        
        case 0x0a:
          v->ca2pulse = SDL_TRUE;
        case 0x08:
          v->ca2 = 0;
          break;
      }
      if( v->w_iora ) v->w_iora( v );
      break;
    case VIA_DDRB:
      v->ddrb = data;
      if( v->w_ddrb ) v->w_ddrb( v );
      break;
    case VIA_DDRA:
      v->ddra = data;
      if( v->w_ddra ) v->w_ddra( v );
      break;
    case VIA_T1C_H:
      v->t1l_h = data;
      v->t1c = (v->t1l_h<<8)|v->t1l_l;
      via_clr_irq( v, VIRQF_T1 );
      v->t1run = SDL_TRUE;
      if( (v->acr&ACRF_T1CON) == 0x80 ) v->orb &= 0x7f;
      break;
    case VIA_T1C_L:
    case VIA_T1L_L:
      v->t1l_l = data;
      break;
    case VIA_T1L_H:
      v->t1l_h = data;
      via_clr_irq( v, VIRQF_T1 );
      break;
    case VIA_T2C_L:
      v->t2l_l = data;
      break;
    case VIA_T2C_H:
      v->t2l_h = data;
      v->t2c = (v->t2l_h<<8)|v->t2l_l;
      via_clr_irq( v, VIRQF_T2 );
      v->t2run = SDL_TRUE;
      break;
    case VIA_SR:
      v->sr = data;
      v->srcount = 0;
      v->srtime  = 0;
      v->srtrigger = SDL_TRUE;
      via_clr_irq( v, VIRQF_SR );
      break;
    case VIA_ACR:
      // Disabling SR?
/*
      if( ( (v->acr&ACRF_SRCON) != 0 ) &&
          ( (data&ACRF_SRCON) == 0 ) )
      {
        switch( v->pcr & PCRF_CB2CON )
        {
          case 0xc0:
            v->cb2 = 0;
            v->cb2pulse = SDL_FALSE;
            break;
        
          case 0xe0:
            v->cb2 = 1;
            break;
        }
        ay_set_bcmode( &v->oric->ay, v->ca2, v->cb2 );
      }
*/
      v->acr = data;
      if( ( ( data & ACRF_T1CON ) != 0x40 ) &&
          ( ( data & ACRF_T1CON ) != 0xc0 ) )
        v->t1reload = SDL_FALSE;
      break;
    case 0x40: // Continuous, no output
    case 0xc0: // Continuous, output on PB7
      break;
    case VIA_PCR:
      v->pcr = data;
      switch( v->pcr & PCRF_CA2CON )
      {
        case 0x0c:
          v->ca2 = 0;
          v->ca2pulse = SDL_FALSE;
          break;
        
        case 0x0e:
          v->ca2 = 1;
          break;
      }

//      if( (v->acr&ACRF_SRCON) == 0 )
      {
        switch( v->pcr & PCRF_CB2CON )
        {
          case 0xc0:
            v->cb2 = 0;
            v->cb2pulse = SDL_FALSE;
            break;
        
          case 0xe0:
            v->cb2 = 1;
            break;
        }
      }

      if( v->w_pcr ) v->w_pcr( v );
      break;
    case VIA_IFR:
      if( data & VIRQF_CA1 ) v->iral = v->ira;
      if( data & VIRQF_CB1 ) v->irbl = v->irb;

      v->ifr &= (~data)&0x7f;
      if( ( v->ier & v->ifr )&0x7f )
        v->ifr |= 0x80;
      else
        v->oric->cpu.irq &= ~v->irqbit;
      break;
    case VIA_IER:
      if( data & 0x80 )
        v->ier |= (data&0x7f);
      else
        v->ier &= ~(data&0x7f);
      via_check_irq( v );
      break;
    case VIA_IORA2:
      v->ora = data;
      if( v->w_iora2 ) v->w_iora2( v );
      break;
  }
}

// Read without side-effects for monitor
unsigned char via_mon_read( struct via *v, int offset )
{
  offset &= 0xf;

  switch( offset )
  {
    case VIA_IORB:
      return (v->orb&v->ddrb)|(v->irbl&(~v->ddrb));
    case VIA_IORA:
      return (v->ora&v->ddra)|(v->iral&(~v->ddra));
    case VIA_DDRB:
      return v->ddrb;
    case VIA_DDRA:
      return v->ddra;
    case VIA_T1C_L:
      return v->t1c&0xff;
    case VIA_T1C_H:
      return v->t1c>>8;
    case VIA_T1L_L:
      return v->t1l_l;
    case VIA_T1L_H:
      return v->t1l_h;
    case VIA_T2C_L:
      return v->t2c&0xff;
    case VIA_T2C_H:
      return v->t2c>>8;
    case VIA_SR:
      return v->sr;
    case VIA_ACR:
      return v->acr;
    case VIA_PCR:
      return v->pcr;
    case VIA_IFR:
      return v->ifr;
    case VIA_IER:
      return v->ier|0x80;
    case VIA_IORA2:
      return (v->ora&v->ddra)|(v->iral&(~v->ddra));
  }

  return 0;
}

// Read VIA from CPU
unsigned char via_read( struct via *v, int offset )
{
  offset &= 0xf;

  switch( offset )
  {
    case VIA_IORB:
      via_clr_irq( v, VIRQF_CB1 );
      switch( v->pcr & PCRF_CB2CON )
      {
        case 0x00:
        case 0x40:
          via_clr_irq( v, VIRQF_CB2 );
          break;

        case 0xa0:
          v->cb2pulse = SDL_TRUE;
        case 0x80:
//          if( (v->acr&ACRF_SRCON) == 0 )
          {
            v->cb2 = 0;
          }
          break;
      }
      if( v->r_iorb ) v->r_iorb( v );
      return (v->orb&v->ddrb)|(v->irbl&(~v->ddrb));
    case VIA_IORA:
      via_clr_irq( v, VIRQF_CA1 );
      switch( v->pcr & PCRF_CA2CON )
      {
        case 0x00:
        case 0x04:
          via_clr_irq( v, VIRQF_CA2 );
          break;

        case 0x0a:
          v->ca2pulse = SDL_TRUE;
        case 0x08:
          v->ca2 = 0;
          break;
      }
      if( v->r_iora ) v->r_iora( v );
      return (v->ora&v->ddra)|(v->iral&(~v->ddra));
    case VIA_DDRB:
      return v->ddrb;
    case VIA_DDRA:
      return v->ddra;
    case VIA_T1C_L:
      via_clr_irq( v, VIRQF_T1 );
      return v->t1c&0xff;
    case VIA_T1C_H:
      return v->t1c>>8;
    case VIA_T1L_L:
      return v->t1l_l;
    case VIA_T1L_H:
      return v->t1l_h;
    case VIA_T2C_L:
      via_clr_irq( v, VIRQF_T2 );
      return v->t2c&0xff;
    case VIA_T2C_H:
      return v->t2c>>8;
    case VIA_SR:
      v->srcount = 0;
      v->srtime  = 0;
      v->srtrigger = SDL_TRUE;
      via_clr_irq( v, VIRQF_SR );
      return v->sr;
    case VIA_ACR:
      return v->acr;
    case VIA_PCR:
      return v->pcr;
    case VIA_IFR:
      return v->ifr;
    case VIA_IER:
      return v->ier|0x80;
    case VIA_IORA2:
      return (v->ora&v->ddra)|(v->iral&(~v->ddra));
  }

  return 0;
}

// For the monitor
void via_mon_write_ifr( struct via *v, unsigned char data )
{
  if( !( data & VIRQF_CA1 ) ) v->iral = v->ira;
  if( !( data & VIRQF_CB1 ) ) v->irbl = v->irb;

  v->ifr = data&0x7f;
  if( ( v->ier & v->ifr )&0x7f )
    v->ifr |= 0x80;
  else
    v->oric->cpu.irq &= ~v->irqbit;
}

// Write CA1,CA2,CB1,CB2 from external device
void via_write_CA1( struct via *v, unsigned char data )
{
  if( ( !v->ca1 ) && ( data != 0 ) ) // Positive transition?
  {
    if( v->pcr & PCRF_CA1CON )
      via_set_irq( v, VIRQF_CA1 );
  } else if( ( v->ca1 ) && ( data == 0 ) ) {   // Negative transition?
    if( !(v->pcr & PCRF_CA1CON) )
      via_set_irq( v, VIRQF_CA1 );
  }

  v->ca1 = data != 0;
}

void via_write_CA2( struct via *v, unsigned char data )
{
  switch( v->pcr & PCRF_CA2CON )
  {
    case 0x00:  // Interrupt on negative transision
    case 0x02:  // Independant negative transition
      if( ( v->ca2 ) && ( data == 0 ) )
        via_set_irq( v, VIRQF_CA2 );
      v->ca2 = data != 0;
      break;
    
    case 0x04:  // Interrupt on positive transition
    case 0x06:  // Independant positive transition
      if( ( !v->ca2 ) && ( data != 0 ) )
        via_set_irq( v, VIRQF_CA2 );
      v->ca2 = data != 0;
      break;
    
    // 0x08, 0x0a, 0x0c, 0x0e: output modes. Nothing to do :)
  }

  if( v->w_ca2ext ) v->w_ca2ext( v );
}

void via_write_CB1( struct via *v, unsigned char data )
{
  if( ( !v->cb1 ) && ( data != 0 ) ) // Positive transition?
  {
    if( v->pcr & PCRF_CB1CON )
      via_set_irq( v, VIRQF_CB1 );
  } else if( ( v->cb1 ) && ( data == 0 ) ) {   // Negative transition?
    if( !(v->pcr & PCRF_CB1CON) )
      via_set_irq( v, VIRQF_CB1 );
  }

  v->cb1 = data != 0;
}

void via_write_CB2( struct via *v, unsigned char data )
{
  switch( v->pcr & PCRF_CB2CON )
  {
    case 0x00:
    case 0x20:
      if( ( v->cb2 ) && ( data == 0 ) )
        via_set_irq( v, VIRQF_CB2 );
      v->cb2 = data != 0;
      break;
    
    case 0x40:
    case 0x60:
      if( ( !v->cb2 ) && ( data != 0 ) )
        via_set_irq( v, VIRQF_CB2 );
      v->cb2 = data != 0;
      break;
  }

  if( v->w_cb2ext ) v->w_cb2ext( v );
}
