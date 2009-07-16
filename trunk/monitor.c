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
**  Monitor/Debugger
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include <SDL/SDL.h>

#include "6502.h"
#include "via.h"
#include "8912.h"
#include "gui.h"
#include "disk.h"
#include "machine.h"
#include "monitor.h"

extern struct textzone *tz[];
extern char vsptmp[];

static char distmp[40];
static char ibuf[60], lastcmd;
static char history[10][60];
static int ilen, cursx, histu=0, histp=-1;
static unsigned short mon_addr;
static unsigned short mw_addr;
static int mw_mode=0, mw_koffs=0;
static char mw_ibuf[8];
static SDL_bool kshifted = SDL_FALSE;

enum
{
  MSHOW_VIA=0,
  MSHOW_AY,
  MSHOW_DISK,
  MSHOW_LAST
};

enum
{
  CSHOW_CONSOLE=0,
  CSHOW_DEBUG,
  CSHOW_MWATCH,
  CSHOW_LAST
};

static int mshow, cshow;

enum
{
  REG_A=0,
  REG_X,
  REG_Y,
  REG_PC,
  REG_SP,
  REG_VIA_PCR,
  REG_VIA_ACR,
  REG_VIA_SR,
  REG_VIA_IFR,
  REG_VIA_IER,
  REG_VIA_IRA,
  REG_VIA_ORA,
  REG_VIA_DDRA,
  REG_VIA_IRB,
  REG_VIA_ORB,
  REG_VIA_DDRB,
  REG_VIA_T1L,
  REG_VIA_T1C,
  REG_VIA_T2L,
  REG_VIA_T2C,
  REG_VIA_CA1,
  REG_VIA_CA2,
  REG_VIA_CB1,
  REG_VIA_CB2
};

enum
{
  AM_IMP=0,
  AM_IMM,
  AM_ZP,
  AM_ZPX,
  AM_ZPY,
  AM_ABS,
  AM_ABX,
  AM_ABY,
  AM_ZIX,
  AM_ZIY,
  AM_REL,
  AM_IND
};

struct disinf
{
  char *name;
  int amode;
};

SDL_bool isws( char c )
{
  if( ( c == 9 ) || ( c == 32 ) ) return SDL_TRUE;
  return SDL_FALSE;
}

SDL_bool isbin( char c )
{
  if( ( c >= '0' ) && ( c <= '1' ) ) return SDL_TRUE;
  return SDL_FALSE;
}

SDL_bool isnum( char c )
{
  if( ( c >= '0' ) && ( c <= '9' ) ) return SDL_TRUE;
  return SDL_FALSE;
}

SDL_bool ishex( char c )
{
  if( isnum( c ) ) return SDL_TRUE;
  if( ( c >= 'a' ) && ( c <= 'f' ) ) return SDL_TRUE;
  if( ( c >= 'A' ) && ( c <= 'F' ) ) return SDL_TRUE;
  return SDL_FALSE;
}

int hexit( char c )
{
  if( isnum( c ) ) return c-'0';
  if( ( c >= 'a' ) && ( c <= 'f' ) ) return c-('a'-10);
  if( ( c >= 'A' ) && ( c <= 'F' ) ) return c-('A'-10);
  return -1;
}

void dbg_scroll( void )
{
  int x, y, s, d;
  struct textzone *ptz = tz[TZ_DEBUG];

  s = ptz->w*2+1;
  d = ptz->w+1;
  for( y=0; y<18; y++ )
  {
    for( x=0; x<48; x++ )
    {
      ptz->tx[d] = ptz->tx[s];
      ptz->fc[d] = ptz->fc[s];
      ptz->bc[d++] = ptz->bc[s++];
    }
    d += 2;
    s += 2;
  }

  for( x=0; x<48; x++ )
  {
    ptz->tx[d] = ' ';
    ptz->fc[d] = 2;
    ptz->bc[d++] = 3;
  }
}

void dbg_printf( char *fmt, ... )
{
  va_list ap;

  dbg_scroll();

  va_start( ap, fmt );
  if( vsnprintf( vsptmp, VSPTMPSIZE, fmt, ap ) != -1 )
  {
    vsptmp[VSPTMPSIZE-1] = 0;
    tzstrpos( tz[TZ_DEBUG], 1, 19, vsptmp );
  }
  va_end( ap );
}


