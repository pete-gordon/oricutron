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
**  6551 ACIA emulation
*/

// Registers
enum
{
  ACIA_RXDATA=0,
  ACIA_STATUS,
  ACIA_COMMAND,
  ACIA_CONTROL,
  ACIA_TXDATA,
  ACIA_LAST
};

// Command register bits
#define ACOMB_DTR    0
#define ACOMF_DTR    (1<<ACOMB_DTR)
#define ACOMB_IRQDIS 1
#define ACOMF_IRQDIS (1<<ACOMB_IRQDIS)
#define ACOMF_TXCON 0x0c
#define ACOMB_ECHO   2
#define ACOMF_ECHO   (1<<ACOMB_ECHO)
#define ACOMF_PARITY 0xe0

// Control register bits
#define ACONF_BAUD   0x0f
#define ACONB_SRC    4
#define ACONF_SRC    (1<<ACONB_SRC)
#define ACONF_WLEN   0x60
#define ACONB_STOPB  7
#define ACONF_STOPB  (1<<ACONB_STOPB)

// Status register bits
#define ASTB_PARITYERR 0
#define ASTF_PARITYERR (1<<ASTB_PARITYERR)
#define ASTB_FRAMEERR  1
#define ASTF_FRAMEERR  (1<<ASTB_FRAMEERR)
#define ASTB_OVRUNERR  2
#define ASTF_OVRUNERR  (1<<ASTB_OVRUNERR)
#define ASTB_RXDATA    3
#define ASTF_RXDATA    (1<<ASTB_RECVDATA)
#define ASTB_TXEMPTY   4
#define ASTF_TXEMPTY   (1<<ASTB_TXEMPTY)
#define ASTB_CARRIER   5
#define ASTF_CARRIER   (1<<ASTB_CARRIER)
#define ASTB_DSR       6
#define ASTF_DSR       (1<<ASTB_DSR)
#define ASTB_IRQ       7
#define ASTF_IRQ       (1<<ASTB_IRQ)

struct acia
{
  struct machine *oric;
  Uint8 regs[ACIA_LAST];
};

void acia_init( struct acia *acia, struct machine *oric );
void acia_clock( struct acia *acia, unsigned int cycles );
void acia_write( struct acia *acia, Uint16 addr, Uint8 data );
Uint8 acia_read( struct acia *acia, Uint16 addr );
