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

/* VIA 6522 */
/* Written from the MOS6522 datasheet */
/* TODO: shift register */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <SDL.h>

#include "6502.h"
#include "via.h"
#include "8912.h"
#include "gui.h"
#include "disk.h"
#include "machine.h"

extern char tapefile[], tapepath[];

void tape_popup( struct machine *oric )
{
  char tmp[40];
  if( !oric->tapename[0] )
  {
    do_popup( "\x0f\x10\x13" );
    return;
  }

  sprintf( tmp, "\x0f\x10""%c %-16s", oric->tapemotor ? 18 : 17, oric->tapename );
  do_popup( tmp );
}

void tape_setmotor( struct machine *oric, SDL_bool motoron )
{
  if( motoron == oric->tapemotor )
    return;

  if( ( !oric->tapeturbo ) || ( oric->type != MACH_ATMOS ) )
  {
    if( !motoron )
    {
      if( oric->tapebit > 0 )
        oric->tapeoffs++;
      oric->tapebit = 0;
    }

    if( ( motoron ) &&
        ( oric->tapebuf ) &&
        ( oric->tapeoffs < oric->tapelen ) &&
        ( oric->tapebuf[oric->tapeoffs] == 0x16 ) )
      oric->tapedupbytes = 80;
  }

  oric->tapemotor = motoron;
  if( oric->tapename[0] ) tape_popup( oric );
}

void tape_eject( struct machine *oric )
{
  if( oric->tapebuf ) free( oric->tapebuf );
  oric->tapebuf = NULL;
  oric->tapelen = 0;
  oric->tapename[0] = 0;
  tape_popup( oric );
}

void tape_rewind( struct machine *oric )
{
  oric->tapeoffs   = 0;
  oric->tapebit    = 0;
  oric->tapecount  = 2;
  oric->tapeout    = 0;
  oric->tapedupbytes = 1;
}

SDL_bool tape_load_tap( struct machine *oric, char *fname )
{
  FILE *f;

  f = fopen( fname, "rb" );
  if( !f ) return SDL_FALSE;

  tape_eject( oric );

  fseek( f, 0, SEEK_END );
  oric->tapelen = ftell( f );
  fseek( f, 0, SEEK_SET );

  if( oric->tapelen <= 0 )
  {
    fclose( f );
    return SDL_FALSE;
  }

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

  oric->tapebuf[oric->tapelen++] = 0;

  fclose( f );
  tape_rewind( oric );
  strncpy( oric->tapename, fname, 16 );
  oric->tapename[15] = 0;
  tape_popup( oric );
  return SDL_TRUE;
};