static struct disinf distab[] = { { "BRK", AM_IMP },  // 00
                                  { "ORA", AM_ZIX },  // 01
                                  { "???", AM_IMP },  // 02
                                  { "???", AM_IMP },  // 03
                                  { "???", AM_IMP },  // 04
                                  { "ORA", AM_ZP  },  // 05
                                  { "ASL", AM_ZP  },  // 06
                                  { "???", AM_IMP },  // 07
                                  { "PHP", AM_IMP },  // 08
                                  { "ORA", AM_IMM },  // 09
                                  { "ASL", AM_IMP },  // 0A
                                  { "???", AM_IMP },  // 0B
                                  { "???", AM_IMP },  // 0C
                                  { "ORA", AM_ABS },  // 0D
                                  { "ASL", AM_ABS },  // 0E
                                  { "???", AM_IMP },  // 0F
                                  { "BPL", AM_REL },  // 10
                                  { "ORA", AM_ZIY },  // 11
                                  { "???", AM_IMP },  // 12
                                  { "???", AM_IMP },  // 13
                                  { "???", AM_IMP },  // 14
                                  { "ORA", AM_ZPX },  // 15
                                  { "ASL", AM_ZPX },  // 16
                                  { "???", AM_IMP },  // 17
                                  { "CLC", AM_IMP },  // 18
                                  { "ORA", AM_ABY },  // 19
                                  { "???", AM_IMP },  // 1A
                                  { "???", AM_IMP },  // 1B
                                  { "???", AM_IMP },  // 1C
                                  { "ORA", AM_ABX },  // 1D
                                  { "ASL", AM_ABX },  // 1E
                                  { "???", AM_IMP },  // 1F
                                  { "JSR", AM_ABS },  // 20
                                  { "AND", AM_ZIX },  // 21
                                  { "???", AM_IMP },  // 22
                                  { "???", AM_IMP },  // 23
                                  { "BIT", AM_ZP  },  // 24
                                  { "AND", AM_ZP  },  // 25
                                  { "ROL", AM_ZP  },  // 26
                                  { "???", AM_IMP },  // 27
                                  { "PLP", AM_IMP },  // 28
                                  { "AND", AM_IMM },  // 29
                                  { "ROL", AM_IMP },  // 2A
                                  { "???", AM_IMP },  // 2B
                                  { "BIT", AM_ABS },  // 2C
                                  { "AND", AM_ABS },  // 2D
                                  { "ROL", AM_ABS },  // 2E
                                  { "???", AM_IMP },  // 2F
                                  { "BMI", AM_REL },  // 30
                                  { "AND", AM_ZIY },  // 31
                                  { "???", AM_IMP },  // 32
                                  { "???", AM_IMP },  // 33
                                  { "???", AM_IMP },  // 34
                                  { "AND", AM_ZPX },  // 35
                                  { "ROL", AM_ZPX },  // 36
                                  { "???", AM_IMP },  // 37
                                  { "SEC", AM_IMP },  // 38
                                  { "AND", AM_ABY },  // 39
                                  { "???", AM_IMP },  // 3A
                                  { "???", AM_IMP },  // 3B
                                  { "???", AM_IMP },  // 3C
                                  { "AND", AM_ABX },  // 3D
                                  { "ROL", AM_ABX },  // 3E
                                  { "???", AM_IMP },  // 3F
                                  { "RTI", AM_IMP },  // 40
                                  { "EOR", AM_ZIX },  // 41
                                  { "???", AM_IMP },  // 42
                                  { "???", AM_IMP },  // 43
                                  { "???", AM_IMP },  // 44
                                  { "EOR", AM_ZP  },  // 45
                                  { "LSR", AM_ZP  },  // 46
                                  { "???", AM_IMP },  // 47
                                  { "PHA", AM_IMP },  // 48
                                  { "EOR", AM_IMM },  // 49
                                  { "LSR", AM_IMP },  // 4A
                                  { "???", AM_IMP },  // 4B
                                  { "JMP", AM_ABS },  // 4C
                                  { "EOR", AM_ABS },  // 4D
                                  { "LSR", AM_ABS },  // 4E
                                  { "???", AM_IMP },  // 4F
                                  { "BVC", AM_REL },  // 50
                                  { "EOR", AM_ZIY },  // 51
                                  { "???", AM_IMP },  // 52
                                  { "???", AM_IMP },  // 53
                                  { "???", AM_IMP },  // 54
                                  { "EOR", AM_ZPX },  // 55
                                  { "LSR", AM_ZPX },  // 56
                                  { "???", AM_IMP },  // 57
                                  { "CLI", AM_IMP },  // 58
                                  { "EOR", AM_ABY },  // 59
                                  { "???", AM_IMP },  // 5A
                                  { "???", AM_IMP },  // 5B
                                  { "???", AM_IMP },  // 5C
                                  { "EOR", AM_ABX },  // 5D
                                  { "LSR", AM_ABX },  // 5E
                                  { "???", AM_IMP },  // 5F
                                  { "RTS", AM_IMP },  // 60
                                  { "ADC", AM_ZIX },  // 61
                                  { "???", AM_IMP },  // 62
                                  { "???", AM_IMP },  // 63
                                  { "???", AM_IMP },  // 64
                                  { "ADC", AM_ZP  },  // 65
                                  { "ROR", AM_ZP  },  // 66
                                  { "???", AM_IMP },  // 67
                                  { "PLA", AM_IMP },  // 68
                                  { "ADC", AM_IMM },  // 69
                                  { "ROR", AM_IMP },  // 6A
                                  { "???", AM_IMP },  // 6B
                                  { "JMP", AM_IND },  // 6C
                                  { "ADC", AM_ABS },  // 6D
                                  { "ROR", AM_ABS },  // 6E
                                  { "???", AM_IMP },  // 6F
                                  { "BVS", AM_REL },  // 70
                                  { "ADC", AM_ZIY },  // 71
                                  { "???", AM_IMP },  // 72
                                  { "???", AM_IMP },  // 73
                                  { "???", AM_IMP },  // 74
                                  { "ADC", AM_ZPX },  // 75
                                  { "ROR", AM_ZPX },  // 76
                                  { "???", AM_IMP },  // 77
                                  { "SEI", AM_IMP },  // 78
                                  { "ADC", AM_ABY },  // 79
                                  { "???", AM_IMP },  // 7A
                                  { "???", AM_IMP },  // 7B
                                  { "???", AM_IMP },  // 7C
                                  { "ADC", AM_ABX },  // 7D
                                  { "ROR", AM_ABX },  // 7E
                                  { "???", AM_IMP },  // 7F
                                  { "???", AM_IMP },  // 80
                                  { "STA", AM_ZIX },  // 81
                                  { "???", AM_IMP },  // 82
                                  { "???", AM_IMP },  // 83
                                  { "STY", AM_ZP  },  // 84
                                  { "STA", AM_ZP  },  // 85
                                  { "STX", AM_ZP  },  // 86
                                  { "???", AM_IMP },  // 87
                                  { "DEY", AM_IMP },  // 88
                                  { "???", AM_IMP },  // 89
                                  { "TXA", AM_IMP },  // 8A
                                  { "???", AM_IMP },  // 8B
                                  { "STY", AM_ABS },  // 8C
                                  { "STA", AM_ABS },  // 8D
                                  { "STX", AM_ABS },  // 8E
                                  { "???", AM_IMP },  // 8F
                                  { "BCC", AM_REL },  // 90
                                  { "STA", AM_ZIY },  // 91
                                  { "???", AM_IMP },  // 92
                                  { "???", AM_IMP },  // 93
                                  { "STY", AM_ZPX },  // 94
                                  { "STA", AM_ZPX },  // 95
                                  { "STX", AM_ZPY },  // 96
                                  { "???", AM_IMP },  // 97
                                  { "TYA", AM_IMP },  // 98
                                  { "STA", AM_ABY },  // 99
                                  { "TXS", AM_IMP },  // 9A
                                  { "???", AM_IMP },  // 9B
                                  { "???", AM_IMP },  // 9C
                                  { "STA", AM_ABX },  // 9D
                                  { "???", AM_IMP },  // 9E
                                  { "???", AM_IMP },  // 9F
                                  { "LDY", AM_IMM },  // A0
                                  { "LDA", AM_ZIX },  // A1
                                  { "LDX", AM_IMM },  // A2
                                  { "???", AM_IMP },  // A3
                                  { "LDY", AM_ZP  },  // A4
                                  { "LDA", AM_ZP  },  // A5
                                  { "LDX", AM_ZP  },  // A6
                                  { "???", AM_IMP },  // A7
                                  { "TAY", AM_IMP },  // A8
                                  { "LDA", AM_IMM },  // A9
                                  { "TAX", AM_IMP },  // AA
                                  { "???", AM_IMP },  // AB
                                  { "LDY", AM_ABS },  // AC
                                  { "LDA", AM_ABS },  // AD
                                  { "LDX", AM_ABS },  // AE
                                  { "???", AM_IMP },  // AF
                                  { "BCS", AM_REL },  // B0
                                  { "LDA", AM_ZIY },  // B1
                                  { "???", AM_IMP },  // B2
                                  { "???", AM_IMP },  // B3
                                  { "LDY", AM_ZPX },  // B4
                                  { "LDA", AM_ZPX },  // B5
                                  { "LDX", AM_ZPY },  // B6
                                  { "???", AM_IMP },  // B7
                                  { "CLV", AM_IMP },  // B8
                                  { "LDA", AM_ABY },  // B9
                                  { "TSX", AM_IMP },  // BA
                                  { "???", AM_IMP },  // BB
                                  { "LDY", AM_ABX },  // BC
                                  { "LDA", AM_ABX },  // BD
                                  { "LDX", AM_ABY },  // BE
                                  { "???", AM_IMP },  // BF
                                  { "CPY", AM_IMM },  // C0
                                  { "CMP", AM_ZIX },  // C1
                                  { "???", AM_IMP },  // C2
                                  { "???", AM_IMP },  // C3
                                  { "CPY", AM_ZP  },  // C4
                                  { "CMP", AM_ZP  },  // C5
                                  { "DEC", AM_ZP  },  // C6
                                  { "???", AM_IMP },  // C7
                                  { "INY", AM_IMP },  // C8
                                  { "CMP", AM_IMM },  // C9
                                  { "DEX", AM_IMP },  // CA
                                  { "???", AM_IMP },  // CB
                                  { "CPY", AM_ABS },  // CC
                                  { "CMP", AM_ABS },  // CD
                                  { "DEC", AM_ABS },  // CE
                                  { "???", AM_IMP },  // CF
                                  { "BNE", AM_REL },  // D0
                                  { "CMP", AM_ZIY },  // D1
                                  { "???", AM_IMP },  // D2
                                  { "???", AM_IMP },  // D3
                                  { "???", AM_IMP },  // D4
                                  { "CMP", AM_ZPX },  // D5
                                  { "DEC", AM_ZPX },  // D6
                                  { "???", AM_IMP },  // D7
                                  { "CLD", AM_IMP },  // D8
                                  { "CMP", AM_ABY },  // D9
                                  { "???", AM_IMP },  // DA
                                  { "???", AM_IMP },  // DB
                                  { "???", AM_IMP },  // DC
                                  { "CMP", AM_ABX },  // DD
                                  { "DEC", AM_ABX },  // DE
                                  { "???", AM_IMP },  // DF
                                  { "CPX", AM_IMM },  // E0
                                  { "SBC", AM_ZIX },  // E1
                                  { "???", AM_IMP },  // E2
                                  { "???", AM_IMP },  // E3
                                  { "CPX", AM_ZP  },  // E4
                                  { "SBC", AM_ZP  },  // E5
                                  { "INC", AM_ZP  },  // E6
                                  { "???", AM_IMP },  // E7
                                  { "INX", AM_IMP },  // E8
                                  { "SBC", AM_IMM },  // E9
                                  { "NOP", AM_IMP },  // EA
                                  { "???", AM_IMP },  // EB
                                  { "CPX", AM_ABS },  // EC
                                  { "SBC", AM_ABS },  // ED
                                  { "INC", AM_ABS },  // EE
                                  { "???", AM_IMP },  // EF
                                  { "BEQ", AM_REL },  // F0
                                  { "SBC", AM_ZIY },  // F1
                                  { "???", AM_IMP },  // F2
                                  { "???", AM_IMP },  // F3
                                  { "???", AM_IMP },  // F4
                                  { "SBC", AM_ZPX },  // F5
                                  { "INC", AM_ZPX },  // F6
                                  { "???", AM_IMP },  // F7
                                  { "SED", AM_IMP },  // F8
                                  { "SBC", AM_ABY },  // F9
                                  { "???", AM_IMP },  // FA
                                  { "???", AM_IMP },  // FB
                                  { "???", AM_IMP },  // FC
                                  { "SBC", AM_ABX },  // FD
                                  { "INC", AM_ABX },  // FE
                                  { "???", AM_IMP } };// FF

