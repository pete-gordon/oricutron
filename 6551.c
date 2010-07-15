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

#define DEBUG_ACIA 1

#if DEBUG_ACIA
static char *writereg_names[] = { "TXDATA", "RESET", "COMMAND", "CONTROL" };
static char *readreg_names[]  = { "RXDATA", "STATUS", "COMMAND", "CONTROL" };
static char *parity_modes[]   = { "NONE", "ODD", "NONE", "EVEN", "NONE", "MARK", "NONE", "SPACE" };
static char *txcon_modes[]    = { "TXIRQ=0,RLEV=H", "TXIRQ=1,RLEV=L", "RXIRQ=0,RLEV=L", "RXIRQ=0,RLEV=L,TXBRK" };
static char *baud_rates[]     = { "EXT", "50", "75", "109.92", "134.58", "150", "300", "600", "1200", "1800", "2400", "3600", "4800", "7200", "9600", "19200" };
static char *bit_nums[]       = { "8", "7", "6", "5" };
#endif

void acia_init( struct acia *acia, struct machine *oric )
{
  acia->oric = oric;
  acia->regs[ACIA_RXDATA]    = 0;
  acia->regs[ACIA_TXDATA]    = 0;
  acia->regs[ACIA_STATUS]  = ASTF_TXEMPTY;
  acia->regs[ACIA_COMMAND] = ACOMF_IRQDIS;
  acia->regs[ACIA_CONTROL] = 0;
}

void acia_clock( struct acia *acia, unsigned int cycles )
{
}

void acia_write( struct acia *acia, Uint16 addr, Uint8 data )
{
#if DEBUG_ACIA
  dbg_printf( "ACIA write: (%04X) %02X to %s", acia->oric->cpu.pc-1, data, writereg_names[addr&3] );
#endif
  switch( addr & 3 )
  {
    case ACIA_RXDATA:      // Data for TX
      acia->regs[ACIA_TXDATA] = data;
      acia->regs[ACIA_STATUS] &= ~(ASTF_TXEMPTY);
      break;
    
    case ACIA_STATUS:      // Reset
     acia->regs[ACIA_COMMAND] &= ACOMF_PARITY;
      acia->regs[ACIA_COMMAND] |= ACOMF_IRQDIS;
      acia->regs[ACIA_STATUS]  &= ~(ASTF_OVRUNERR);
      break;
    
    case ACIA_COMMAND:
      acia->regs[ACIA_COMMAND] = data;
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
#if DEBUG_ACIA
  dbg_printf( "ACIA read: (%04X) from %s (=%02X)", acia->oric->cpu.pc-1, readreg_names[addr&3], acia->regs[addr&3] );
#endif
  return acia->regs[addr&3];
}
