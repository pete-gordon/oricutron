/*
**  Oricutron
**  Copyright (C) 2009-2012 Peter Gordon
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
**  6551 ACIA emulation
*/

#include "system.h"
#include "6502.h"
#include "via.h"
#include "8912.h"
#include "gui.h"
#include "disk.h"
#include "monitor.h"
#include "6551.h"
#include "machine.h"

#define DEBUG_ACIA    0

// 3 - Full console logging
#if DEBUG_ACIA == 3
#undef dbg_printf
#define dbg_printf(x...) { printf(x); printf("\n"); }
#define DEBUG_ACIA_STATUS 1
// 2 - Console logging with out noisy status
#elif DEBUG_ACIA == 2
#undef dbg_printf
#define dbg_printf(x...) { printf(x); printf("\n"); }
#define DEBUG_ACIA_STATUS 0
// 1 - Full monitor logging
#elif DEBUG_ACIA == 1
#define DEBUG_ACIA_STATUS 1
// 0 - Disable logging
#else
#define DEBUG_ACIA_STATUS 0
#endif

#if DEBUG_ACIA
static char *writereg_names[] = { "TXDATA", "RESET", "COMMAND", "CONTROL" };
static char *readreg_names[]  = { "RXDATA", "STATUS", "COMMAND", "CONTROL" };
static char *parity_modes[]   = { "NONE", "ODD", "NONE", "EVEN", "NONE", "MARK", "NONE", "SPACE" };
static char *txcon_modes[]    = { "TXIRQ=0,RLEV=H", "TXIRQ=1,RLEV=L", "RXIRQ=0,RLEV=L", "RXIRQ=0,RLEV=L,TXBRK" };
static char *baud_rates[]     = { "EXT", "50", "75", "109.92", "134.58", "150", "300", "600", "1200", "1800", "2400", "3600", "4800", "7200", "9600", "19200" };
static char *bit_nums[]       = { "8", "7", "6", "5" };
#endif

// NOTE: work with integers 109.92 and 134.58 are approximated
static Uint32 boudrates[] = { 0, 50, 75, 110, 134, 150, 300, 600, 1200, 1800, 2400, 3600, 4800, 7200, 9600, 19200 };

static void acia_update_params( struct acia *acia )
{
  switch(8 - ((acia->regs[ACIA_CONTROL]&ACONF_WLEN)>>5))
  {
    case 5: acia->bitmask = 0x1f; break;
    case 6: acia->bitmask = 0x3f; break;
    case 7: acia->bitmask = 0x7f; break;
    case 8: default: acia->bitmask = 0xff; break;
  }
  
  acia->rx = (acia->regs[ACIA_COMMAND] & ACOMF_DTR)? SDL_TRUE : SDL_FALSE;
  acia->irqrx = (acia->rx && 0 == ( acia->regs[ACIA_COMMAND] & ACOMF_IRQDIS ))? SDL_TRUE : SDL_FALSE;

  switch( ( acia->regs[ACIA_COMMAND] & ACOMF_TXCON ) >> 2 )
  {
    case 0:
      acia->tx = SDL_FALSE;
      acia->irqtx = SDL_FALSE;
      break;
    case 1:
      acia->tx = SDL_TRUE;
      acia->irqtx = SDL_TRUE;
      break;
    case 2:
      acia->tx = SDL_TRUE;
      acia->irqtx = SDL_FALSE;
      break;
    case 3:
      // FIXME: ACIA transmit BRK...
      acia->tx = SDL_FALSE;
      acia->irqtx = SDL_FALSE;
      break;
  }

  acia->echo = (acia->regs[ACIA_COMMAND]&ACOMF_ECHO)? SDL_TRUE : SDL_FALSE;

  // FIXME: calculate number of frame bits include 1.5 stop bits
  acia->framebits =
    (1) +                                               // start bit
    (8 - ((acia->regs[ACIA_CONTROL]&ACONF_WLEN)>>5)) +  // data bits
    ((acia->regs[ACIA_COMMAND]>>5)&1) +                 // parity bit_nums
    ((acia->regs[ACIA_CONTROL]&ACONF_STOPB) ? 2 : 1);   // stop bit(s)

  acia->boudrate = boudrates[acia->regs[ACIA_CONTROL]&ACONF_BAUD];

  // 1.8432 MHz / 16 (internal divider) = 115,2 kHz
  // 115,2 kHz => ~ 8.68 usec
  if(0 < acia->boudrate)
    acia->framecycle = ( 115200 * 8680 ) / ( acia->boudrate * 1000 );
  else
    acia->framecycle = 0;
}

void acia_init( struct acia *acia, struct machine *oric )
{
  if(acia->done)
    acia->done( acia );

  acia->oric = oric;
  acia->regs[ACIA_RXDATA]  = 0;
  acia->regs[ACIA_TXDATA]  = 0;
  acia->regs[ACIA_STATUS]  = ASTF_TXEMPTY;
  acia->regs[ACIA_COMMAND] = ACOMF_IRQDIS;
  acia->regs[ACIA_CONTROL] = 0;

  acia->cycles = 0;
  acia_update_params( acia );

  switch(oric->aciabackend)
  {
    case ACIA_TYPE_LOOPBACK:
      acia_init_loopback( acia );
      break;
#ifdef BACKEND_MODEM
    case ACIA_TYPE_MODEM:
      acia_init_modem( acia );
      break;
#endif
#ifdef BACKEND_COM
    case ACIA_TYPE_COM:
      acia_init_com( acia );
      break;
#endif
    case ACIA_TYPE_NONE:
    default:
      acia_init_none( acia );
      break;
  }
}