char *mon_disassemble( struct machine *oric, unsigned short *paddr )
{
  unsigned short iaddr;
  unsigned char op, a1, a2;
  int i;

  iaddr = *paddr;
  op = oric->cpu.read( &oric->cpu, (*paddr)++ );
  switch( distab[op].amode )
  {
    case AM_IMP:
      sprintf( distmp, "  %04X  %02X        %s", iaddr, op, distab[op].name );
      break;
    
    case AM_IMM:
      a1 = oric->cpu.read( &oric->cpu, (*paddr)++ );
      sprintf( distmp, "  %04X  %02X %02X     %s #$%02X", iaddr, op, a1, distab[op].name, a1 );
      break;

    case AM_ZP:
    case AM_ZPX:
    case AM_ZPY:
      a1 = oric->cpu.read( &oric->cpu, (*paddr)++ );
      sprintf( distmp, "  %04X  %02X %02X     %s $%02X", iaddr, op, a1, distab[op].name, a1 );
      if( distab[op].amode == AM_ZP ) break;
      strcat( distmp, distab[op].amode == AM_ZPX ? ",X" : ",Y" );
      break;
    
    case AM_ABS:
    case AM_ABX:
    case AM_ABY:
      a1 = oric->cpu.read( &oric->cpu, (*paddr)++ );
      a2 = oric->cpu.read( &oric->cpu, (*paddr)++ );
      sprintf( distmp, "  %04X  %02X %02X %02X  %s $%02X%02X", iaddr, op, a1, a2, distab[op].name, a2, a1 );
      if( distab[op].amode == AM_ABS ) break;
      strcat( distmp, distab[op].amode == AM_ABX ? ",X" : ",Y" );
      break;

    case AM_ZIX:
      a1 = oric->cpu.read( &oric->cpu, (*paddr)++ );
      sprintf( distmp, "  %04X  %02X %02X     %s ($%02X,X)", iaddr, op, a1, distab[op].name, a1 );
      break;

    case AM_ZIY:
      a1 = oric->cpu.read( &oric->cpu, (*paddr)++ );
      sprintf( distmp, "  %04X  %02X %02X     %s ($%02X),Y", iaddr, op, a1, distab[op].name, a1 );
      break;

    case AM_REL:
      a1 = oric->cpu.read( &oric->cpu, (*paddr)++ );
      sprintf( distmp, "  %04X  %02X %02X     %s $%04X", iaddr, op, a1, distab[op].name, ((*paddr)+((signed char)a1))&0xffff );
      break;

    case AM_IND:
      a1 = oric->cpu.read( &oric->cpu, (*paddr)++ );
      a2 = oric->cpu.read( &oric->cpu, (*paddr)++ );
      sprintf( distmp, "  %04X  %02X %02X %02X  %s ($%02X%02X)", iaddr, op, a1, a2, distab[op].name, a2, a1 );
      break;
    
    default:
      strcpy( distmp, "  WTF?" );
      break;
  }

  oric->cpu.anybp = SDL_FALSE;
  for( i=0; i<16; i++ )
  {
    if( oric->cpu.breakpoints[i] != -1 )
    {
      oric->cpu.anybp = SDL_TRUE;
      if( iaddr == oric->cpu.breakpoints[i] )
        distmp[0] = '*';
    }
  }

  if( iaddr == oric->cpu.pc )
    distmp[1] = '>';

  return distmp;
}

void mon_update_regs( struct machine *oric )
{
  int i;
  unsigned short addr;

  tzprintfpos( tz[TZ_REGS], 2, 2, "PC=%04X  SP=01%02X    A=%02X  X=%02X  Y=%02X",
    oric->cpu.pc,
    oric->cpu.sp,
    oric->cpu.a,
    oric->cpu.x,
    oric->cpu.y );
#if CYCLECOUNT
  tzprintfpos( tz[TZ_REGS], 2, 4, "CY=%08d", oric->cpu.cycles );
  tzprintfpos( tz[TZ_REGS], 2, 5, "FM=%06d RS=%03d", oric->frames, oric->vid_raster );
#endif
  tzstrpos( tz[TZ_REGS], 30, 4, "NV-BDIZC" );
  tzprintfpos( tz[TZ_REGS], 30, 5, "%01X%01X1%01X%01X%01X%01X%01X",
    oric->cpu.f_n,
    oric->cpu.f_v,
    oric->cpu.f_b,
    oric->cpu.f_d,
    oric->cpu.f_i,
    oric->cpu.f_z,
    oric->cpu.f_c );

  addr = oric->cpu.pc;
  for( i=0; i<10; i++ )
  {
    tzsetcol( tz[TZ_REGS], (addr==oric->cpu.pc) ? 1 : 2, 3 );
    tzstrpos( tz[TZ_REGS], 23, 7+i, "        " );
    tzstrpos( tz[TZ_REGS],  2, 7+i, mon_disassemble( oric, &addr ) );
  }
}