void tape_ticktock( struct machine *oric, int cycles )
{
  Sint32 i, j;
  char *odir;

  if( oric->type == MACH_ATMOS )
  {
    switch( oric->cpu.pc )
    {
      case 0xe85f: // Decoded filename
        if( !oric->autoinsert ) break;
        if( oric->cpu.read( &oric->cpu, 0x027f ) == 0 ) break;

        for( i=0; i<16; i++ )
        {
          j = oric->cpu.read( &oric->cpu, 0x027f+i );
          if( !j ) break;
          tapefile[i] = j;
        }
        tapefile[i] = 0;
        oric->cpu.write( &oric->cpu, 0x27f, 0 );

        odir = getcwd( NULL, 0 );
        chdir( tapepath );
        tape_load_tap( oric, tapefile );
        chdir( odir );
        free( odir );
        break;
    }
  }

  if( ( !oric->tapebuf ) || ( !oric->tapemotor ) ) return;
  if( ( oric->tapeoffs < 0 ) || ( oric->tapeoffs >= oric->tapelen ) )
    return;

  if( ( oric->type == MACH_ATMOS ) && ( oric->tapeturbo ) )
  {
    switch( oric->cpu.pc )
    {
      case 0xe735: // Cassette sync
        if( oric->tapebuf[oric->tapeoffs] != 0x16 )
        {
          do
          {
            oric->tapeoffs++;
            if( oric->tapeoffs >= oric->tapelen )
              return;
          } while( oric->tapebuf[oric->tapeoffs] != 0x16 );
        }
        oric->cpu.pc = 0xe759;
        break;
      
      case 0xe6c9: // Read byte
        oric->cpu.a = oric->tapebuf[oric->tapeoffs++];
        oric->cpu.f_z = oric->cpu.a == 0;
        oric->cpu.f_c = 1;
        oric->cpu.write( &oric->cpu, 0x2f, oric->cpu.a );
        oric->cpu.write( &oric->cpu, 0x2b1, 0x00 );
        oric->cpu.pc = 0xe6fb;
        break;
    }
    return;
  }

  if( oric->tapecount > cycles )
  {
    oric->tapecount -= cycles;
    return;
  }

  oric->tapeout ^= 1;
  via_write_CB1( &oric->via, oric->tapeout );

  if( oric->tapeout ) // New bit?
  {
    switch( oric->tapebit )
    {
      case 0:
        oric->tapetime = TAPE_1_PULSE;
        break;

      case 1:  // sync pulse
        oric->tapetime = TAPE_0_PULSE;
        oric->tapeparity = 1;
        break;
      
      default:
        if( oric->tapebuf[oric->tapeoffs]&(1<<(oric->tapebit-2)) )
        {
          oric->tapetime = TAPE_1_PULSE;
          oric->tapeparity ^= 1;
        } else {
          oric->tapetime = TAPE_0_PULSE;
        }
        break;

      case 10:  // Parity bit
        if( oric->tapeparity )
          oric->tapetime = TAPE_1_PULSE;
        else
          oric->tapetime = TAPE_0_PULSE;
        break;

      case 11:
      case 12:
      case 13:
      case 14:
      case 15:
        oric->tapetime = TAPE_1_PULSE;
        break;
    }

    oric->tapebit = (oric->tapebit+1)%16;
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

// Init/Reset
void via_init( struct via *v, struct machine *oric )
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
  v->t1run = SDL_FALSE;
  v->t2run = SDL_FALSE;
  v->ca2pulse = SDL_FALSE;
  v->cb2pulse = SDL_FALSE;
  v->oric = oric;
}

void via_reset( struct via *v )
{
}

static inline void via_set_irq( struct via *v, unsigned char bit )
{
  v->ifr |= bit;
  if( ( v->ier & v->ifr )&0x7f )
    v->ifr |= 0x80;

  if( ( v->ier & bit ) == 0 ) return;
  v->oric->cpu.irq |= IRQF_VIA;
  v->oric->cpu.irql = SDL_TRUE;
}

static inline void via_clr_irq( struct via *v, unsigned char bit )
{
  if( bit & VIRQF_CA1 )
    v->iral = v->ira;

  if( bit & VIRQF_CA2 )
    v->irbl = v->irb;

  v->ifr &= ~bit;
  if( (( v->ier & v->ifr )&0x7f) == 0 )
  {
    v->oric->cpu.irq &= ~IRQF_VIA;
    v->ifr &= 0x7f;
  }
}

// Move timers on etc.
void via_clock( struct via *v, unsigned int cycles )
{
  unsigned int crem;

  tape_ticktock( v->oric, cycles );

  if( cycles >= 1 ) // :-)
  {
    if( v->ca2pulse )
    {
      v->ca2 = 1;
      ay_set_bc1( &v->oric->ay, 1 );
      v->ca2pulse = SDL_FALSE;
    }

    if( v->cb2pulse )
    {
      v->cb2 = 1;
      ay_set_bdir( &v->oric->ay, 1 );
      v->cb2pulse = SDL_FALSE;
    }
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
      while( crem > v->t1c )
      {
        // Continuous finished
        crem -= v->t1c;   // Done this one
        via_set_irq( v, VIRQF_T1 );
        if( v->acr&0x80 ) v->orb ^= 0x80;     // PB7 low now that the timer has finished
        v->t1c = (v->t1l_h<<8)|v->t1l_l;       // Reload timer
        if( v->t1c == 0 ) break; // Avoid an infinite loop
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
}

// Write VIA from CPU
void via_write( struct via *v, int offset, unsigned char data )
{
  offset &= 0xf;

  switch( offset )
  {
    case VIA_IORB:
      v->orb = data;

      tape_setmotor( v->oric, (v->orb&v->ddrb&0x40) != 0 );

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
          v->cb2 = 0;
          ay_set_bdir( &v->oric->ay, 0 );
          break;
      }
      ay_update_keybits( &v->oric->ay );
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
          ay_set_bc1( &v->oric->ay, 0 );
          break;
      }
      break;
    case VIA_DDRB:
      v->ddrb = data;
      ay_update_keybits( &v->oric->ay );
      break;
    case VIA_DDRA:
      v->ddra = data;
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
      break;
    case VIA_ACR:
      v->acr = data;
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
      break;
    case VIA_IFR:
      if( data & VIRQF_CA1 ) v->iral = v->ira;
      if( data & VIRQF_CB1 ) v->irbl = v->irb;

      v->ifr &= (~data)&0x7f;
      if( ( v->ier & v->ifr )&0x7f )
        v->ifr |= 0x80;
      else
        v->oric->cpu.irq &= ~IRQF_VIA;
      break;
    case VIA_IER:
      if( data & 0x80 )
        v->ier |= (data&0x7f);
      else
        v->ier &= ~(data&0x7f);
      break;

    case VIA_IORA2:
      v->ora = data;
      break;
  }
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
          v->cb2 = 0;
          ay_set_bdir( &v->oric->ay, 0 );
          break;
      }
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
          ay_set_bc1( &v->oric->ay, 0 );
          break;
      }
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
      return v->sr;
    case VIA_ACR:
      return v->acr;
    case VIA_PCR:
      return v->pcr;
    case VIA_IFR:
      return v->ifr;
    case VIA_IER:
      return v->ier&0x7f;
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
    v->oric->cpu.irq &= ~IRQF_VIA;
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
      ay_set_bc1( &v->oric->ay, v->ca2 );
      break;
    
    case 0x04:  // Interrupt on positive transition
    case 0x06:  // Independant positive transition
      if( ( !v->ca2 ) && ( data != 0 ) )
        via_set_irq( v, VIRQF_CA2 );
      v->ca2 = data != 0;
      ay_set_bc1( &v->oric->ay, v->ca2 );
      break;
    
    // 0x08, 0x0a, 0x0c, 0x0e: output modes. Nothing to do :)
  }
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
      ay_set_bdir( &v->oric->ay, v->cb2 );
      break;
    
    case 0x40:
    case 0x60:
      if( ( !v->cb2 ) && ( data != 0 ) )
        via_set_irq( v, VIRQF_CB2 );
      v->cb2 = data != 0;
      ay_set_bdir( &v->oric->ay, v->cb2 );
      break;
  }
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
void via_write_porta( struct via *v, unsigned char mask, unsigned char data )
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
