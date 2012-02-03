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
#include "tape.h"

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

  // Was there a negative transition?
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
  v->t1reload = 0;
  v->t2l_l = 0;
  v->t2l_h = 0;
  v->t2c = 0;
  v->t2reload = 0;
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
      v->orbchange  = tape_orbchange;
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
      v->orbchange  = NULL;
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
          if( v->acr&0x80 )
          {
            v->orb |= 0x80;   // PB7 high now that the timer has finished
            if (v->orbchange) v->orbchange( v );
          }
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
        if( v->acr&0x80 )
        {
          v->orb ^= 0x80;     // PB7 low now that the timer has finished
          if( v->orbchange ) v->orbchange( v );
        }

        if( !crem )
        {
          v->t1reload = SDL_TRUE;
          break;
        }

        crem--;
        v->t1c = (v->t1l_h<<8)|v->t1l_l;       // Reload timer
      }

      v->t1c -= crem;
      break;
  }

  // Timer 2 tick-tock
  if( ( v->acr & ACRF_T2CON ) == 0 )
  {
	crem = cycles;
	if( v->t2reload )
	{
	  crem--;
	  v->t2reload = 0;
	}

    // Timer 2 is one-shot
    if( v->t2run )
    {
      if( crem > v->t2c )
      {
        via_set_irq( v, VIRQF_T2 );
        v->t2run = SDL_FALSE;
      }
    }
    v->t2c -= crem;
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
      if( v->orbchange ) v->orbchange( v );
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
      v->t1reload = 1;
      via_clr_irq( v, VIRQF_T1 );
      v->t1run = SDL_TRUE;
      if( (v->acr&ACRF_T1CON) == 0x80 )
      {
        v->orb &= 0x7f;
        if( v->orbchange ) v->orbchange( v );
      }
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
	  v->t2reload = 1;
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