void mon_update_via( struct machine *oric )
{
  tzprintfpos( tz[TZ_VIA], 2, 2, "PCR=%02X ACR=%02X SR=%02X",
    oric->via.pcr,
    oric->via.acr,
    oric->via.sr );
  tzprintfpos( tz[TZ_VIA], 2, 3, "IFR=%02X IER=%02X",
    oric->via.ifr,
    oric->via.ier );  
  tzprintfpos( tz[TZ_VIA], 2, 5, "A=%02X DDRA=%02X  B=%02X DDRB=%02X",
    (oric->via.ora&oric->via.ddra)|(oric->via.ira&(~oric->via.ddra)),
    oric->via.ddra,
    (oric->via.orb&oric->via.ddrb)|(oric->via.irb&(~oric->via.ddrb)),
    oric->via.ddrb );
  tzprintfpos( tz[TZ_VIA], 2, 6, "CA1=%01X CA2=%01X   CB1=%01X CB2=%01X",
    oric->via.ca1,
    oric->via.ca2,
    oric->via.cb1,
    oric->via.cb2 );
  tzprintfpos( tz[TZ_VIA], 2, 8, "T1L=%02X%02X T1C=%04X",
    oric->via.t1l_h,
    oric->via.t1l_l,
    oric->via.t1c );
  tzprintfpos( tz[TZ_VIA], 2, 9, "T2L=%02X%02X T2C=%04X",
    oric->via.t2l_h,
    oric->via.t2l_l,
    oric->via.t2c );

  tzprintfpos( tz[TZ_VIA], 2, 11, "TAPE OFFS = %07d", oric->tapeoffs );
  tzprintfpos( tz[TZ_VIA], 2, 12, "TAPE LEN  = %07d", oric->tapelen );
  tzprintfpos( tz[TZ_VIA], 2, 13, "COUNT     = %07d", oric->tapecount );
  tzprintfpos( tz[TZ_VIA], 2, 14, "BIT = %02X  DATA = %1X",
    (oric->tapebit+9)%10,
    oric->tapetime == TAPE_1_PULSE );
  tzprintfpos( tz[TZ_VIA], 2, 15, "MOTOR = %1X", oric->tapemotor );
}

void mon_update_ay( struct machine *oric )
{
  tzprintfpos( tz[TZ_AY], 2, 2, "BC1=%1X BDIR=%1X REG=%02X",
    (oric->ay.bmode & AYBMF_BC1) ? 1 : 0,
    (oric->ay.bmode & AYBMF_BDIR) ? 1 : 0,
    oric->ay.creg );
  tzprintfpos( tz[TZ_AY], 2, 4, "A:P=%04X A=%02X   N:PER=%02X",
    (oric->ay.regs[AY_CHA_PER_H]<<8)|(oric->ay.regs[AY_CHA_PER_L]),
    oric->ay.regs[AY_CHA_AMP],
    oric->ay.regs[AY_NOISE_PER] );
  tzprintfpos( tz[TZ_AY], 2, 5, "B:P=%04X A=%02X   E:PER=%04X",
    (oric->ay.regs[AY_CHB_PER_H]<<8)|(oric->ay.regs[AY_CHB_PER_L]),
    oric->ay.regs[AY_CHB_AMP],
    (oric->ay.regs[AY_ENV_PER_H]<<8)|(oric->ay.regs[AY_ENV_PER_L]) );
  tzprintfpos( tz[TZ_AY], 2, 6, "C:P=%04X A=%02X   E:CYC=%02X",
    (oric->ay.regs[AY_CHC_PER_H]<<8)|(oric->ay.regs[AY_CHC_PER_L]),
    oric->ay.regs[AY_CHC_AMP],
    oric->ay.regs[AY_ENV_CYCLE] );
  tzprintfpos( tz[TZ_AY], 2, 8, "STATUS=%02X",
    oric->ay.regs[AY_STATUS] );
  tzprintfpos( tz[TZ_AY], 2, 9, "PORT=%02X",
    oric->ay.regs[AY_PORT_A] );  
}

/*
  Uint8             r_status;
  Uint8             cmd;
  Uint8             r_track;
  Uint8             r_sector;
  Uint8             r_data;
  Uint8             c_drive;
  Uint8             c_side;
  Uint8             c_track;
  SDL_bool          last_step_in;
  void            (*setintrq)(void *);
  void            (*clrintrq)(void *);
  void             *intrqarg;
  void            (*setdrq)(void *);
  void            (*clrdrq)(void *);
  void             *drqarg;
  struct diskimage *disk[MAX_DRIVES];
  int               currentop;
  int               delayedint;
*/
void mon_update_disk( struct machine *oric )
{
  if( oric->drivetype == DRV_NONE )
    return;

  tzprintfpos( tz[TZ_DISK], 2, 2, "STATUS=%02X        ROMDIS=%1X", oric->wddisk.r_status, oric->romdis );
  tzprintfpos( tz[TZ_DISK], 2, 3, "DRIVE=%02X SIDE=%02X TRACK=%02X",
    oric->wddisk.c_drive,
    oric->wddisk.c_side,
    oric->wddisk.c_track );

  switch( oric->drivetype )
  {
    case DRV_MICRODISC:
      tzprintfpos( tz[TZ_DISK], 2, 5, "MDSTAT=%02X INTRQ=%1X DRQ=%1X",
        oric->md.status,
        oric->md.intrq!=0,
        oric->md.drq!=0 );
      break;
    
    case DRV_JASMIN:
      break;
  }
}

void mon_update_mwatch( struct machine *oric )
{
  unsigned short addr;
  int j,k,l;
  unsigned int v;

  addr = mw_addr;
  for( j=0; j<19; j++ )
  {
    sprintf( vsptmp, "%04X  ", addr );
    for( k=0; k<8; k++ )
    {
      sprintf( &vsptmp[128], "%02X ", oric->cpu.read( &oric->cpu, addr+k ) );
      strcat( vsptmp, &vsptmp[128] );
    }
    l = strlen( vsptmp );
    vsptmp[l++] = 32;
    vsptmp[l++] = '\'';
    for( k=0; k<8; k++ )
    {
      v = oric->cpu.read( &oric->cpu, addr++ );
      vsptmp[l++] = ((v>31)&&(v<128))?v:'.';
    }
    vsptmp[l++] = '\'';
    vsptmp[l++] = 0;
    tzstrpos( tz[TZ_MEMWATCH], 1, j+1, vsptmp );
  }

  if( mw_mode == 0 ) return;

  makebox( tz[TZ_MEMWATCH], 17, 8, 7, 3, 2, 3 );
  tzstrpos( tz[TZ_MEMWATCH], 18, 9, mw_ibuf );
}

void mon_render( struct machine *oric )
{
  video_show( oric );
  mon_update_regs( oric );
  
  switch( mshow )
  {
    case MSHOW_VIA:
      mon_update_via( oric );
      draw_textzone( tz[TZ_VIA] );
      break;
    
    case MSHOW_AY:
      mon_update_ay( oric );
      draw_textzone( tz[TZ_AY] );
      break;
    
    case MSHOW_DISK:
      mon_update_disk( oric );
      draw_textzone( tz[TZ_DISK] );
      break;
  }

  switch( cshow )
  {
    case CSHOW_CONSOLE:
      draw_textzone( tz[TZ_MONITOR] );
      break;
    
    case CSHOW_DEBUG:
      draw_textzone( tz[TZ_DEBUG] );
      break;

    case CSHOW_MWATCH:
      mon_update_mwatch( oric );
      draw_textzone( tz[TZ_MEMWATCH] );
      break;
}
  draw_textzone( tz[TZ_REGS] );
}