void acia_clock( struct acia *acia, unsigned int cycles )
{
  // EXT clock used - not implemented
  if( 0 == ( acia->regs[ACIA_CONTROL] & ACONF_SRC ))
    return;

  // INT clock
  if( 0 == acia->framecycle )
    return;

  acia->cycles += cycles;
  while( acia->framecycle <= acia->cycles )
  {
    acia->cycles -= acia->framecycle;

    // TX enabled
    if( acia->tx && 0 == (acia->regs[ACIA_STATUS] & ASTF_TXEMPTY) )
    {
      if(!acia->put_byte( acia->regs[ACIA_TXDATA] ) )
        acia->regs[ACIA_STATUS] |= ASTF_OVRUNERR;
      else
      {
        acia->regs[ACIA_STATUS] |= ASTF_TXEMPTY;
        if( acia->irqtx )
        {
          acia->regs[ACIA_STATUS] |= ASTF_IRQ;
          acia->oric->cpu.irq |= IRQF_ACIA;
        }
        if(acia->echo)
          acia->put_byte(acia->regs[ACIA_TXDATA]);
      }
    }

    // RX enabled
    if( acia-> rx )
    {
      Uint8 data = 0;
      if( acia->has_byte(&data) )
      {
        acia->regs[ACIA_STATUS] |= ASTF_RXDATA;
        if( acia->irqrx )
        {
          acia->regs[ACIA_STATUS] |= ASTF_IRQ;
          acia->oric->cpu.irq |= IRQF_ACIA;
        }
      }
    }
  }

  // update modem lines status
  if( acia->stat )
    acia->regs[ACIA_STATUS] = acia->stat(acia->regs[ACIA_STATUS]);
  else
    acia->regs[ACIA_STATUS] |= (ASTF_CARRIER|ASTF_DSR);
}

void acia_write( struct acia *acia, Uint16 addr, Uint8 data )
{
#if DEBUG_ACIA
  dbg_printf( "ACIA write: (%04X) %02X to %s", acia->oric->cpu.pc-1, data, writereg_names[addr&3] );
#endif
  switch( addr & 3 )
  {
    case ACIA_RXDATA:      // Data for TX
      acia->regs[ACIA_TXDATA] = data & acia->bitmask;
      acia->regs[ACIA_STATUS] &= ~(ASTF_TXEMPTY);
      break;

    case ACIA_STATUS:      // Reset
      acia->regs[ACIA_COMMAND] &= ACOMF_PARITY;
      acia->regs[ACIA_COMMAND] |= ACOMF_IRQDIS;
      acia->regs[ACIA_STATUS]  &= ~(ASTF_OVRUNERR);
      acia_update_params( acia );
      break;

    case ACIA_COMMAND:
      acia->regs[ACIA_COMMAND] = data;
      acia_update_params( acia );

#if DEBUG_ACIA
      dbg_printf( "  Parity  = %s", parity_modes[data>>5] );
      dbg_printf( "  RX Mode = %s", (data&ACOMF_ECHO) ? "ECHO" : "NORMAL" );
      dbg_printf( "  TXCon   = %s", txcon_modes[(data&ACOMF_TXCON)>>2] );
      dbg_printf( "  IRQ     = %s", (data&ACOMF_IRQDIS) ? "OFF" : "ON" );
      dbg_printf( "  DTR     = %s", (data&ACOMF_DTR) ? "ON" : "OFF" );
#endif
      break;

    case ACIA_CONTROL:
      acia->regs[ACIA_CONTROL] = data;
      acia_update_params( acia );

#if DEBUG_ACIA
      dbg_printf( "  Baud     = %s", baud_rates[data&ACONF_BAUD] );
      dbg_printf( "  RXClock  = %s", (data&ACONF_SRC) ? "INT" : "EXT" );
      dbg_printf( "  Bits     = %s", bit_nums[(data&ACONF_WLEN)>>5] );
      dbg_printf( "  Stopbits = %d", (data&ACONF_STOPB) ? 2 : 1 );
#endif
      break;
  }
}

Uint8 acia_read( struct acia *acia, Uint16 addr )
{
  Uint8 data = acia->regs[addr&3];
  
  switch( addr & 3 )
  {
    case ACIA_RXDATA:
#if DEBUG_ACIA
      dbg_printf( "ACIA read: (%04X) from %s (=%02X)", acia->oric->cpu.pc-1, readreg_names[addr&3], data );
#endif
      acia->get_byte( &data );
      data &= acia->bitmask;
      acia->regs[ACIA_RXDATA] = data;
      acia->regs[ACIA_STATUS] &= ~(ASTF_PARITYERR|ASTF_FRAMEERR|ASTF_OVRUNERR);
      acia->regs[ACIA_STATUS] &= ~(ASTF_RXDATA);
      break;

    case ACIA_STATUS:
#if DEBUG_ACIA_STATUS
      dbg_printf( "ACIA read: (%04X) from %s (=%02X)", acia->oric->cpu.pc-1, readreg_names[addr&3], data );
#endif
      // clear IRQ flag
      acia->regs[ACIA_STATUS] &= ~(ASTF_IRQ);
      acia->oric->cpu.irq &= ~(IRQF_ACIA);
      break;

    default:
#if DEBUG_ACIA
      dbg_printf( "ACIA read: (%04X) from %s (=%02X)", acia->oric->cpu.pc-1, readreg_names[addr&3], data );
#endif
      break;
  }

  return data;
}

// none - fake back-end, no api function will be called
SDL_bool acia_init_none( struct acia *acia )
{
  acia->done = NULL;
  acia->stat = NULL;
  acia->has_byte = NULL;
  acia->get_byte = NULL;
  acia->put_byte = NULL;
  
  acia->oric->aciabackend = ACIA_TYPE_NONE;
  return SDL_TRUE;
}