void mon_hide_curs( void )
{
  struct textzone *ptz = tz[TZ_MONITOR];
  ptz->fc[ptz->w*19+2+cursx] = 2;
  ptz->bc[ptz->w*19+2+cursx] = 3;
}

void mon_show_curs( void )
{
  struct textzone *ptz = tz[TZ_MONITOR];
  ptz->fc[ptz->w*19+2+cursx] = 3;
  ptz->bc[ptz->w*19+2+cursx] = 2;
}

void mon_scroll( SDL_bool above )
{
  int x, y, s, d, h;
  struct textzone *ptz = tz[TZ_MONITOR];

  h = above ? 17 : 18;

  s = ptz->w*2+1;
  d = ptz->w+1;
  for( y=0; y<h; y++ )
  {
    for( x=0; x<48; x++ )
    {
      ptz->tx[d] = ptz->tx[s];
      ptz->fc[d] = ptz->fc[s];
      ptz->bc[d++] = ptz->bc[s++];
    }
    d += 2;
    s += 2;
  }

  for( x=0; x<48; x++ )
  {
    ptz->tx[d] = ' ';
    ptz->fc[d] = 2;
    ptz->bc[d++] = 3;
  }
}

void mon_printf( char *fmt, ... )
{
  va_list ap;

  mon_scroll( SDL_FALSE );

  va_start( ap, fmt );
  if( vsnprintf( vsptmp, VSPTMPSIZE, fmt, ap ) != -1 )
  {
    vsptmp[VSPTMPSIZE-1] = 0;
    tzstrpos( tz[TZ_MONITOR], 1, 19, vsptmp );
  }
  va_end( ap );
}

void mon_printf_above( char *fmt, ... )
{
  va_list ap;

  mon_scroll( SDL_TRUE );

  va_start( ap, fmt );
  if( vsnprintf( vsptmp, VSPTMPSIZE, fmt, ap ) != -1 )
  {
    vsptmp[VSPTMPSIZE-1] = 0;
    tzstrpos( tz[TZ_MONITOR], 1, 18, vsptmp );
  }
  va_end( ap );
}

void mon_str( char *str )
{
  mon_scroll( SDL_FALSE );
  tzstrpos( tz[TZ_MONITOR], 1, 19, str );
}

void mon_str_above( char *str )
{
  mon_scroll( SDL_TRUE );
  tzstrpos( tz[TZ_MONITOR], 1, 18, str );
}

void mon_start_input( void )
{
  mon_scroll( SDL_FALSE );
  ibuf[0] = 0;
  ilen = 0;
  cursx = 0;
  tzsetcol( tz[TZ_MONITOR], 1, 3 );
  tzstrpos( tz[TZ_MONITOR], 1, 19, "]" ); 
}

void mon_init( struct machine *oric )
{
  mshow = MSHOW_VIA;
  cshow = CSHOW_CONSOLE;
  mon_start_input();
  mon_show_curs();
  mon_addr = oric->cpu.pc;
  lastcmd = 0;
}

int mon_getreg( char *buf, int *off, SDL_bool addrregs, SDL_bool nregs, SDL_bool viaregs )
{
  int i;

  i = *off;

  while( isws( buf[i] ) ) i++;

  if( viaregs )
  {
    if( strncasecmp( &buf[i], "vpcr", 4 ) == 0 )
    {
      i+=4;
      *off = i;
      return REG_VIA_PCR;
    }

    if( strncasecmp( &buf[i], "vacr", 4 ) == 0 )
    {
      i+=4;
      *off = i;
      return REG_VIA_ACR;
    }

    if( strncasecmp( &buf[i], "vsr", 3 ) == 0 )
    {
      i+=3;
      *off = i;
      return REG_VIA_SR;
    }

    if( strncasecmp( &buf[i], "vifr", 4 ) == 0 )
    {
      i+=4;
      *off = i;
      return REG_VIA_IFR;
    }

    if( strncasecmp( &buf[i], "vier", 4 ) == 0 )
    {
      i+=4;
      *off = i;
      return REG_VIA_IER;
    }

    if( strncasecmp( &buf[i], "vira", 4 ) == 0 )
    {
      i+=4;
      *off = i;
      return REG_VIA_IRA;
    }

    if( strncasecmp( &buf[i], "vora", 4 ) == 0 )
    {
      i+=4;
      *off = i;
      return REG_VIA_ORA;
    }

    if( strncasecmp( &buf[i], "vddra", 5 ) == 0 )
    {
      i+=5;
      *off = i;
      return REG_VIA_DDRA;
    }

    if( strncasecmp( &buf[i], "virb", 4 ) == 0 )
    {
      i+=4;
      *off = i;
      return REG_VIA_IRB;
    }

    if( strncasecmp( &buf[i], "vorb", 4 ) == 0 )
    {
      i+=4;
      *off = i;
      return REG_VIA_ORB;
    }

    if( strncasecmp( &buf[i], "vddrb", 5 ) == 0 )
    {
      i+=5;
      *off = i;
      return REG_VIA_DDRB;
    }

    if( strncasecmp( &buf[i], "vt1l", 4 ) == 0 )
    {
      i+=4;
      *off = i;
      return REG_VIA_T1L;
    }

    if( strncasecmp( &buf[i], "vt1c", 4 ) == 0 )
    {
      i+=4;
      *off = i;
      return REG_VIA_T1C;
    }

    if( strncasecmp( &buf[i], "vt2l", 4 ) == 0 )
    {
      i+=4;
      *off = i;
      return REG_VIA_T2L;
    }

    if( strncasecmp( &buf[i], "vt2c", 4 ) == 0 )
    {
      i+=4;
      *off = i;
      return REG_VIA_T2C;
    }

    if( strncasecmp( &buf[i], "vca1", 4 ) == 0 )
    {
      i+=4;
      *off = i;
      return REG_VIA_CA1;
    }

    if( strncasecmp( &buf[i], "vca2", 4 ) == 0 )
    {
      i+=4;
      *off = i;
      return REG_VIA_CA2;
    }

    if( strncasecmp( &buf[i], "vcb1", 4 ) == 0 )
    {
      i+=4;
      *off = i;
      return REG_VIA_CB1;
    }

    if( strncasecmp( &buf[i], "vcb2", 4 ) == 0 )
    {
      i+=4;
      *off = i;
      return REG_VIA_CA2;
    }
  }

  if( addrregs )
  {
    if( strncasecmp( &buf[i], "pc", 2 ) == 0 )
    {
      i+=2;
      *off = i;
      return REG_PC;
    }

    if( strncasecmp( &buf[i], "sp", 2 ) == 0 )
    {
      i+=2;
      *off = i;
      return REG_SP;
    }
  }

  if( nregs )
  {
    switch( buf[i] )
    {
      case 'a':
      case 'A':
        i++;
        *off = i;
        return REG_A;

      case 'x':
      case 'X':
        i++;
        *off = i;
        return REG_X;

      case 'y':
      case 'Y':
        i++;
        *off = i;
        return REG_Y;
    }
  }

  return -1;
}

SDL_bool mon_getnum( struct machine *oric, unsigned int *num, char *buf, int *off, SDL_bool addrregs, SDL_bool nregs, SDL_bool viaregs )
{
  int i, j;
  unsigned int v;

  i = *off;
  while( isws( buf[i] ) ) i++;

  j = mon_getreg( buf, off, addrregs, nregs, viaregs );
  if( j != -1 )
  {
    switch( j )
    {
      case REG_A:
        *num = oric->cpu.a;
        return SDL_TRUE;
      case REG_X:
        *num = oric->cpu.x;
        return SDL_TRUE;
      case REG_Y:
        *num = oric->cpu.y;
        return SDL_TRUE;
      case REG_PC:
        *num = oric->cpu.pc;
        return SDL_TRUE;
      case REG_SP:
        *num = oric->cpu.sp|0x100;
        return SDL_TRUE;
      case REG_VIA_PCR:
        *num = oric->via.pcr;
        return SDL_TRUE;
      case REG_VIA_ACR:
        *num = oric->via.acr;
        return SDL_TRUE;
      case REG_VIA_SR:
        *num = oric->via.sr;
        return SDL_TRUE;
      case REG_VIA_IFR:
        *num = oric->via.ifr;
        return SDL_TRUE;
      case REG_VIA_IER:
        *num = oric->via.ier;
        return SDL_TRUE;
      case REG_VIA_IRA:
        *num = oric->via.ira;
        return SDL_TRUE;
      case REG_VIA_ORA:
        *num = oric->via.ora;
        return SDL_TRUE;
      case REG_VIA_DDRA:
        *num = oric->via.ddra;
        return SDL_TRUE;
      case REG_VIA_IRB:
        *num = oric->via.irb;
        return SDL_TRUE;
      case REG_VIA_ORB:
        *num = oric->via.orb;
        return SDL_TRUE;
      case REG_VIA_DDRB:
        *num = oric->via.ddrb;
        return SDL_TRUE;
      case REG_VIA_T1L:
        *num = (oric->via.t1l_h<<8)|oric->via.t1l_l;
        return SDL_TRUE;
      case REG_VIA_T1C:
        *num = oric->via.t1c;
        return SDL_TRUE;
      case REG_VIA_T2L:
        *num = (oric->via.t2l_h<<8)|oric->via.t2l_l;
        return SDL_TRUE;
      case REG_VIA_T2C:
        *num = oric->via.t2c;
        return SDL_TRUE;
      case REG_VIA_CA1:
        *num = oric->via.ca1;
        return SDL_TRUE;
      case REG_VIA_CA2:
        *num = oric->via.ca2;
        return SDL_TRUE;
      case REG_VIA_CB1:
        *num = oric->via.cb1;
        return SDL_TRUE;
      case REG_VIA_CB2:
        *num = oric->via.cb2;
        return SDL_TRUE;
    }
  }

  if( buf[i] == '%' )
  {
    // Binary
    i++;
    if( !isbin( buf[i] ) ) return SDL_FALSE;

    v = 0;
    while( isbin( buf[i] ) )
    {
      v = (v<<1) | (buf[i]-'0');
      i++;
    }

    *num = v;
    *off = i;
    return SDL_TRUE;
  }

  if( buf[i] == '$' )
  {
    // Hex
    i++;
    if( !ishex( buf[i] ) ) return SDL_FALSE;

    v = 0;
    for( ;; )
    {
      j = hexit( buf[i] );
      if( j == -1 ) break;
      v = (v<<4) | j;
      i++;
    }

    *num = v;
    *off = i;
    return SDL_TRUE;
  }

  if( !isnum( buf[i] ) ) return SDL_FALSE;

  v = 0;
  while( isnum( buf[i] ) )
  {
    v = (v*10) + (buf[i]-'0');
    i++;
  }

  *num = v;
  *off = i;
  return SDL_TRUE;    
}

SDL_bool mon_cmd( char *cmd, struct machine *oric, SDL_bool *needrender )
{
  int i, j, k, l;
  unsigned int v, w;
  SDL_bool done = SDL_FALSE;
  unsigned char *tmem;
  FILE *f;

  i=0;
  while( isws( cmd[i] ) ) i++;

  switch( cmd[i] )
  {
    case 'b':
      lastcmd = 0;
      i++;
      switch( cmd[i] )
      {
        case 'l':
          oric->cpu.anybp = SDL_FALSE;
          for( j=0; j<16; j++ )
          {
            if( oric->cpu.breakpoints[j] != -1 )
            {
              mon_printf( "%02d: $%04X", j, oric->cpu.breakpoints[j] );
              oric->cpu.anybp = SDL_TRUE;
            }
          }
          break;
        
        case 's':
          for( j=0; j<16; j++ )
          {
            if( oric->cpu.breakpoints[j] == -1 )
              break;
          }

          if( j == 16 )
          {
            mon_str( "Max 16 breakpoints" );
            break;
          }

          i++;
          if( !mon_getnum( oric, &v, cmd, &i, SDL_TRUE, SDL_TRUE, SDL_TRUE ) )
          {
            mon_str( "Address expected" );
            break;
          }

          oric->cpu.breakpoints[j] = v & 0xffff;
          oric->cpu.anybp = SDL_TRUE;
          mon_printf( "%02d: $%04X", j, oric->cpu.breakpoints[j] );
          break;

        case 'c':
          i++;
          if( !mon_getnum( oric, &v, cmd, &i, SDL_FALSE, SDL_FALSE, SDL_FALSE ) )
          {
            mon_str( "Breakpoint ID expected" );
            break;
          }

          if( v > 15 )
          {
            mon_str( "Invalid breakpoint ID" );
            break;
          }

          oric->cpu.breakpoints[v] = -1;
          oric->cpu.anybp = SDL_FALSE;
          for( j=0; j<16; j++ )
          {
            if( oric->cpu.breakpoints[i] != -1 )
            {
              oric->cpu.anybp = SDL_TRUE;
              break;
            }
          }
          break;
        
        case 'z':
          for( i=0; i<16; i++ )
            oric->cpu.breakpoints[i] = -1;
          oric->cpu.anybp = SDL_FALSE;
          break;
        
        default:
          mon_str( "???" );
          break;
      }
      break;

    case 'm':
      lastcmd = cmd[i];
      i++;
      if( mon_getnum( oric, &v, cmd, &i, SDL_TRUE, SDL_FALSE, SDL_FALSE ) )
        mon_addr = v;

      for( j=0; j<16; j++ )
      {
        sprintf( vsptmp, "%04X  ", mon_addr );
        for( k=0; k<8; k++ )
        {
          sprintf( &vsptmp[128], "%02X ", oric->cpu.read( &oric->cpu, mon_addr+k ) );
          strcat( vsptmp, &vsptmp[128] );
        }
        l = strlen( vsptmp );
        vsptmp[l++] = 32;
        vsptmp[l++] = '\'';
        for( k=0; k<8; k++ )
        {
          v = oric->cpu.read( &oric->cpu, mon_addr++ );
          vsptmp[l++] = ((v>31)&&(v<128))?v:'.';
        }
        vsptmp[l++] = '\'';
        vsptmp[l++] = 0;
        mon_str( vsptmp );
      }
      break;
    
    case 'd':
      lastcmd = cmd[i];
      i++;
      if( mon_getnum( oric, &v, cmd, &i, SDL_TRUE, SDL_FALSE, SDL_FALSE ) )
        mon_addr = v;

      for( j=0; j<16; j++ )
        mon_str( mon_disassemble( oric, &mon_addr ) );
      break;

    case 'r':
      lastcmd = 0;
      i++;

      j = mon_getreg( cmd, &i, SDL_TRUE, SDL_TRUE, SDL_TRUE );
      if( j == -1 )
      {
        mon_str( "Register expected" );
        break;
      }
      
      if( !mon_getnum( oric, &v, cmd, &i, SDL_TRUE, SDL_TRUE, SDL_TRUE ) )
      {
        mon_str( "Expression expected" );
        break;
      }

      switch( j )
      {
        case REG_PC:
          oric->cpu.pc = v;
          break;

        case REG_SP:
          oric->cpu.sp = v;
          break;

        case REG_A:
          oric->cpu.a = v;
          break;

        case REG_X:
          oric->cpu.x = v;
          break;

        case REG_Y:
          oric->cpu.y = v;
          break;

        case REG_VIA_PCR:
          via_write( &oric->via, VIA_PCR, v );
          break;

        case REG_VIA_ACR:
          via_write( &oric->via, VIA_ACR, v );
          break;

        case REG_VIA_SR:
          via_write( &oric->via, VIA_SR, v );
          break;

        case REG_VIA_IFR:
          via_mon_write_ifr( &oric->via, v );
          break;

        case REG_VIA_IER:
          oric->via.ier = v&0x7f;
//          via_write( &oric->via, VIA_IER, v );
          break;

        case REG_VIA_IRA:
          via_write_porta( &oric->via, 0xff, v );
          break;

        case REG_VIA_ORA:
          via_write( &oric->via, VIA_IORA, v );
          break;

        case REG_VIA_DDRA:
          via_write( &oric->via, VIA_DDRA, v );
          break;

        case REG_VIA_IRB:
          via_write_portb( &oric->via, 0xff, v );
          break;

        case REG_VIA_ORB:
          via_write( &oric->via, VIA_IORB, v );
          break;

        case REG_VIA_DDRB:
          via_write( &oric->via, VIA_DDRB, v );
          break;

        case REG_VIA_T1L:
          via_write( &oric->via, VIA_T1L_L, v&0xff );
          via_write( &oric->via, VIA_T1L_H, (v>>8)&0xff );
          break;

        case REG_VIA_T1C:
          oric->via.t1c = v;
          break;

        case REG_VIA_T2L:
          via_write( &oric->via, VIA_T2C_L, v&0xff );
          via_write( &oric->via, VIA_T2C_H, (v>>8)&0xff );
          break;

        case REG_VIA_T2C:
          oric->via.t2c = v;
          break;

        case REG_VIA_CA1:
          via_write_CA1( &oric->via, v );
          break;

        case REG_VIA_CA2:
          via_write_CA2( &oric->via, v );
          break;

        case REG_VIA_CB1:
          via_write_CB1( &oric->via, v );
          break;

        case REG_VIA_CB2:
          via_write_CB2( &oric->via, v );
          break;
      }
      *needrender = SDL_TRUE;
      break;
    
    case 0:
      return SDL_FALSE;
    
    case 'q':
      lastcmd = cmd[i];
      switch( cmd[i+1] )
      {
        case 32:
        case 0:
        case 'm':
          setemumode( oric, NULL, EM_RUNNING );
          break;
        
        case 'e':
          done = SDL_TRUE;
          break;
        
        default:
          mon_printf( "???" );
          break;
      }
      break;
    
    case 'x':
      setemumode( oric, NULL, EM_RUNNING );
      break;
    
    case 'w':
      lastcmd = 0;
      i++;
      switch( cmd[i] )
      {
        case 'm':
          i++;
          if( !mon_getnum( oric, &v, cmd, &i, SDL_TRUE, SDL_TRUE, SDL_TRUE ) )
          {
            mon_str( "Address expected" );
            break;
          }

          if( !mon_getnum( oric, &w, cmd, &i, SDL_TRUE, SDL_TRUE, SDL_TRUE ) )
          {
            mon_str( "Size expected" );
            break;
          }

          while( isws( cmd[i] ) ) i++;
          if( !cmd[i] )
          {
            mon_str( "Filename expected" );
            break;
          }

          tmem = malloc( w );
          if( !tmem )
          {
            mon_str( "Amnesia!" );
            break;
          }

          for( j=0; j<w; j++ )
            tmem[j] = oric->cpu.read( &oric->cpu, v+j );

          f = fopen( &cmd[i], "wb" );
          if( !f )
          {
            free( tmem );
            mon_printf( "Unable to open '%s'", &cmd[i] );
            break;
          }

          fwrite( tmem, w, 1, f );
          fclose( f );
          free( tmem );
          mon_printf( "Written %d bytes to '%s'", w, &cmd[i] );
          break;
      }
      break;

    case '?':
      lastcmd = cmd[i];
      mon_str( "KEYS:" );
      //          |          -          |          -          |
      mon_str( "  F2 : Quit Monitor     F3 : Toggle Console" );
      mon_str( "  F4 : Toggle info      F9 : Reset cycles" );
      mon_str( "  F10: Step CPU" );
      mon_str( " " );
      mon_str( "COMMANDS:" );
      mon_str( "  bs <addr>             - Set breakpoint" );
      mon_str( "  bc <bp id>            - Clear breakpoint" );
      mon_str( "  bz                    - Zap breakpoints" );
      mon_str( "  bl                    - List breakpoints" );
      mon_str( "  m <addr>              - Dump memory" );
      mon_str( "  d <addr>              - Disassemble" );
      mon_str( "  r <reg> <val>         - Set <reg> to <val>" );
      mon_str( "  q, x or qm            - Quit monitor" );
      mon_str( "  qe                    - Quit emulator" );
      mon_str( "  wm <addr> <len> <file>- Write mem to disk" );
      break;

    default:
      lastcmd = 0;
      mon_printf( "???" );
      break;
  }

  return done;
}

static SDL_bool mon_console_keydown( SDL_Event *ev, struct machine *oric, SDL_bool *needrender )
{
  int i;
  SDL_bool done;

  done = SDL_FALSE;
  switch( ev->key.keysym.sym )
  {
    case SDLK_UP:
      if( histp >= (histu-1) ) break;
      mon_hide_curs();
      strcpy( ibuf, &history[++histp][0] );
      ilen = strlen( ibuf );
      cursx = ilen;
      tzstrpos( tz[TZ_MONITOR], 2, 19, "                                               " );
      tzstrpos( tz[TZ_MONITOR], 2, 19, ibuf );
      mon_show_curs();
      *needrender = SDL_TRUE;
      break;

    case SDLK_DOWN:
      mon_hide_curs();
      if( histp <= 0 )
      {
        histp = -1;
        ibuf[0] = 0;
        ilen = 0;
        cursx = 0;
      } else {
        strcpy( ibuf, &history[--histp][0] );
        ilen = strlen( ibuf );
        cursx = ilen;
      }
      tzstrpos( tz[TZ_MONITOR], 2, 19, "                                               " );
      tzstrpos( tz[TZ_MONITOR], 2, 19, ibuf );
      mon_show_curs();
      *needrender = SDL_TRUE;
      break;
    
    default:
      break;
  }

  switch( ev->key.keysym.unicode )
  {
    case SDLK_BACKSPACE:
      if( cursx > 0 )
      {
        mon_hide_curs();
        for( i=cursx-1; i<ilen; i++ )
          ibuf[i] = ibuf[i+1];
        cursx--;
        ilen = strlen( ibuf );
        tzstrpos( tz[TZ_MONITOR], 2, 19, ibuf );
        tzstr( tz[TZ_MONITOR], " " );
        mon_show_curs();
        *needrender = SDL_TRUE;
      }
      break;
    
    case SDLK_LEFT:
      if( cursx > 0 )
      {
        mon_hide_curs();
        cursx--;
        mon_show_curs();
        *needrender = SDL_TRUE;
      }
      break;

    case SDLK_RIGHT:
      if( cursx < ilen )
      {
        mon_hide_curs();
        cursx++;
        mon_show_curs();
        *needrender = SDL_TRUE;
      }
      break;
    
    case SDLK_RETURN:
      mon_hide_curs();
      ibuf[ilen] = 0;
      if( ilen == 0 )
      {
        switch( lastcmd )
        {
          case 'm':
          case 'd':
            ibuf[cursx++] = lastcmd;
            ibuf[cursx] = 0;
            ilen = 1;
            break;
        }
      } else {
        if( ( histu > 0 ) && ( strcmp( &history[0][0], ibuf ) == 0 ) )
        {
        } else {
          if( histu > 0 )
          {
            for( i=histu; i>0; i-- )
              strcpy( &history[i][0], &history[i-1][0] );

          }
          strcpy( &history[0][0], ibuf );
          if( histu < 10 ) histu++;
        }
      }
      histp = -1;

      tzsetcol( tz[TZ_MONITOR], 2, 3 ); 
      done |= mon_cmd( ibuf, oric, needrender );
      mon_start_input();
      mon_show_curs();
      *needrender = SDL_TRUE;
      break;

    default:
      if( ( ev->key.keysym.unicode > 31 ) && ( ev->key.keysym.unicode < 127 ) )
      {
        if( ilen >= 47 ) break;

        mon_hide_curs();
        for( i=ilen+1; i>cursx; i-- )
          ibuf[i] = ibuf[i-1];
        ibuf[cursx] = ev->key.keysym.unicode;
        cursx++;
        ilen++;
        tzstrpos( tz[TZ_MONITOR], 2, 19, ibuf );
        tzstr( tz[TZ_MONITOR], " " );
        mon_show_curs();
        *needrender = SDL_TRUE;
      }
      break;
  }
  return done;
}

static SDL_bool mon_mwatch_keydown( SDL_Event *ev, struct machine *oric, SDL_bool *needrender )
{
  SDL_bool done;
  unsigned int v;
  int i;

  done = SDL_FALSE;
  switch( ev->key.keysym.sym )
  {
    case SDLK_UP:
      mw_addr -= kshifted ? 8*18 : 8;
      *needrender = SDL_TRUE;
      break;

    case SDLK_DOWN:
      mw_addr += kshifted ? 8*18 : 8;
      *needrender = SDL_TRUE;
      break;

    case SDLK_PAGEUP:
      mw_addr -= 8*18;
      *needrender = SDL_TRUE;
      break;

    case SDLK_PAGEDOWN:
      mw_addr += 8*18;
      *needrender = SDL_TRUE;
      break;
    
    case SDLK_RETURN:
      if( mw_mode != 1 ) break;
      mw_ibuf[5] = 0;
      mw_mode = 0;
      i = 0;
      if( mon_getnum( oric, &v, mw_ibuf, &i, SDL_FALSE, SDL_FALSE, SDL_FALSE ) )
        mw_addr = v;
      *needrender = SDL_TRUE;
      break;
    
    case SDLK_ESCAPE:
      mw_mode = 0;
      break;
     
    case SDLK_BACKSPACE:
      if( mw_mode != 1 ) break;
      if( mw_koffs <= 0 ) break;
      mw_ibuf[mw_koffs--] = 0;
      *needrender = SDL_TRUE;
      break;

    default:
      break;
  }

  if( ishex( ev->key.keysym.unicode ) )
  {
    if( mw_mode != 1 )
    {
      mw_mode = 1;
      mw_ibuf[0] = '$';
      mw_ibuf[1] = 0;
      mw_koffs = 0;
    }

    if( mw_koffs < 4 )
    {
      mw_ibuf[++mw_koffs] = ev->key.keysym.unicode;
      mw_ibuf[mw_koffs+1] = 0;
    }

    *needrender = SDL_TRUE;
  }

  return done;
}

SDL_bool mon_event( SDL_Event *ev, struct machine *oric, SDL_bool *needrender )
{
  SDL_bool done;
  SDL_bool donkey;

  done = SDL_FALSE;
  donkey = SDL_FALSE;
  switch( ev->type )
  {
    case SDL_KEYUP:
      switch( ev->key.keysym.sym )
      {
#if CYCLECOUNT
        case SDLK_F9:
          oric->cpu.cycles = 0;
          *needrender = SDL_TRUE;
          break;
#endif
        case SDLK_LSHIFT:
        case SDLK_RSHIFT:
          kshifted = SDL_FALSE;
          break;

        case SDLK_F2:
          // In case we're on a breakpoint
          m6502_inst( &oric->cpu, SDL_FALSE );
          via_clock( &oric->via, oric->cpu.icycles );
          ay_ticktock( &oric->ay, oric->cpu.icycles );
          if( oric->drivetype ) wd17xx_ticktock( &oric->wddisk, oric->cpu.icycles );
          if( oric->cpu.rastercycles <= 0 )
          {
            video_doraster( oric );
            oric->cpu.rastercycles += oric->cyclesperraster;
          }
          *needrender = SDL_TRUE;
          setemumode( oric, NULL, EM_RUNNING );
          break;

        case SDLK_F3:
          cshow = (cshow+1)%CSHOW_LAST;
          *needrender = SDL_TRUE;
          break;

        case SDLK_F4:
          mshow = (mshow+1)%MSHOW_LAST;
          if( ( oric->drivetype == DRV_NONE ) && ( mshow == MSHOW_DISK ) )
            mshow = (mshow+1)%MSHOW_LAST;
          *needrender = SDL_TRUE;
          break;

        default:
          break;
      }
      break;

    case SDL_KEYDOWN:
      switch( ev->key.keysym.sym )
      {
        case SDLK_LSHIFT:
        case SDLK_RSHIFT:
          kshifted = SDL_TRUE;
          donkey = SDL_TRUE;
          break;

        case SDLK_F10:
          m6502_inst( &oric->cpu, SDL_FALSE );
          via_clock( &oric->via, oric->cpu.icycles );
          ay_ticktock( &oric->ay, oric->cpu.icycles );
          if( oric->drivetype ) wd17xx_ticktock( &oric->wddisk, oric->cpu.icycles );
          if( oric->cpu.rastercycles <= 0 )
          {
            video_doraster( oric );
            oric->cpu.rastercycles += oric->cyclesperraster;
          }
          *needrender = SDL_TRUE;
          donkey = SDL_TRUE;
          break;
        
        default:
          break;
      }

      if( !donkey )
      {
        switch( cshow )
        {
          case CSHOW_CONSOLE:
            done |= mon_console_keydown( ev, oric, needrender );
            break;

          case CSHOW_MWATCH:
            done |= mon_mwatch_keydown( ev, oric, needrender );
            break;
        }
      }
      break;
  }

  return done;
}
