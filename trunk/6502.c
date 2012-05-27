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
**  6502 emulation
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "system.h"
#include "6502.h"

void dbg_printf( char *fmt, ... );


// These are the default read/write routines.
// You need to overwrite these with your own ones if you want
// the CPU to do anything other than constantly execute brk
// instructions :-)
static void nullwrite( struct m6502 *cpu, unsigned short addr, unsigned char data )
{
}

static unsigned char nullread( struct m6502 *cpu, unsigned short addr )
{
  return 0;
}

/*
** Initialise an already allocated 6502 instance.
** The userdata param is just used to fill in the
** cpu->userdata field.
*/
void m6502_init( struct m6502 *cpu, void *userdata, SDL_bool nukebreakpoints )
{
  int i;
  cpu->rastercycles = 0;
  cpu->icycles = 0;
  cpu->cycles = 0;
  cpu->write = nullwrite;
  cpu->read  = nullread;

  if( nukebreakpoints )
  {
    for( i=0; i<16; i++ )
    {
      cpu->breakpoints[i] = -1;
      cpu->membreakpoints[i].flags = 0;
    }
    cpu->anybp = SDL_FALSE;
    cpu->anymbp = SDL_FALSE;
  }
  cpu->userdata = userdata;
}

/*
** Resets the 6502 cpu to powerup state */
void m6502_reset( struct m6502 *cpu )
{
  cpu->a = cpu->x = cpu->y = 0;
  cpu->sp = 0xff;
  cpu->f_c = 0;
  cpu->f_z = 1;
  cpu->f_i = 0;
  cpu->f_d = 0;
  cpu->f_b = 0;
  cpu->f_v = 0;
  cpu->f_n = 0;
  cpu->pc = (cpu->read( cpu, 0xfffd )<<8) | cpu->read( cpu, 0xfffc );
  cpu->lastpc = 0;
  cpu->nmi = SDL_FALSE;
  cpu->irq = 0;
  cpu->nmicount = 0;
}

// Macros to set flags for various instructions
#define FLAG_ZCN(n) cpu->f_z = ((n)&0xff) == 0;\
                    cpu->f_c = ((n)&0xff00) != 0;\
                    cpu->f_n = ((n)&0x80) != 0

#define FLAG_SZCN(n) cpu->f_z = ((n)&0xff) == 0;\
                     cpu->f_c = ((n)&0xff00) == 0;\
                     cpu->f_n = ((n)&0x80) != 0

#define FLAG_ZN(n)  cpu->f_z = (n) == 0;\
                    cpu->f_n = ((n)&0x80) != 0


// Macro to perform ADC logic
#define DO_ADC if( cpu->f_d )\
               {\
                 r = (cpu->a&0xf)+(v&0xf)+cpu->f_c;\
                 if( r > 9 ) r += 6;\
                 t = (cpu->a>>4)+(v>>4)+(r>15?1:0);\
                 if( t > 9 ) t += 6;\
                 cpu->a = (r&0xf)|(t<<4);\
                 cpu->f_c = t>15 ? 1 : 0;\
                 cpu->f_z = cpu->a!=0;\
                 cpu->f_n = cpu->a&0x80;\
               } else {\
                 r = cpu->a + v + cpu->f_c;\
                 cpu->f_v = ((cpu->a^v)&(cpu->a^(r&0xff))&0x80) ? 1 : 0;\
                 FLAG_ZCN(r);\
                 cpu->a = r;\
               }

// Macro to perform AND logic
#define DO_AND cpu->a &= v;\
               FLAG_ZN(cpu->a)

// Macro to perform ASL logic
#define DO_ASL(s) r = s << 1;\
                  FLAG_ZCN(r);\
                  s = r;

// Macro to perform ORA logic
#define DO_ORA cpu->a |= v;\
               FLAG_ZN(cpu->a)

// Macro to perform ROL logic
#define DO_ROL(s) r = (s<<1)|cpu->f_c;\
                  cpu->f_c = r&0x100 ? 1 : 0;\
                  s = r;\
                  FLAG_ZN(s)

// Macro to perform ROR logic
#define DO_ROR(s) r = (s)&1;\
                  s = (s>>1)|(cpu->f_c<<7);\
                  cpu->f_c = r;\
                  FLAG_ZN(s)

// Macro to perform EOR logic
#define DO_EOR cpu->a ^= v;\
               FLAG_ZN(cpu->a)

// Macro to perform LSR logic
#define DO_LSR(s) cpu->f_c = (s)&0x01;\
                  s >>= 1;\
                  FLAG_ZN(s)

// Macro to perform SBC logic
#define DO_SBC if( cpu->f_d )\
               {\
                 r = (cpu->a&0xf) - (v&0xf) - (cpu->f_c^1);\
                 if( r&0x10 ) r -= 6;\
                 t = (cpu->a>>4) - (v>>4) - ((r&0x10)>>4);\
                 if( t&0x10 ) t -= 6;\
                 cpu->a = (r&0xf)|(t<<4);\
                 cpu->f_c = (t>15) ? 0 : 1;\
                 cpu->f_z = cpu->a!=0;\
                 cpu->f_n = cpu->a&0x80;\
               } else {\
                 r = (cpu->a - v) - (cpu->f_c^1);\
                 cpu->f_v = ((cpu->a^v)&(cpu->a^(r&0xff))&0x80) ? 1 : 0;\
                 FLAG_SZCN(r);\
                 cpu->a = r;\
               }


#define BADDR_ZP  baddr = cpu->read( cpu, cpu->pc )
#define BADDR_ZPX baddr = (cpu->read( cpu, cpu->pc ) + cpu->x)&0xff
#define BADDR_ZPY baddr = (cpu->read( cpu, cpu->pc ) + cpu->y)&0xff
#define BADDR_ABS baddr = (cpu->read( cpu, cpu->pc+1 )<<8) | cpu->read( cpu, cpu->pc )
#define BADDR_ABX baddr = ((cpu->read( cpu, cpu->pc+1 )<<8) | cpu->read( cpu, cpu->pc ))+cpu->x
#define BADDR_ABY baddr = ((cpu->read( cpu, cpu->pc+1 )<<8) | cpu->read( cpu, cpu->pc ))+cpu->y
#define BADDR_ZIX baddr = (unsigned char)(cpu->read( cpu, cpu->pc )+cpu->x); baddr = (cpu->read( cpu, baddr+1 )<<8) | cpu->read( cpu, baddr )
#define BADDR_ZIY baddr = cpu->read( cpu, cpu->pc ); baddr = ((cpu->read( cpu, baddr+1 )<<8) | cpu->read( cpu, baddr ))+cpu->y

#define NBADDR_ZP  baddr = cpu->read( cpu, cpu->calcpc+1 )
#define NBADDR_ZPX baddr = (cpu->read( cpu, cpu->calcpc+1 ) + cpu->x)&0xff
#define NBADDR_ZPY baddr = (cpu->read( cpu, cpu->calcpc+1 ) + cpu->y)&0xff
#define NBADDR_ABS baddr = (cpu->read( cpu, cpu->calcpc+2 )<<8) | cpu->read( cpu, cpu->calcpc+1 )
#define NBADDR_ABX baddr = ((cpu->read( cpu, cpu->calcpc+2 )<<8) | cpu->read( cpu, cpu->calcpc+1 ))+cpu->x
#define NBADDR_ABY baddr = ((cpu->read( cpu, cpu->calcpc+2 )<<8) | cpu->read( cpu, cpu->calcpc+1 ))+cpu->y
#define NBADDR_ZIX baddr = (unsigned char)(cpu->read( cpu, cpu->calcpc+1 )+cpu->x); baddr = (cpu->read( cpu, baddr+1 )<<8) | cpu->read( cpu, baddr )
#define NBADDR_ZIY baddr = cpu->read( cpu, cpu->calcpc+1 ); baddr = ((cpu->read( cpu, baddr+1 )<<8) | cpu->read( cpu, baddr ))+cpu->y

#define R_BADDR_ZP   NBADDR_ZP; raddr = baddr; rlen = 1
#define W_BADDR_ZP   NBADDR_ZP; waddr = baddr; wlen = 1
#define RW_BADDR_ZP  NBADDR_ZP; waddr = raddr = baddr; wlen = rlen = 1
#define R_BADDR_ZPX  NBADDR_ZPX; raddr = baddr; rlen = 1
#define W_BADDR_ZPX  NBADDR_ZPX; waddr = baddr; wlen = 1
#define RW_BADDR_ZPX NBADDR_ZPX; waddr = raddr = baddr; wlen = rlen = 1
#define R_BADDR_ZPY  NBADDR_ZPY; raddr = baddr; rlen = 1
#define W_BADDR_ZPY  NBADDR_ZPY; waddr = baddr; wlen = 1
#define RW_BADDR_ZPY NBADDR_ZPY; waddr = raddr = baddr; wlen = rlen = 1
#define R_BADDR_ABS  NBADDR_ABS; raddr = baddr; rlen = 1
#define W_BADDR_ABS  NBADDR_ABS; waddr = baddr; wlen = 1
#define RW_BADDR_ABS NBADDR_ABS; waddr = raddr = baddr; wlen = rlen = 1
#define R_BADDR_ABX  NBADDR_ABX; raddr = baddr; rlen = 1
#define W_BADDR_ABX  NBADDR_ABX; waddr = baddr; wlen = 1
#define RW_BADDR_ABX NBADDR_ABX; waddr = raddr = baddr; wlen = rlen = 1
#define R_BADDR_ABY  NBADDR_ABY; raddr = baddr; rlen = 1
#define W_BADDR_ABY  NBADDR_ABY; waddr = baddr; wlen = 1
#define RW_BADDR_ABY NBADDR_ABY; waddr = raddr = baddr; wlen = rlen = 1
#define R_BADDR_ZIX  NBADDR_ZIX; raddr = baddr; rlen = 1
#define W_BADDR_ZIX  NBADDR_ZIX; waddr = baddr; wlen = 1
#define RW_BADDR_ZIX NBADDR_ZIX; waddr = raddr = baddr; wlen = rlen = 1
#define R_BADDR_ZIY  NBADDR_ZIY; raddr = baddr; rlen = 1
#define W_BADDR_ZIY  NBADDR_ZIY; waddr = baddr; wlen = 1
#define RW_BADDR_ZIY NBADDR_ZIY; waddr = raddr = baddr; wlen = rlen = 1

// Macros for each addressing mode of the 6502
#define READ_IMM v=cpu->read( cpu, cpu->pc++ )
#define READ_ZP  v=cpu->read( cpu, cpu->read( cpu, cpu->pc++ ) )
#define READ_ZPX v=cpu->read( cpu, (cpu->read( cpu, cpu->pc++ ) + cpu->x)&0xff )
#define READ_ZPY v=cpu->read( cpu, (cpu->read( cpu, cpu->pc++ ) + cpu->y)&0xff )
#define READ_ABS v=cpu->read( cpu, (cpu->read( cpu, cpu->pc+1 )<<8) | cpu->read( cpu, cpu->pc ) ); cpu->pc+=2
#define READ_ABX BADDR_ABX; v = cpu->read( cpu, baddr ); cpu->pc+=2
#define READ_ABY BADDR_ABY; v = cpu->read( cpu, baddr ); cpu->pc+=2
#define READ_ZIX BADDR_ZIX; v = cpu->read( cpu, baddr ); cpu->pc++
#define READ_ZIY BADDR_ZIY; v = cpu->read( cpu, baddr ); cpu->pc++

#define KREAD_ZP  baddr = cpu->read( cpu, cpu->pc++ ); v = cpu->read( cpu, baddr )
#define KREAD_ZPX baddr = (unsigned char)(cpu->read( cpu, cpu->pc++ )+cpu->x); v = cpu->read( cpu, baddr )
#define KREAD_ABS baddr = (cpu->read( cpu, cpu->pc+1 )<<8) | cpu->read( cpu, cpu->pc ); v=cpu->read( cpu, baddr ); cpu->pc+=2

// .. and for writing
#define WRITE_ZP(n)  cpu->write( cpu, cpu->read( cpu, cpu->pc++ ), n )
#define WRITE_ZPX(n) cpu->write( cpu, (cpu->read( cpu, cpu->pc++ ) + cpu->x)&0xff, n )
#define WRITE_ZPY(n) cpu->write( cpu, (cpu->read( cpu, cpu->pc++ ) + cpu->y)&0xff, n )
#define WRITE_ABS(n) cpu->write( cpu, (cpu->read( cpu, cpu->pc+1 )<<8) | cpu->read( cpu, cpu->pc ), n ); cpu->pc+=2
#define WRITE_ABX(n) baddr = ((cpu->read( cpu, cpu->pc+1 )<<8) | cpu->read( cpu, cpu->pc )); cpu->write( cpu, baddr + cpu->x, n ); cpu->pc+=2
#define WRITE_ABY(n) baddr = ((cpu->read( cpu, cpu->pc+1 )<<8) | cpu->read( cpu, cpu->pc )); cpu->write( cpu, baddr + cpu->y, n ); cpu->pc+=2
#define WRITE_ZIX(n) baddr = (unsigned char)(cpu->read( cpu, cpu->pc++ )+cpu->x); cpu->write( cpu, (cpu->read( cpu, baddr+1 )<<8) | cpu->read( cpu, baddr ), n )
#define WRITE_ZIY(n) baddr = cpu->read( cpu, cpu->pc++ ); baddr = (cpu->read( cpu, baddr+1 )<<8) | cpu->read( cpu, baddr ); cpu->write( cpu, baddr + cpu->y, n )

// Page check to see if an offset takes you out of the base page (baddr)
#define PAGECHECK(n) ( ((baddr+n)&0xff00) != (baddr&0xff00) )

// Same as above, but when (baddr+n) is already calculated (in cpu->baddr)
#define CPAGECHECK ((cpu->baddr&0xff00) != (baddr&0xff00))

// Page check to see if a branch takes you out of the current page
#define BPAGECHECK ( (cpu->baddr&0xff00) != (cpu->calcpc&0xff00) )

// Macro to perform branch logic
#define BRANCH(condition) if( condition ) cpu->pc = cpu->baddr; else cpu->pc++;

// Macro to calculate cycles of a branch instruction
#define IBRANCH(condition) cpu->icycles = 2;\
                           if( condition )\
                           {\
                             cpu->baddr = cpu->calcpc+2+((signed char)cpu->read( cpu, cpu->calcpc+1 ));\
                             cpu->icycles++;\
                             if( BPAGECHECK ) cpu->icycles++;\
                           }\

// Macros to simplify pushing and popping
#define PUSHB(n) cpu->write( cpu, (cpu->sp--)+0x100, n )
#define POPB cpu->read( cpu, (++cpu->sp)+0x100 )
#define PUSHW(n) PUSHB( n>>8 ); PUSHB( n )
#define POPW(n)  n = (cpu->read(cpu,((cpu->sp+2)&0xff)+0x100)<<8)|cpu->read(cpu,((cpu->sp+1)&0xff)+0x100); cpu->sp+=2


// Get the number of cycles the NEXT cpu instruction will take
// Returns TRUE if we've hit some kind of breakpoint
SDL_bool m6502_set_icycles( struct m6502 *cpu, SDL_bool dobp, char *bpmsg )
{
  unsigned short baddr;
  unsigned int extra = 0;
  int i;

  if( cpu->nmicount > 0 )
  {
    cpu->nmicount--;
    if( !cpu->nmicount )
      cpu->nmi = SDL_FALSE;
  }

  if( cpu->nmi )
  {
    extra = 7;
    cpu->calcpc = (cpu->read( cpu, 0xfffb )<<8)|cpu->read( cpu, 0xfffa );
    cpu->calcint = 2;
  } else if( ( cpu->irq ) && ( cpu->f_i == 0 ) ) {
    extra = 7;
    cpu->calcpc = (cpu->read( cpu, 0xffff )<<8)|cpu->read( cpu, 0xfffe );
    cpu->calcint = 1;
  }
  else
  {
    cpu->calcpc = cpu->pc;
    cpu->calcint = 0;
  }

  cpu->calcop = cpu->read( cpu, cpu->calcpc );
  if( dobp )
  {
    if( cpu->anybp )
    {
      for( i=0; i<16; i++ )
      {
        if( ( cpu->breakpoints[i] != -1 ) && ( cpu->calcpc == cpu->breakpoints[i] ) )
          return SDL_TRUE;
      }
    }

    if( cpu->anymbp )
    {
      unsigned short waddr, raddr, wlen, rlen;

      waddr = raddr = 0;
      wlen = rlen = 0;
    
      // Going to read or write?
      switch( cpu->calcop )
      {
        case 0x00: // { "BRK", AM_IMP },  // 00
          waddr = (cpu->sp+0x100)-3;
          wlen  = 3;
          break;

        case 0x40: // { "RTI", AM_IMP },  // 40
          waddr = (cpu->sp+0x100);
          wlen = 3;
          break;

        case 0x08: // { "PHP", AM_IMP },  // 08
        case 0x48: // { "PHA", AM_IMP },  // 48
          waddr = (cpu->sp+0x100)-1;
          wlen = 1;
          break;

        case 0x28: // { "PLP", AM_IMP },  // 28
        case 0x68: // { "PLA", AM_IMP },  // 68
          raddr = cpu->sp+0x100;
          rlen = 1;
          break;

        case 0x60: // { "RTS", AM_IMP },  // 60
          raddr = cpu->sp+0x100;
          rlen = 2;
          break;

        case 0x20: // { "JSR", AM_ABS },  // 20
          waddr = (cpu->sp+0x100)-2;
          wlen = 2;
          break;

        case 0x06: // { "ASL", AM_ZP  },  // 06
        case 0x26: // { "ROL", AM_ZP  },  // 26
        case 0x46: // { "LSR", AM_ZP  },  // 46
        case 0x66: // { "ROR", AM_ZP  },  // 66
        case 0xC6: // { "DEC", AM_ZP  },  // C6
        case 0xE6: // { "INC", AM_ZP  },  // E6
        case 0xC7: // { "DCP", AM_ZP  },  // C7 (illegal)
        case 0xE7: // { "ISC", AM_ZP  },  // E7 (illegal)
        case 0x27: // { "RLA", AM_ZP  },  // 27 (illegal)
        case 0x67: // { "RRA", AM_ZP  },  // 67 (illegal)
        case 0x07: // { "SLO", AM_ZP  },  // 07 (illegal)
        case 0x47: // { "SRE", AM_ZP  },  // 47 (illegal)
          RW_BADDR_ZP;
          break;

        case 0x0E: // { "ASL", AM_ABS },  // 0E
        case 0x2E: // { "ROL", AM_ABS },  // 2E
        case 0x4E: // { "LSR", AM_ABS },  // 4E
        case 0x6E: // { "ROR", AM_ABS },  // 6E
        case 0xCE: // { "DEC", AM_ABS },  // CE
        case 0xEE: // { "INC", AM_ABS },  // EE
        case 0xCF: // { "DCP", AM_ABS },  // CF (illegal)
        case 0xEF: // { "ISC", AM_ABS },  // EF (illegal)
        case 0x2F: // { "RLA", AM_ABS },  // 2F (illegal)
        case 0x6F: // { "RRA", AM_ABS },  // 6F (illegal)
        case 0x0F: // { "SLO", AM_ABS },  // 0F (illegal)
        case 0x4F: // { "SRE", AM_ABS },  // 4F (illegal)
          RW_BADDR_ABS;
          break;

        case 0x1E: // { "ASL", AM_ABX },  // 1E
        case 0x3E: // { "ROL", AM_ABX },  // 3E
        case 0x5E: // { "LSR", AM_ABX },  // 5E
        case 0x7E: // { "ROR", AM_ABX },  // 7E
        case 0xDE: // { "DEC", AM_ABX },  // DE
        case 0xFE: // { "INC", AM_ABX },  // FE
        case 0xDF: // { "DCP", AM_ABX },  // DF (illegal)
        case 0xFF: // { "ISC", AM_ABX },  // FF (illegal)
        case 0x3F: // { "RLA", AM_ABX },  // 3F (illegal)
        case 0x7F: // { "RRA", AM_ABX },  // 7F (illegal)
        case 0x1F: // { "SLO", AM_ABX },  // 1F (illegal)
        case 0x5F: // { "SRE", AM_ABX },  // 5F (illegal)
          RW_BADDR_ABX;
          break;

        case 0x16: // { "ASL", AM_ZPX },  // 16
        case 0x36: // { "ROL", AM_ZPX },  // 36
        case 0x56: // { "LSR", AM_ZPX },  // 56
        case 0x76: // { "ROR", AM_ZPX },  // 76
        case 0xD6: // { "DEC", AM_ZPX },  // D6
        case 0xF6: // { "INC", AM_ZPX },  // F6
        case 0xD7: // { "DCP", AM_ZPX },  // D7 (illegal)
        case 0xF7: // { "ISC", AM_ZPX },  // F7 (illegal)
        case 0x37: // { "RLA", AM_ZPX },  // 37 (illegal)
        case 0x77: // { "RRA", AM_ZPX },  // 77 (illegal)
        case 0x17: // { "SLO", AM_ZPX },  // 17 (illegal)
        case 0x57: // { "SRE", AM_ZPX },  // 57 (illegal)
          RW_BADDR_ZPX;
          break;
        
        case 0xDB: // { "DCP", AM_ABY },  // DB (illegal)
        case 0xFB: // { "ISC", AM_ABY },  // FB (illegal)
        case 0x3B: // { "RLA", AM_ABY },  // 3B (illegal)
        case 0x7B: // { "RRA", AM_ABY },  // 7B (illegal)
        case 0x1B: // { "SLO", AM_ABY },  // 1B (illegal)
        case 0x5B: // { "SRE", AM_ABY },  // 5B (illegal)
          RW_BADDR_ABY;
          break;
        
        case 0xC3: // { "DCP", AM_ZIX },  // C3 (illegal)
        case 0xE3: // { "ISC", AM_ZIX },  // E3 (illegal)
        case 0x23: // { "RLA", AM_ZIX },  // 23 (illegal)
        case 0x63: // { "RRA", AM_ZIX },  // 63 (illegal)
        case 0x03: // { "SLO", AM_ZIX },  // 03 (illegal)
        case 0x43: // { "SRE", AM_ZIX },  // 43 (illegal)
          RW_BADDR_ZIX;
          break;
        
        case 0xD3: // { "DCP", AM_ZIY },  // D3 (illegal)
        case 0xF3: // { "ISC", AM_ZIY },  // F3 (illegal)
        case 0x33: // { "RLA", AM_ZIY },  // 33 (illegal)
        case 0x73: // { "RRA", AM_ZIY },  // 73 (illegal)
        case 0x13: // { "SLO", AM_ZIY },  // 13 (illegal)
        case 0x53: // { "SRE", AM_ZIY },  // 53 (illegal)
          RW_BADDR_ZIY;
          break;

        case 0x01: // { "ORA", AM_ZIX },  // 01
        case 0x21: // { "AND", AM_ZIX },  // 21
        case 0x41: // { "EOR", AM_ZIX },  // 41
        case 0x61: // { "ADC", AM_ZIX },  // 61
        case 0xA1: // { "LDA", AM_ZIX },  // A1
        case 0xC1: // { "CMP", AM_ZIX },  // C1
        case 0xE1: // { "SBC", AM_ZIX },  // E1
        case 0xA3: // { "LAX", AM_ZIX },  // A3 (illegal)
          R_BADDR_ZIX;
          break;

        case 0x11: // { "ORA", AM_ZIY },  // 11
        case 0x31: // { "AND", AM_ZIY },  // 31
        case 0x51: // { "EOR", AM_ZIY },  // 51
        case 0x71: // { "ADC", AM_ZIY },  // 71
        case 0xB1: // { "LDA", AM_ZIY },  // B1
        case 0xD1: // { "CMP", AM_ZIY },  // D1
        case 0xF1: // { "SBC", AM_ZIY },  // F1
        case 0xBB: // { "LAS", AM_ZIY },  // BB
        case 0xB3: // { "LAX", AM_ZIY },  // B3 (illegal)
          R_BADDR_ZIY;
          break;

        case 0x05: // { "ORA", AM_ZP  },  // 05
        case 0x24: // { "BIT", AM_ZP  },  // 24
        case 0x25: // { "AND", AM_ZP  },  // 25
        case 0x45: // { "EOR", AM_ZP  },  // 45
        case 0x65: // { "ADC", AM_ZP  },  // 65
        case 0xA4: // { "LDY", AM_ZP  },  // A4
        case 0xA5: // { "LDA", AM_ZP  },  // A5
        case 0xA6: // { "LDX", AM_ZP  },  // A6
        case 0xC4: // { "CPY", AM_ZP  },  // C4
        case 0xC5: // { "CMP", AM_ZP  },  // C5
        case 0xE4: // { "CPX", AM_ZP  },  // E4
        case 0xE5: // { "SBC", AM_ZP  },  // E5
        case 0xA7: // { "LAX", AM_ZP  },  // A7 (illegal)
          R_BADDR_ZP;
          break;

        case 0x0D: // { "ORA", AM_ABS },  // 0D
        case 0x2C: // { "BIT", AM_ABS },  // 2C
        case 0x2D: // { "AND", AM_ABS },  // 2D
        case 0x4D: // { "EOR", AM_ABS },  // 4D
        case 0x6D: // { "ADC", AM_ABS },  // 6D
        case 0xAC: // { "LDY", AM_ABS },  // AC
        case 0xAD: // { "LDA", AM_ABS },  // AD
        case 0xAE: // { "LDX", AM_ABS },  // AE
        case 0xCC: // { "CPY", AM_ABS },  // CC
        case 0xCD: // { "CMP", AM_ABS },  // CD
        case 0xEC: // { "CPX", AM_ABS },  // EC
        case 0xED: // { "SBC", AM_ABS },  // ED
        case 0xAF: // { "LAX", AM_ABS },  // AF (illegal)
          R_BADDR_ABS;
          break;

        case 0x15: // { "ORA", AM_ZPX },  // 15
        case 0x35: // { "AND", AM_ZPX },  // 35
        case 0x55: // { "EOR", AM_ZPX },  // 55
        case 0x75: // { "ADC", AM_ZPX },  // 75
        case 0xB4: // { "LDY", AM_ZPX },  // B4
        case 0xB5: // { "LDA", AM_ZPX },  // B5
        case 0xD5: // { "CMP", AM_ZPX },  // D5
        case 0xF5: // { "SBC", AM_ZPX },  // F5
          R_BADDR_ZPX;
          break;

        case 0x19: // { "ORA", AM_ABY },  // 19
        case 0x39: // { "AND", AM_ABY },  // 39
        case 0x59: // { "EOR", AM_ABY },  // 59
        case 0x79: // { "ADC", AM_ABY },  // 79
        case 0xB9: // { "LDA", AM_ABY },  // B9
        case 0xBE: // { "LDX", AM_ABY },  // BE
        case 0xD9: // { "CMP", AM_ABY },  // D9
        case 0xF9: // { "SBC", AM_ABY },  // F9
        case 0xBF: // { "LAX", AM_ABY },  // BF (illegal)
          R_BADDR_ABY;
          break;    

        case 0x1D: // { "ORA", AM_ABX },  // 1D
        case 0x3D: // { "AND", AM_ABX },  // 3D
        case 0x5D: // { "EOR", AM_ABX },  // 5D
        case 0x7D: // { "ADC", AM_ABX },  // 7D
        case 0xBC: // { "LDY", AM_ABX },  // BC
        case 0xBD: // { "LDA", AM_ABX },  // BD
        case 0xDD: // { "CMP", AM_ABX },  // DD
        case 0xFD: // { "SBC", AM_ABX },  // FD
          R_BADDR_ABX;
          break;    

        case 0xB6: // { "LDX", AM_ZPY },  // B6
        case 0xB7: // { "LAX", AM_ZPY },  // B7 (illegal)
          R_BADDR_ZPY;
          break;

        case 0x6C: // { "JMP", AM_IND },  // 6C
          raddr = (cpu->read( cpu, cpu->calcpc+2 )<<8)|cpu->read( cpu, cpu->calcpc+1 );
          rlen = 2;
          break;
          
        case 0x81: // { "STA", AM_ZIX },  // 81
        case 0x83: // { "SAX", AM_ZIX },  // 83 (illegal)
          W_BADDR_ZIX;
          break;

        case 0x84: // { "STY", AM_ZP  },  // 84
        case 0x85: // { "STA", AM_ZP  },  // 85
        case 0x86: // { "STX", AM_ZP  },  // 86
        case 0x87: // { "SAX", AM_ZP  },  // 87 (illegal)
          W_BADDR_ZP;
          break;
         
        case 0x8C: // { "STY", AM_ABS },  // 8C
        case 0x8D: // { "STA", AM_ABS },  // 8D
        case 0x8E: // { "STX", AM_ABS },  // 8E
        case 0x8F: // { "SAX", AM_ABS },  // 8F (illegal)
          W_BADDR_ABS;
          break;

        case 0x91: // { "STA", AM_ZIY },  // 91
          W_BADDR_ZIY;
          break;

        case 0x94: // { "STY", AM_ZPX },  // 94
        case 0x95: // { "STA", AM_ZPX },  // 95
          W_BADDR_ZPX;
          break;

        case 0x96: // { "STX", AM_ZPY },  // 96
        case 0x97: // { "SAX", AM_ZPY },  // 97 (illegal)
          W_BADDR_ZPY;
          break;

        case 0x99: // { "STA", AM_ABY },  // 99
        case 0x9F: // { "AHX", AM_ABY },  // 9F (illegal, unstable)
        case 0x9B: // { "TAS", AM_ABY },  // 9B (illegal, unstable)
          W_BADDR_ABY;
          break;

        case 0x9D: // { "STA", AM_ABX },  // 9D
          W_BADDR_ABX;
          break;
      }

      for( i=0; i<16; i++ )
      {
        if( ( wlen != 0 ) && ( cpu->membreakpoints[i].flags & MBPF_WRITE ) )
        {
          if( ( cpu->membreakpoints[i].addr >= waddr ) &&
              ( cpu->membreakpoints[i].addr < (waddr+wlen) ) )
          {
            sprintf( bpmsg, "Break on WRITE to $%04X", cpu->membreakpoints[i].addr );
            return SDL_TRUE;
          }
        }

        if( ( rlen != 0 ) && ( cpu->membreakpoints[i].flags & MBPF_READ ) )
        {
          if( ( cpu->membreakpoints[i].addr >= raddr ) &&
              ( cpu->membreakpoints[i].addr < (raddr+rlen) ) )
          {
            sprintf( bpmsg, "Break on READ from $%04X", cpu->membreakpoints[i].addr );
            return SDL_TRUE;
          }
        }

        if( cpu->membreakpoints[i].flags & MBPF_CHANGE )
        {
          if( cpu->membreakpoints[i].lastval != cpu->read( cpu, cpu->membreakpoints[i].addr ) )
          {
            cpu->membreakpoints[i].lastval = cpu->read( cpu, cpu->membreakpoints[i].addr );
            sprintf( bpmsg, "Break after $%04X changed", cpu->membreakpoints[i].addr );
            return SDL_TRUE;
          }
        }
      }
    }
  }


  switch( cpu->calcop )
  {
    case 0x00: // { "BRK", AM_IMP },  // 00
    case 0x1E: // { "ASL", AM_ABX },  // 1E
    case 0x3E: // { "ROL", AM_ABX },  // 3E
    case 0x5E: // { "LSR", AM_ABX },  // 5E
    case 0x7E: // { "ROR", AM_ABX },  // 7E
    case 0xDE: // { "DEC", AM_ABX },  // DE
    case 0xFE: // { "INC", AM_ABX },  // FE
    case 0xDF: // { "DCP", AM_ABX },  // DF (illegal)
    case 0xDB: // { "DCP", AM_ZPY },  // DB (illegal)
    case 0xFF: // { "ISC", AM_ABX },  // FF (illegal)
    case 0xFB: // { "ISC", AM_ABY },  // FB (illegal)
    case 0x3F: // { "RLA", AM_ABX },  // 3F (illegal)
    case 0x3B: // { "RLA", AM_ABY },  // 3B (illegal)
    case 0x7F: // { "RRA", AM_ABX },  // 7F (illegal)
    case 0x7B: // { "RRA", AM_ABY },  // 7B (illegal)
    case 0x1F: // { "SLO", AM_ABX },  // 1F (illegal)
    case 0x1B: // { "SLO", AM_ABY },  // 1B (illegal)
      cpu->icycles = 7;
      break;

    case 0x01: // { "ORA", AM_ZIX },  // 01
    case 0x0E: // { "ASL", AM_ABS },  // 0E
    case 0x16: // { "ASL", AM_ZPX },  // 16
    case 0x20: // { "JSR", AM_ABS },  // 20
    case 0x21: // { "AND", AM_ZIX },  // 21
    case 0x2E: // { "ROL", AM_ABS },  // 2E
    case 0x36: // { "ROL", AM_ZPX },  // 36
    case 0x40: // { "RTI", AM_IMP },  // 40
    case 0x41: // { "EOR", AM_ZIX },  // 41
    case 0x4E: // { "LSR", AM_ABS },  // 4E
    case 0x56: // { "LSR", AM_ZPX },  // 56
    case 0x60: // { "RTS", AM_IMP },  // 60
    case 0x61: // { "ADC", AM_ZIX },  // 61
    case 0x6E: // { "ROR", AM_ABS },  // 6E
    case 0x76: // { "ROR", AM_ZPX },  // 76
    case 0x81: // { "STA", AM_ZIX },  // 81
    case 0x91: // { "STA", AM_ZIY },  // 91
    case 0xA1: // { "LDA", AM_ZIX },  // A1
    case 0xC1: // { "CMP", AM_ZIX },  // C1
    case 0xD6: // { "DEC", AM_ZPX },  // D6
    case 0xE1: // { "SBC", AM_ZIX },  // E1
    case 0xEE: // { "INC", AM_ABS },  // EE
    case 0xF6: // { "INC", AM_ZPX },  // F6
    case 0xCE: // { "DEC", AM_ABS },  // CE
    case 0x83: // { "SAX", AM_ZIX },  // 83 (illegal)
    case 0x93: // { "AHX", AM_ZIY },  // 93 (illegal, unstable)
    case 0xD7: // { "DCP", AM_ZPX },  // D7 (illegal)
    case 0xCF: // { "DCP", AM_ABS },  // CF (illegal)
    case 0xF7: // { "ISC", AM_ZPX },  // F7 (illegal)
    case 0xEF: // { "ISC", AM_ABS },  // EF (illegal)
    case 0xA3: // { "LAX", AM_ABY },  // A3 (illegal)
    case 0x37: // { "RLA", AM_ZPX },  // 37 (illegal)
    case 0x2F: // { "RLA", AM_ABS },  // 2F (illegal)
    case 0x77: // { "RRA", AM_ZPX },  // 77 (illegal)
    case 0x6F: // { "RRA", AM_ABS },  // 6F (illegal)
    case 0x17: // { "SLO", AM_ZPX },  // 17 (illegal)
    case 0x0F: // { "SLO", AM_ABS },  // 0F (illegal)
      cpu->icycles = 6;
      break;

    case 0x05: // { "ORA", AM_ZP  },  // 05
    case 0x08: // { "PHP", AM_IMP },  // 08
    case 0x24: // { "BIT", AM_ZP  },  // 24
    case 0x25: // { "AND", AM_ZP  },  // 25
    case 0x45: // { "EOR", AM_ZP  },  // 45
    case 0x48: // { "PHA", AM_IMP },  // 48
    case 0x4C: // { "JMP", AM_ABS },  // 4C
    case 0x65: // { "ADC", AM_ZP  },  // 65
    case 0x84: // { "STY", AM_ZP  },  // 84
    case 0x85: // { "STA", AM_ZP  },  // 85
    case 0x86: // { "STX", AM_ZP  },  // 86
    case 0xA4: // { "LDY", AM_ZP  },  // A4
    case 0xA5: // { "LDA", AM_ZP  },  // A5
    case 0xA6: // { "LDX", AM_ZP  },  // A6
    case 0xC4: // { "CPY", AM_ZP  },  // C4
    case 0xC5: // { "CMP", AM_ZP  },  // C5
    case 0xE4: // { "CPX", AM_ZP  },  // E4
    case 0xE5: // { "SBC", AM_ZP  },  // E5
    case 0x87: // { "SAX", AM_ZP  },  // 87 (illegal)
    case 0x04: // { "DOP", AM_ZP  },  // 04 (illegal)
    case 0x44: // { "DOP", AM_ZP  },  // 44 (illegal)
    case 0x64: // { "DOP", AM_ZP  },  // 64 (illegal)
    case 0xA7: // { "LAX", AM_ZP  },  // A7 (illegal)
      cpu->icycles = 3;
      break;

    case 0x06: // { "ASL", AM_ZP  },  // 06
    case 0x26: // { "ROL", AM_ZP  },  // 26
    case 0x46: // { "LSR", AM_ZP  },  // 46
    case 0x66: // { "ROR", AM_ZP  },  // 66
    case 0x6C: // { "JMP", AM_IND },  // 6C
    case 0x99: // { "STA", AM_ABY },  // 99
    case 0x9D: // { "STA", AM_ABX },  // 9D
    case 0xC6: // { "DEC", AM_ZP  },  // C6
    case 0xE6: // { "INC", AM_ZP  },  // E6
    case 0x9F: // { "AHX", AM_ABY },  // 9F (illegal, unstable)
    case 0xC7: // { "DCP", AM_ZP  },  // C7 (illegal)
    case 0xE7: // { "ISC", AM_ZP  },  // E7 (illegal)
    case 0x27: // { "RLA", AM_ZP  },  // 27 (illegal)
    case 0x67: // { "RRA", AM_ZP  },  // 67 (illegal)
    case 0x07: // { "SLO", AM_ZP  },  // 07 (illegal)
    case 0x9E: // { "SHX", AM_ABY },  // 9E (illegal, unstable)
    case 0x9C: // { "SHY", AM_ABX },  // 9C (illegal, unstable)
    case 0x9B: // { "TAS", AM_ABY },  // 9B (illegal, unstable)
      cpu->icycles = 5;
      break;

    case 0x09: // { "ORA", AM_IMM },  // 09
    case 0x0A: // { "ASL", AM_IMP },  // 0A
    case 0x18: // { "CLC", AM_IMP },  // 18
    case 0x29: // { "AND", AM_IMM },  // 29
    case 0x2A: // { "ROL", AM_IMP },  // 2A
    case 0x38: // { "SEC", AM_IMP },  // 38
    case 0x49: // { "EOR", AM_IMM },  // 49
    case 0x4A: // { "LSR", AM_IMP },  // 4A
    case 0x58: // { "CLI", AM_IMP },  // 58
    case 0x69: // { "ADC", AM_IMM },  // 69
    case 0x6A: // { "ROR", AM_IMP },  // 6A
    case 0x78: // { "SEI", AM_IMP },  // 78
    case 0x88: // { "DEY", AM_IMP },  // 88
    case 0x8A: // { "TXA", AM_IMP },  // 8A
    case 0x98: // { "TYA", AM_IMP },  // 98
    case 0x9A: // { "TXS", AM_IMP },  // 9A
    case 0xA0: // { "LDY", AM_IMM },  // A0
    case 0xA2: // { "LDX", AM_IMM },  // A2
    case 0xA8: // { "TAY", AM_IMP },  // A8
    case 0xA9: // { "LDA", AM_IMM },  // A9
    case 0xAA: // { "TAX", AM_IMP },  // AA
    case 0xB8: // { "CLV", AM_IMP },  // B8
    case 0xBA: // { "TSX", AM_IMP },  // BA
    case 0xC0: // { "CPY", AM_IMM },  // C0
    case 0xC8: // { "INY", AM_IMP },  // C8
    case 0xC9: // { "CMP", AM_IMM },  // C9
    case 0xCA: // { "DEX", AM_IMP },  // CA
    case 0xD8: // { "CLD", AM_IMP },  // D8
    case 0xE0: // { "CPX", AM_IMM },  // E0
    case 0xE8: // { "INX", AM_IMP },  // E8
    case 0xE9: // { "SBC", AM_IMM },  // E9
    case 0xEB: // { "SBC", AM_IMM },  // EB (illegal)
    case 0xEA: // { "NOP", AM_IMP },  // EA
    case 0xF8: // { "SED", AM_IMP },  // F8
    case 0x0B: // { "ANC", AM_IMM },  // 0B (illegal)
    case 0x2B: // { "ANC", AM_IMM },  // 2B (illegal)
    case 0x6B: // { "ARR", AM_IMM },  // 6B (illegal)
    case 0x4B: // { "ALR", AM_IMM },  // 4B (illegal)
    case 0xAB: // { "LAX", AM_IMM },  // AB (illegal, unstable)
    case 0xCB: // { "AXS", AM_IMM },  // CB (illegal)
    case 0x80: // { "DOP", AM_IMM },  // 80 (illegal)
    case 0x82: // { "DOP", AM_IMM },  // 82 (illegal)
    case 0x89: // { "DOP", AM_IMM },  // 89 (illegal)
    case 0xC2: // { "DOP", AM_IMM },  // C2 (illegal)
    case 0xE2: // { "DOP", AM_IMM },  // E2 (illegal)
    case 0x1A: // { "NOP", AM_IMP },  // 1A (illegal)
    case 0x3A: // { "NOP", AM_IMP },  // 3A (illegal)
    case 0x5A: // { "NOP", AM_IMP },  // 5A (illegal)
    case 0x7A: // { "NOP", AM_IMP },  // 7A (illegal)
    case 0xDA: // { "NOP", AM_IMP },  // DA (illegal)
    case 0xFA: // { "NOP", AM_IMP },  // FA (illegal)
    case 0x8B: // { "XAA", AM_IMM },  // 8B (illegal, unstable)
      cpu->icycles = 2;
      break;

    case 0x0D: // { "ORA", AM_ABS },  // 0D
    case 0x15: // { "ORA", AM_ZPX },  // 15
    case 0x28: // { "PLP", AM_IMP },  // 28
    case 0x2C: // { "BIT", AM_ABS },  // 2C
    case 0x2D: // { "AND", AM_ABS },  // 2D
    case 0x35: // { "AND", AM_ZPX },  // 35
    case 0x4D: // { "EOR", AM_ABS },  // 4D
    case 0x55: // { "EOR", AM_ZPX },  // 55
    case 0x6D: // { "ADC", AM_ABS },  // 6D
    case 0x75: // { "ADC", AM_ZPX },  // 75
    case 0x68: // { "PLA", AM_IMP },  // 68
    case 0x8C: // { "STY", AM_ABS },  // 8C
    case 0x8D: // { "STA", AM_ABS },  // 8D
    case 0x8E: // { "STX", AM_ABS },  // 8E
    case 0x94: // { "STY", AM_ZPX },  // 94
    case 0x95: // { "STA", AM_ZPX },  // 95
    case 0x96: // { "STX", AM_ZPY },  // 96
    case 0xAC: // { "LDY", AM_ABS },  // AC
    case 0xAD: // { "LDA", AM_ABS },  // AD
    case 0xAE: // { "LDX", AM_ABS },  // AE
    case 0xB4: // { "LDY", AM_ZPX },  // B4
    case 0xB5: // { "LDA", AM_ZPX },  // B5
    case 0xB6: // { "LDX", AM_ZPY },  // B6
    case 0xCC: // { "CPY", AM_ABS },  // CC
    case 0xCD: // { "CMP", AM_ABS },  // CD
    case 0xD5: // { "CMP", AM_ZPX },  // D5
    case 0xEC: // { "CPX", AM_ABS },  // EC
    case 0xED: // { "SBC", AM_ABS },  // ED
    case 0xF5: // { "SBC", AM_ZPX },  // F5
    case 0x97: // { "SAX", AM_ZPY },  // 97 (illegal)
    case 0x8F: // { "SAX", AM_ABS },  // 8F (illegal)
    case 0x14: // { "DOP", AM_ZPX },  // 14 (illegal)
    case 0x34: // { "DOP", AM_ZPX },  // 34 (illegal)
    case 0x54: // { "DOP", AM_ZPX },  // 54 (illegal)
    case 0x74: // { "DOP", AM_ZPX },  // 74 (illegal)
    case 0xD4: // { "DOP", AM_ZPX },  // D4 (illegal)
    case 0xF4: // { "DOP", AM_ZPX },  // F4 (illegal)
    case 0xB7: // { "LAX", AM_ZPY },  // B7 (illegal)
    case 0xAF: // { "LAX", AM_ABS },  // AF (illegal)
    case 0x0C: // { "TOP", AM_ABS },  // 0C (illegal)
      cpu->icycles = 4;
      break;

    case 0xC3: // { "DCP", AM_ZIX },  // C3 (illegal)
    case 0xD3: // { "DCP", AM_ZIY },  // D3 (illegal)
    case 0xE3: // { "ISC", AM_ZIY },  // E3 (illegal)
    case 0xF3: // { "ISC", AM_ZIY },  // F3 (illegal)
    case 0x23: // { "RLA", AM_ZIX },  // 23 (illegal)
    case 0x33: // { "RLA", AM_ZIY },  // 33 (illegal)
    case 0x63: // { "RRA", AM_ZIX },  // 63 (illegal)
    case 0x73: // { "RRA", AM_ZIY },  // 73 (illegal)
    case 0x03: // { "SLO", AM_ZIX },  // 03 (illegal)
    case 0x13: // { "SLO", AM_ZIY },  // 13 (illegal)
      cpu->icycles = 8;
      break;

    case 0x10: // { "BPL", AM_REL },  // 10
      IBRANCH( !cpu->f_n );
      break;

    case 0x11: // { "ORA", AM_ZIY },  // 11
    case 0x31: // { "AND", AM_ZIY },  // 31
    case 0x51: // { "EOR", AM_ZIY },  // 51
    case 0x71: // { "ADC", AM_ZIY },  // 71
    case 0xB1: // { "LDA", AM_ZIY },  // B1
    case 0xD1: // { "CMP", AM_ZIY },  // D1
    case 0xF1: // { "SBC", AM_ZIY },  // F1
    case 0xB3: // { "LAX", AM_ZIY },  // B3 (illegal)
      baddr = cpu->read( cpu, cpu->calcpc+1 );
      baddr = ((cpu->read( cpu, baddr+1 )<<8) | cpu->read( cpu, baddr ));
      cpu->icycles = 5;
      cpu->baddr = baddr+cpu->y;
      if( CPAGECHECK ) cpu->icycles++;
      break;

    case 0xBB: // { "LAS", AM_ZIY },  // BB
      baddr = cpu->read( cpu, cpu->calcpc+1 );
      baddr = ((cpu->read( cpu, baddr+1 )<<8) | cpu->read( cpu, baddr ));
      cpu->icycles = 4;
      cpu->baddr = baddr+cpu->y;
      if( CPAGECHECK ) cpu->icycles++;
      break;

    case 0x19: // { "ORA", AM_ABY },  // 19
    case 0x39: // { "AND", AM_ABY },  // 39
    case 0x59: // { "EOR", AM_ABY },  // 59
    case 0x79: // { "ADC", AM_ABY },  // 79
    case 0xB9: // { "LDA", AM_ABY },  // B9
    case 0xBE: // { "LDX", AM_ABY },  // BE
    case 0xD9: // { "CMP", AM_ABY },  // D9
    case 0xF9: // { "SBC", AM_ABY },  // F9
    case 0xBF: // { "LAX", AM_ABY },  // BF (illegal)
      NBADDR_ABS;
      cpu->icycles = 4;
      cpu->baddr = baddr+cpu->y;
      if( CPAGECHECK ) cpu->icycles++;
      break;    

    case 0x1D: // { "ORA", AM_ABX },  // 1D
    case 0x3D: // { "AND", AM_ABX },  // 3D
    case 0x5D: // { "EOR", AM_ABX },  // 5D
    case 0x7D: // { "ADC", AM_ABX },  // 7D
    case 0xBC: // { "LDY", AM_ABX },  // BC
    case 0xBD: // { "LDA", AM_ABX },  // BD
    case 0xDD: // { "CMP", AM_ABX },  // DD
    case 0xFD: // { "SBC", AM_ABX },  // FD
    case 0x1C: // { "TOP", AM_ABX },  // 1C (illegal)
    case 0x3C: // { "TOP", AM_ABX },  // 3C (illegal)
    case 0x5C: // { "TOP", AM_ABX },  // 5C (illegal)
    case 0x7C: // { "TOP", AM_ABX },  // 7C (illegal)
    case 0xDC: // { "TOP", AM_ABX },  // DC (illegal)
    case 0xFC: // { "TOP", AM_ABX },  // FC (illegal)
      NBADDR_ABS;
      cpu->icycles = 4;
      cpu->baddr = baddr+cpu->x;
      if( CPAGECHECK ) cpu->icycles++;
      break;    

    case 0x30: // { "BMI", AM_REL },  // 30
      IBRANCH( cpu->f_n );
      break;

    case 0x50: // { "BVC", AM_REL },  // 50
      IBRANCH( !cpu->f_v );
      break;

    case 0x70: // { "BVS", AM_REL },  // 70
      IBRANCH( cpu->f_v );
      break;
    
    case 0x90: // { "BCC", AM_REL },  // 90
      IBRANCH( !cpu->f_c );
      break;

    case 0xB0: // { "BCS", AM_REL },  // B0
      IBRANCH( cpu->f_c );
      break;

    case 0xD0: // { "BNE", AM_REL },  // D0
      IBRANCH( !cpu->f_z );
      break;

    case 0xF0: // { "BEQ", AM_REL },  // F0
      IBRANCH( cpu->f_z );
      break;

    default:
      cpu->icycles = 6;
      break;
  }
  
  cpu->icycles += extra;
  return SDL_FALSE;
}

// Execute one 6502 instruction
SDL_bool m6502_inst( struct m6502 *cpu )
{
  unsigned char v;
  unsigned short r, t, baddr;

  // Make sure you call set_icycles before this routine!
  cpu->cycles += cpu->icycles;

  if( cpu->calcint > 0 )
  {
    PUSHW( cpu->pc );
    PUSHB( MAKEFLAGSBC );
    cpu->f_d = 0;
    if( cpu->calcint == 2 )
    {
      cpu->nmi = SDL_FALSE;
    } else {
      cpu->f_i = 1;
    }
  }

  cpu->lastpc = cpu->pc = cpu->calcpc;
  cpu->pc++;
  switch( cpu->calcop )
  {
    case 0x00: // { "BRK", AM_IMP },  // 00
      PUSHW( (cpu->pc+1) );
      PUSHB( MAKEFLAGS | (1<<4) );   // Set B on the stack
      cpu->f_i = 1;
      cpu->f_d = 0;
      cpu->pc = (cpu->read( cpu, 0xffff )<<8) | cpu->read( cpu, 0xfffe );
      break;

    case 0x01: // { "ORA", AM_ZIX },  // 01
      READ_ZIX;
      DO_ORA;
      break;

    case 0x05: // { "ORA", AM_ZP  },  // 05
      READ_ZP;
      DO_ORA;
      break;

    case 0x06: // { "ASL", AM_ZP  },  // 06
      KREAD_ZP;
      DO_ASL(v);
      cpu->write( cpu, baddr, v );
      break;

    case 0x08: // { "PHP", AM_IMP },  // 08
      PUSHB( MAKEFLAGS );
      break;

    case 0x09: // { "ORA", AM_IMM },  // 09
      READ_IMM;
      DO_ORA;
      break;

    case 0x0A: // { "ASL", AM_IMP },  // 0A
      DO_ASL(cpu->a);
      break;

    case 0x0D: // { "ORA", AM_ABS },  // 0D
      READ_ABS;
      DO_ORA;
      break;

    case 0x0E: // { "ASL", AM_ABS },  // 0E
      KREAD_ABS;
      DO_ASL(v);
      cpu->write( cpu, baddr, v );
      break;

    case 0x10: // { "BPL", AM_REL },  // 10
      BRANCH( !cpu->f_n );
      break;

    case 0x11: // { "ORA", AM_ZIY },  // 11
      v = cpu->read( cpu, cpu->baddr );  // baddr is already calculated for this case
      cpu->pc++;
      DO_ORA;
      break;

    case 0x15: // { "ORA", AM_ZPX },  // 15
      READ_ZPX;
      DO_ORA;
      break;

    case 0x16: // { "ASL", AM_ZPX },  // 16
      KREAD_ZPX;
      DO_ASL(v);
      cpu->write( cpu, baddr, v );
      break;
    
    case 0x18: // { "CLC", AM_IMP },  // 18
      cpu->f_c = 0;
      break;

    case 0x19: // { "ORA", AM_ABY },  // 19
    case 0x1D: // { "ORA", AM_ABX },  // 1D
      v = cpu->read( cpu, cpu->baddr );  // baddr is already calculated for this case
      cpu->pc+=2;
      DO_ORA;
      break;    

    case 0x1E: // { "ASL", AM_ABX },  // 1E
      READ_ABX;
      DO_ASL(v);
      cpu->write( cpu, baddr, v );
      break;

    case 0x20: // { "JSR", AM_ABS },  // 20
      baddr = (cpu->read( cpu, cpu->pc+1 )<<8) | cpu->read( cpu, cpu->pc );
      PUSHW( (cpu->pc+1) );
      cpu->pc = baddr;
      break;

    case 0x21: // { "AND", AM_ZIX },  // 21
      READ_ZIX;
      DO_AND;
      break;
    
    case 0x24: // { "BIT", AM_ZP  },  // 24
      READ_ZP;
      cpu->f_n = v&0x80 ? 1 : 0;
      cpu->f_v = v&0x40 ? 1 : 0;
      cpu->f_z = (cpu->a&v)==0;
      break;

    case 0x25: // { "AND", AM_ZP  },  // 25
      READ_ZP;
      DO_AND;
      break;

    case 0x26: // { "ROL", AM_ZP  },  // 26
      KREAD_ZP;
      DO_ROL(v);
      cpu->write( cpu, baddr, v );
      break;

    case 0x28: // { "PLP", AM_IMP },  // 28
      v = POPB|(1<<4);
      SETFLAGS(v);
      break;

    case 0x29: // { "AND", AM_IMM },  // 29
      READ_IMM;
      DO_AND;
      break;
    
    case 0x2A: // { "ROL", AM_IMP },  // 2A
      DO_ROL(cpu->a);
      break;

    case 0x2C: // { "BIT", AM_ABS },  // 2C
      READ_ABS;
      cpu->f_n = v&0x80 ? 1 : 0;
      cpu->f_v = v&0x40 ? 1 : 0;
      cpu->f_z = (cpu->a&v)==0;
      break;

    case 0x2D: // { "AND", AM_ABS },  // 2D
      READ_ABS;
      DO_AND;
      break;
    
    case 0x2E: // { "ROL", AM_ABS },  // 2E
      KREAD_ABS;
      DO_ROL(v);
      cpu->write( cpu, baddr, v );
      break;

    case 0x30: // { "BMI", AM_REL },  // 30
      BRANCH( cpu->f_n );
      break;

    case 0x31: // { "AND", AM_ZIY },  // 31
      v = cpu->read( cpu, cpu->baddr );  // baddr is already calculated for this case
      cpu->pc++;
      DO_AND;
      break;

    case 0x35: // { "AND", AM_ZPX },  // 35
      READ_ZPX;
      DO_AND;
      break;

    case 0x36: // { "ROL", AM_ZPX },  // 36
      KREAD_ZPX;
      DO_ROL(v);
      cpu->write( cpu, baddr, v );
      break;

    case 0x38: // { "SEC", AM_IMP },  // 38
      cpu->f_c = 1;
      break;

    case 0x39: // { "AND", AM_ABY },  // 39
    case 0x3D: // { "AND", AM_ABX },  // 3D
      v = cpu->read( cpu, cpu->baddr );  // baddr is already calculated for this case
      cpu->pc+=2;
      DO_AND;
      break;    

    case 0x3E: // { "ROL", AM_ABX },  // 3E
      READ_ABX;
      DO_ROL(v);
      cpu->write( cpu, baddr, v );
      break;

    case 0x40: // { "RTI", AM_IMP },  // 40
      v = POPB;
      SETFLAGS(v);
      POPW( cpu->pc );
      break;

    case 0x41: // { "EOR", AM_ZIX },  // 41
      READ_ZIX;
      DO_EOR;
      break;

    case 0x45: // { "EOR", AM_ZP  },  // 45
      READ_ZP;
      DO_EOR;
      break;

    case 0x46: // { "LSR", AM_ZP  },  // 46
      KREAD_ZP;
      DO_LSR(v);
      cpu->write( cpu, baddr, v );
      break;

    case 0x48: // { "PHA", AM_IMP },  // 48
      PUSHB( cpu->a );
      break;

    case 0x49: // { "EOR", AM_IMM },  // 49
      READ_IMM;
      DO_EOR;
      break;

    case 0x4A: // { "LSR", AM_IMP },  // 4A
      DO_LSR(cpu->a);
      break;

    case 0x4C: // { "JMP", AM_ABS },  // 4C
      cpu->pc = (cpu->read( cpu, cpu->pc+1 )<<8)|cpu->read( cpu, cpu->pc );
      break;

    case 0x4D: // { "EOR", AM_ABS },  // 4D
      READ_ABS;
      DO_EOR;
      break;

    case 0x4E: // { "LSR", AM_ABS },  // 4E
      KREAD_ABS;
      DO_LSR(v);
      cpu->write( cpu, baddr, v );
      break;

    case 0x50: // { "BVC", AM_REL },  // 50
      BRANCH( !cpu->f_v );
      break;

    case 0x51: // { "EOR", AM_ZIY },  // 51
      v = cpu->read( cpu, cpu->baddr );  // baddr is already calculated for this case
      cpu->pc++;
      DO_EOR;
      break;

    case 0x55: // { "EOR", AM_ZPX },  // 55
      READ_ZPX;
      DO_EOR;
      break;

    case 0x56: // { "LSR", AM_ZPX },  // 56
      KREAD_ZPX;
      DO_LSR(v);
      cpu->write( cpu, baddr, v );
      break;

    case 0x58: // { "CLI", AM_IMP },  // 58
      cpu->f_i = 0;
      break;

    case 0x59: // { "EOR", AM_ABY },  // 59
    case 0x5D: // { "EOR", AM_ABX },  // 5D
      v = cpu->read( cpu, cpu->baddr );  // baddr is already calculated for this case
      cpu->pc+=2;
      DO_EOR;
      break;    

    case 0x5E: // { "LSR", AM_ABX },  // 5E
      READ_ABX;
      DO_LSR(v);
      cpu->write( cpu, baddr, v );
      break;

    case 0x60: // { "RTS", AM_IMP },  // 60
      POPW( cpu->pc );
      cpu->pc++;
      break;

    case 0x61: // { "ADC", AM_ZIX },  // 61
      READ_ZIX;
      DO_ADC;
      break;

    case 0x65: // { "ADC", AM_ZP  },  // 65
      READ_ZP;
      DO_ADC;
      break;

    case 0x66: // { "ROR", AM_ZP  },  // 66
      KREAD_ZP;
      DO_ROR(v);
      cpu->write( cpu, baddr, v );
      break;

    case 0x68: // { "PLA", AM_IMP },  // 68
      cpu->a = POPB;
      FLAG_ZN(cpu->a);
      break;

    case 0x69: // { "ADC", AM_IMM },  // 69
      READ_IMM;
      DO_ADC;
      break;
    
    case 0x6A: // { "ROR", AM_IMP },  // 6A
      DO_ROR(cpu->a);
      break;

    case 0x6C: // { "JMP", AM_IND },  // 6C
      baddr = (cpu->read( cpu, cpu->pc+1 )<<8)|cpu->read( cpu, cpu->pc );
      cpu->pc = (cpu->read( cpu, baddr+1 )<<8)|cpu->read( cpu, baddr );
      break;
      
    case 0x6D: // { "ADC", AM_ABS },  // 6D
      READ_ABS;
      DO_ADC;
      break;
    
    case 0x6E: // { "ROR", AM_ABS },  // 6E
      KREAD_ABS;
      DO_ROR(v);
      cpu->write( cpu, baddr, v );
      break;

    case 0x70: // { "BVS", AM_REL },  // 70
      BRANCH( cpu->f_v );
      break;
    
    case 0x71: // { "ADC", AM_ZIY },  // 71
      v = cpu->read( cpu, cpu->baddr );  // baddr is already calculated for this case
      cpu->pc++;
      DO_ADC;
      break;
    
    case 0x75: // { "ADC", AM_ZPX },  // 75
      READ_ZPX;
      DO_ADC;
      break;
    
    case 0x76: // { "ROR", AM_ZPX },  // 76
      KREAD_ZPX;
      DO_ROR(v);
      cpu->write( cpu, baddr, v );
      break;

    case 0x78: // { "SEI", AM_IMP },  // 78
      cpu->f_i = 1;
      break;

    case 0x79: // { "ADC", AM_ABY },  // 79
    case 0x7D: // { "ADC", AM_ABX },  // 7D
      v = cpu->read( cpu, cpu->baddr );  // baddr is already calculated for this case
      cpu->pc+=2;
      DO_ADC;
      break;    

    case 0x7E: // { "ROR", AM_ABX },  // 7E
      READ_ABX;
      DO_ROR(v);
      cpu->write( cpu, baddr, v );
      break;

    case 0x81: // { "STA", AM_ZIX },  // 81
      WRITE_ZIX(cpu->a);
      break;

    case 0x84: // { "STY", AM_ZP  },  // 84
      WRITE_ZP(cpu->y);
      break;

    case 0x85: // { "STA", AM_ZP  },  // 85
      WRITE_ZP(cpu->a);
      break;

    case 0x86: // { "STX", AM_ZP  },  // 86
      WRITE_ZP(cpu->x);
      break;

    case 0x88: // { "DEY", AM_IMP },  // 88
      cpu->y--;
      FLAG_ZN(cpu->y);
      break;

    case 0x8A: // { "TXA", AM_IMP },  // 8A
      cpu->a = cpu->x;
      FLAG_ZN(cpu->a);
      break;

    case 0x8C: // { "STY", AM_ABS },  // 8C
      WRITE_ABS(cpu->y);
      break;

    case 0x8D: // { "STA", AM_ABS },  // 8D
      WRITE_ABS(cpu->a);
      break;

    case 0x8E: // { "STX", AM_ABS },  // 8E
      WRITE_ABS(cpu->x);
      break;

    case 0x90: // { "BCC", AM_REL },  // 90
      BRANCH( !cpu->f_c );
      break;

    case 0x91: // { "STA", AM_ZIY },  // 91
      WRITE_ZIY(cpu->a);
      break;

    case 0x94: // { "STY", AM_ZPX },  // 94
      WRITE_ZPX(cpu->y);
      break;

    case 0x95: // { "STA", AM_ZPX },  // 95
      WRITE_ZPX(cpu->a);
      break;

    case 0x96: // { "STX", AM_ZPY },  // 96
      WRITE_ZPY(cpu->x);
      break;

    case 0x98: // { "TYA", AM_IMP },  // 98
      cpu->a = cpu->y;
      FLAG_ZN(cpu->a);
      break;

    case 0x99: // { "STA", AM_ABY },  // 99
      WRITE_ABY(cpu->a);
      break;

    case 0x9A: // { "TXS", AM_IMP },  // 9A
      cpu->sp = cpu->x;
      break;

    case 0x9D: // { "STA", AM_ABX },  // 9D
      WRITE_ABX(cpu->a);
      break;

    case 0xA0: // { "LDY", AM_IMM },  // A0
      READ_IMM;
      cpu->y = v;
      FLAG_ZN(cpu->y);
      break;

    case 0xA1: // { "LDA", AM_ZIX },  // A1
      READ_ZIX;
      cpu->a = v;
      FLAG_ZN(cpu->a);
      break;

    case 0xA2: // { "LDX", AM_IMM },  // A2
      READ_IMM;
      cpu->x = v;
      FLAG_ZN(cpu->x);
      break;
      
    case 0xA4: // { "LDY", AM_ZP  },  // A4
      READ_ZP;
      cpu->y = v;
      FLAG_ZN(cpu->y);
      break;

    case 0xA5: // { "LDA", AM_ZP  },  // A5
      READ_ZP;
      cpu->a = v;
      FLAG_ZN(cpu->a);
      break;

    case 0xA6: // { "LDX", AM_ZP  },  // A6
      READ_ZP;
      cpu->x = v;
      FLAG_ZN(cpu->x);
      break;

    case 0xA8: // { "TAY", AM_IMP },  // A8
      cpu->y = cpu->a;
      FLAG_ZN(cpu->y);
      break;

    case 0xA9: // { "LDA", AM_IMM },  // A9
      READ_IMM;
      cpu->a = v;
      FLAG_ZN(cpu->a);
      break;

    case 0xAA: // { "TAX", AM_IMP },  // AA
      cpu->x = cpu->a;
      FLAG_ZN(cpu->x);
      break;

    case 0xAC: // { "LDY", AM_ABS },  // AC
      READ_ABS;
      cpu->y = v;
      FLAG_ZN(cpu->y);
      break;

    case 0xAD: // { "LDA", AM_ABS },  // AD
      READ_ABS;
      cpu->a = v;
      FLAG_ZN(cpu->a);
      break;

    case 0xAE: // { "LDX", AM_ABS },  // AE
      READ_ABS;
      cpu->x = v;
      FLAG_ZN(cpu->x);
      break;

    case 0xB0: // { "BCS", AM_REL },  // B0
      BRANCH( cpu->f_c );
      break;

    case 0xB1: // { "LDA", AM_ZIY },  // B1
      cpu->a = cpu->read( cpu, cpu->baddr );  // baddr is already calculated for this case
      cpu->pc++;
      FLAG_ZN(cpu->a);
      break;

    case 0xB4: // { "LDY", AM_ZPX },  // B4
      READ_ZPX;
      cpu->y = v;
      FLAG_ZN(cpu->y);
      break;

    case 0xB5: // { "LDA", AM_ZPX },  // B5
      READ_ZPX;
      cpu->a = v;
      FLAG_ZN(cpu->a);
      break;

    case 0xB6: // { "LDX", AM_ZPY },  // B6
      READ_ZPY;
      cpu->x = v;
      FLAG_ZN(cpu->x);
      break;

    case 0xB8: // { "CLV", AM_IMP },  // B8
      cpu->f_v = 0;
      break;

    case 0xB9: // { "LDA", AM_ABY },  // B9
    case 0xBD: // { "LDA", AM_ABX },  // BD
      cpu->a = cpu->read( cpu, cpu->baddr );  // baddr is already calculated for this case
      cpu->pc+=2;
      FLAG_ZN(cpu->a);
      break;

    case 0xBA: // { "TSX", AM_IMP },  // BA
      cpu->x = cpu->sp;
      FLAG_ZN(cpu->x);
      break;

    case 0xBC: // { "LDY", AM_ABX },  // BC
      cpu->y = cpu->read( cpu, cpu->baddr );  // baddr is already calculated for this case
      cpu->pc+=2;
      FLAG_ZN(cpu->y);
      break;

    case 0xBE: // { "LDX", AM_ABY },  // BE
      cpu->x = cpu->read( cpu, cpu->baddr );  // baddr is already calculated for this case
      cpu->pc+=2;
      FLAG_ZN(cpu->x);
      break;

    case 0xC0: // { "CPY", AM_IMM },  // C0
      READ_IMM;
      r = cpu->y-v;
      FLAG_SZCN(r);
      break;
      
    case 0xC1: // { "CMP", AM_ZIX },  // C1
      READ_ZIX;
      r = cpu->a-v;
      FLAG_SZCN(r);
      break;

    case 0xC4: // { "CPY", AM_ZP  },  // C4
      READ_ZP;
      r = cpu->y-v;
      FLAG_SZCN(r);
      break;

    case 0xC5: // { "CMP", AM_ZP  },  // C5
      READ_ZP;
      r = cpu->a-v;
      FLAG_SZCN(r);
      break;

    case 0xC6: // { "DEC", AM_ZP  },  // C6
      KREAD_ZP;
      cpu->write( cpu, baddr, --v );
      FLAG_ZN(v);
      break;
     
    case 0xC8: // { "INY", AM_IMP },  // C8
      cpu->y++;
      FLAG_ZN(cpu->y);
      break;

    case 0xC9: // { "CMP", AM_IMM },  // C9
      READ_IMM;
      r = cpu->a-v;
      FLAG_SZCN(r);
      break;

    case 0xCA: // { "DEX", AM_IMP },  // CA
      cpu->x--;
      FLAG_ZN(cpu->x);
      break;

    case 0xCC: // { "CPY", AM_ABS },  // CC
      READ_ABS;
      r = cpu->y-v;
      FLAG_SZCN(r);
      break;

    case 0xCD: // { "CMP", AM_ABS },  // CD
      READ_ABS;
      r = cpu->a-v;
      FLAG_SZCN(r);
      break;

    case 0xCE: // { "DEC", AM_ABS },  // CE
      KREAD_ABS;
      cpu->write( cpu, baddr, --v );
      FLAG_ZN(v);
      break;
      
    case 0xD0: // { "BNE", AM_REL },  // D0
      BRANCH( !cpu->f_z );
      break;

    case 0xD1: // { "CMP", AM_ZIY },  // D1
      r = cpu->a-cpu->read( cpu, cpu->baddr );  // baddr is already calculated for this case
      cpu->pc++;
      FLAG_SZCN(r);
      break;

    case 0xD5: // { "CMP", AM_ZPX },  // D5
      READ_ZPX;
      r = cpu->a-v;
      FLAG_SZCN(r);
      break;
    
    case 0xD6: // { "DEC", AM_ZPX },  // D6
      KREAD_ZPX;
      cpu->write( cpu, baddr, --v );
      FLAG_ZN(v);
      break;

    case 0xD8: // { "CLD", AM_IMP },  // D8
      cpu->f_d = 0;
      break;

    case 0xD9: // { "CMP", AM_ABY },  // D9
    case 0xDD: // { "CMP", AM_ABX },  // DD
      r = cpu->a - cpu->read( cpu, cpu->baddr );  // baddr is already calculated for this case
      cpu->pc+=2;
      FLAG_SZCN(r);
      break;

    case 0xDE: // { "DEC", AM_ABX },  // DE
      READ_ABX;
      cpu->write( cpu, baddr, --v );
      FLAG_ZN(v);
      break;

    case 0xE0: // { "CPX", AM_IMM },  // E0
      READ_IMM;
      r = cpu->x-v;
      FLAG_SZCN(r);
      break;

    case 0xE1: // { "SBC", AM_ZIX },  // E1
      READ_ZIX;
      DO_SBC;
      break;

    case 0xE4: // { "CPX", AM_ZP  },  // E4
      READ_ZP;
      r = cpu->x-v;
      FLAG_SZCN(r);
      break;

    case 0xE5: // { "SBC", AM_ZP  },  // E5
      READ_ZP;
      DO_SBC;
      break;

    case 0xE6: // { "INC", AM_ZP  },  // E6
      KREAD_ZP;
      cpu->write( cpu, baddr, ++v );
      FLAG_ZN(v);
      break;

    case 0xE8: // { "INX", AM_IMP },  // E8
      cpu->x++;
      FLAG_ZN(cpu->x);
      break;

    case 0xE9: // { "SBC", AM_IMM },  // E9
    case 0xEB: // { "SBC", AM_IMM },  // EB (illegal)
      READ_IMM;
      DO_SBC;
      break;

    case 0xEA: // { "NOP", AM_IMP },  // EA
      break;

    case 0xEC: // { "CPX", AM_ABS },  // EC
      READ_ABS;
      r = cpu->x-v;
      FLAG_SZCN(r);
      break;

    case 0xED: // { "SBC", AM_ABS },  // ED
      READ_ABS;
      DO_SBC;
      break;

    case 0xEE: // { "INC", AM_ABS },  // EE
      KREAD_ABS;
      cpu->write( cpu, baddr, ++v );
      FLAG_ZN(v);
      break;

    case 0xF0: // { "BEQ", AM_REL },  // F0
      BRANCH( cpu->f_z );
      break;

    case 0xF1: // { "SBC", AM_ZIY },  // F1
      v = cpu->read( cpu, cpu->baddr );  // baddr is already calculated for this case
      cpu->pc++;
      DO_SBC;
      break;

    case 0xF5: // { "SBC", AM_ZPX },  // F5
      READ_ZPX;
      DO_SBC;
      break;

    case 0xF6: // { "INC", AM_ZPX },  // F6
      KREAD_ZPX;
      cpu->write( cpu, baddr, ++v );
      FLAG_ZN(v);
      break;

    case 0xF8: // { "SED", AM_IMP },  // F8
      cpu->f_d = 1;
      break;

    case 0xF9: // { "SBC", AM_ABY },  // F9
    case 0xFD: // { "SBC", AM_ABX },  // FD
      v = cpu->read( cpu, cpu->baddr );  // baddr is already calculated for this case
      cpu->pc+=2;
      DO_SBC;
      break;    

    case 0xFE: // { "INC", AM_ABX },  // FE
      READ_ABX;
      cpu->write( cpu, baddr, ++v );
      FLAG_ZN(v);
      break;

    // Illegal opcodes
    case 0x0B: // { "ANC", AM_IMM },  // 0B (illegal)
    case 0x2B: // { "ANC", AM_IMM },  // 2B (illegal)
      READ_IMM;
      DO_AND;
      cpu->f_c = cpu->f_n;
      break;
    
    case 0x87: // { "SAX", AM_ZP  },  // 87 (illegal)
      v = cpu->a & cpu->x;
      WRITE_ZP(v);
      cpu->f_n = (v&0x80)!=0;
      cpu->f_z = v!=0;
      break;

    case 0x97: // { "SAX", AM_ZPY },  // 97 (illegal)
      v = cpu->a & cpu->x;
      WRITE_ZPY(v);
      cpu->f_n = (v&0x80)!=0;
      cpu->f_z = v!=0;
      break;

    case 0x83: // { "SAX", AM_ZIX },  // 83 (illegal)
      v = cpu->a & cpu->x;
      WRITE_ZIX(v);
      cpu->f_n = (v&0x80)!=0;
      cpu->f_z = v!=0;
      break;

    case 0x8F: // { "SAX", AM_ABS },  // 8F (illegal)
      v = cpu->a & cpu->x;
      WRITE_ABS(v);
      cpu->f_n = (v&0x80)!=0;
      cpu->f_z = v!=0;
      break;

    case 0x6B: // { "ARR", AM_IMM },  // 6B (illegal)
      READ_IMM;
      cpu->a = (cpu->a&v)>>1;
      cpu->f_n = (cpu->a&0x80) != 0;
      cpu->f_z = cpu->a != 0;
      switch (cpu->a&0x60)
      {
        case 0x00: cpu->f_c=0; cpu->f_v=0; break;
        case 0x20: cpu->f_c=0; cpu->f_v=1; break;
        case 0x60: cpu->f_c=1; cpu->f_v=0; break;
        case 0x40: cpu->f_c=1; cpu->f_v=1; break;
      }
      break;

    case 0x4B: // { "ALR", AM_IMM },  // 4B (illegal)
      READ_IMM;
      cpu->a = (cpu->a&v)>>1;
      FLAG_ZCN(cpu->a);
      break;

    case 0xAB: // { "LAX", AM_IMM },  // AB (illegal, unstable)
      READ_IMM;
      DO_AND;
      cpu->x = cpu->a;
      break;

    case 0x93: // { "AHX", AM_ZIY },  // 93 (illegal, unstable)
    case 0x9F: // { "AHX", AM_ABY },  // 9F (illegal, unstable)
      r = cpu->x;

      // Instability 1 (sometimes, the &H drops off)
      if ((cpu->cycles&7)>2) // pseudorandom 5 in 8 chance of &H
        r &= (cpu->pc>>8)+1;
      
      v = cpu->a & r;
      
      // Instability 2 (A is input and output, which leads to "bit fight",
      // but anywhere that X&H=1 will work)
      if (((cpu->cycles>>8)&7)<2) // pseudorandom 2 in 8 chance of bit fight
      {
        r = (r^0xff)&cpu->a; // inverse R now has 1 for each bit that we can mess with
        r &= cpu->cycles;    // Make some of them 0 (pseudorandomly)
        v |= r;
      }
      if (cpu->calcop==0x9F)
      {
        WRITE_ABY(v);
      }
      else
      {
        WRITE_ZIY(v);
      }
      break;

    case 0xCB: // { "AXS", AM_IMM },  // CB (illegal)
      READ_IMM;
      cpu->x = (cpu->a&cpu->x)-v;
      FLAG_ZCN(cpu->x);
      break;

    case 0xC7: // { "DCP", AM_ZP  },  // C7 (illegal)
      BADDR_ZP;
      v = cpu->read(cpu, baddr);
      cpu->write(cpu, baddr, --v);
      WRITE_ZP(--v);
      FLAG_ZN(v);
      break;

    case 0xD7: // { "DCP", AM_ZPX },  // D7 (illegal)
      BADDR_ZPX;
      v = cpu->read(cpu, baddr);
      cpu->write(cpu, baddr, --v);
      FLAG_ZN(v);
      break;

    case 0xCF: // { "DCP", AM_ABS },  // CF (illegal)
      BADDR_ABS;
      v = cpu->read(cpu, baddr);
      cpu->write(cpu, baddr, --v);
      FLAG_ZN(v);
      break;

    case 0xDF: // { "DCP", AM_ABX },  // DF (illegal)
      BADDR_ABX;
      v = cpu->read(cpu, baddr);
      cpu->write(cpu, baddr, --v);
      FLAG_ZN(v);
      break;

    case 0xDB: // { "DCP", AM_ABY },  // DB (illegal)
      BADDR_ABY;
      v = cpu->read(cpu, baddr);
      cpu->write(cpu, baddr, --v);
      FLAG_ZN(v);
      break;

    case 0xC3: // { "DCP", AM_ZIX },  // C3 (illegal)
      BADDR_ZIX;
      v = cpu->read(cpu, baddr);
      cpu->write(cpu, baddr, --v);
      FLAG_ZN(v);
      break;

    case 0xD3: // { "DCP", AM_ZIY },  // D3 (illegal)
      BADDR_ZIY;
      v = cpu->read(cpu, baddr);
      cpu->write(cpu, baddr, --v);
      FLAG_ZN(v);
      break;

    case 0x04: // { "DOP", AM_IMM },  // 04 (illegal)
    case 0x14: // { "DOP", AM_IMM },  // 14 (illegal)
    case 0x34: // { "DOP", AM_IMM },  // 34 (illegal)
    case 0x44: // { "DOP", AM_IMM },  // 44 (illegal)
    case 0x54: // { "DOP", AM_IMM },  // 54 (illegal)
    case 0x64: // { "DOP", AM_IMM },  // 64 (illegal)
    case 0x74: // { "DOP", AM_IMM },  // 74 (illegal)
    case 0x80: // { "DOP", AM_IMM },  // 80 (illegal)
    case 0x82: // { "DOP", AM_IMM },  // 82 (illegal)
    case 0x89: // { "DOP", AM_IMM },  // 89 (illegal)
    case 0xC2: // { "DOP", AM_IMM },  // C2 (illegal)
    case 0xD4: // { "DOP", AM_IMM },  // D4 (illegal)
    case 0xE2: // { "DOP", AM_IMM },  // E2 (illegal)
    case 0xF4: // { "DOP", AM_IMM },  // F4 (illegal)
    case 0x1A: // { "NOP", AM_IMP },  // 1A (illegal)
    case 0x3A: // { "NOP", AM_IMP },  // 3A (illegal)
    case 0x5A: // { "NOP", AM_IMP },  // 5A (illegal)
    case 0x7A: // { "NOP", AM_IMP },  // 7A (illegal)
    case 0xDA: // { "NOP", AM_IMP },  // DA (illegal)
    case 0xFA: // { "NOP", AM_IMP },  // FA (illegal)
    case 0x0C: // { "TOP", AM_ABS },  // 0C (illegal)
    case 0x1C: // { "TOP", AM_ABX },  // 1C (illegal)
    case 0x3C: // { "TOP", AM_ABX },  // 3C (illegal)
    case 0x5C: // { "TOP", AM_ABX },  // 5C (illegal)
    case 0x7C: // { "TOP", AM_ABX },  // 7C (illegal)
    case 0xDC: // { "TOP", AM_ABX },  // DC (illegal)
    case 0xFC: // { "TOP", AM_ABX },  // FC (illegal)
      break;

    case 0xE7: // { "ISC", AM_ZP  },  // E7 (illegal)
      BADDR_ZP;
      v = cpu->read(cpu, baddr);
      cpu->write(cpu, baddr, ++v);
      r = (cpu->a - v) - (cpu->f_c^1);
      cpu->f_v = ((cpu->a^v)&(cpu->a^(r&0xff))&0x80) ? 1 : 0;
      FLAG_SZCN(r);
      cpu->a = r;
      cpu->pc++;
      break;

    case 0xF7: // { "ISC", AM_ZPX },  // F7 (illegal)
      BADDR_ZPX;
      v = cpu->read(cpu, baddr);
      cpu->write(cpu, baddr, ++v);
      r = (cpu->a - v) - (cpu->f_c^1);
      cpu->f_v = ((cpu->a^v)&(cpu->a^(r&0xff))&0x80) ? 1 : 0;
      FLAG_SZCN(r);
      cpu->a = r;
      cpu->pc++;
      break;

    case 0xEF: // { "ISC", AM_ABS },  // EF (illegal)
      BADDR_ABS;
      v = cpu->read(cpu, baddr);
      cpu->write(cpu, baddr, ++v);
      r = (cpu->a - v) - (cpu->f_c^1);
      cpu->f_v = ((cpu->a^v)&(cpu->a^(r&0xff))&0x80) ? 1 : 0;
      FLAG_SZCN(r);
      cpu->a = r;
      cpu->pc+=2;
      break;

    case 0xFF: // { "ISC", AM_ABX },  // FF (illegal)
      BADDR_ABX;
      v = cpu->read(cpu, baddr);
      cpu->write(cpu, baddr, ++v);
      r = (cpu->a - v) - (cpu->f_c^1);
      cpu->f_v = ((cpu->a^v)&(cpu->a^(r&0xff))&0x80) ? 1 : 0;
      FLAG_SZCN(r);
      cpu->a = r;
      cpu->pc+=2;
      break;

    case 0xFB: // { "ISC", AM_ABY },  // FB (illegal)
      BADDR_ABY;
      v = cpu->read(cpu, baddr);
      cpu->write(cpu, baddr, ++v);
      r = (cpu->a - v) - (cpu->f_c^1);
      cpu->f_v = ((cpu->a^v)&(cpu->a^(r&0xff))&0x80) ? 1 : 0;
      FLAG_SZCN(r);
      cpu->a = r;
      cpu->pc+=2;
      break;

    case 0xE3: // { "ISC", AM_ZIX },  // E3 (illegal)
      BADDR_ZIX;
      v = cpu->read(cpu, baddr);
      cpu->write(cpu, baddr, ++v);
      r = (cpu->a - v) - (cpu->f_c^1);
      cpu->f_v = ((cpu->a^v)&(cpu->a^(r&0xff))&0x80) ? 1 : 0;
      FLAG_SZCN(r);
      cpu->a = r;
      cpu->pc++;
      break;

    case 0xF3: // { "ISC", AM_ZIY },  // F3 (illegal)
      BADDR_ZIY;
      v = cpu->read(cpu, baddr);
      cpu->write(cpu, baddr, ++v);
      r = (cpu->a - v) - (cpu->f_c^1);
      cpu->f_v = ((cpu->a^v)&(cpu->a^(r&0xff))&0x80) ? 1 : 0;
      FLAG_SZCN(r);
      cpu->a = r;
      cpu->pc++;
      break;

    case 0xBB: // { "LAS", AM_ZIY },  // BB (illegal)
      cpu->sp &= cpu->read( cpu, cpu->baddr );
      cpu->pc++;
      cpu->a = cpu->sp;
      cpu->x = cpu->sp;
      FLAG_ZN(cpu->a);
      break;

    case 0xA7: // { "LAX", AM_ZP  },  // A7 (illegal)
      READ_ZP;
      cpu->a = v;
      cpu->x = v;
      FLAG_ZN(cpu->a);
      break;

    case 0xB7: // { "LAX", AM_ZPY },  // B7 (illegal)
      READ_ZPY;
      cpu->a = v;
      cpu->x = v;
      FLAG_ZN(cpu->a);
      break;

    case 0xAF: // { "LAX", AM_ABS },  // AF (illegal)
      READ_ABS;
      cpu->a = v;
      cpu->x = v;
      FLAG_ZN(cpu->a);
      break;

    case 0xBF: // { "LAX", AM_ABY },  // BF (illegal)
      cpu->a = cpu->read( cpu, cpu->baddr );  // baddr is already calculated for this case
      cpu->pc+=2;
      cpu->x = cpu->a;
      FLAG_ZN(cpu->a);
      break;

    case 0xA3: // { "LAX", AM_ZIX },  // A3 (illegal)
      READ_ZIX;
      cpu->a = v;
      cpu->x = v;
      FLAG_ZN(cpu->a);
      break;

    case 0xB3: // { "LAX", AM_ZIY },  // B3 (illegal)
      cpu->a = cpu->read( cpu, cpu->baddr );  // baddr is already calculated for this case
      cpu->x = cpu->a;
      cpu->pc++;
      FLAG_ZN(cpu->a);
      break;

    case 0x27: // { "RLA", AM_ZP  },  // 27 (illegal)
      BADDR_ZP;
      r = (cpu->read(cpu, baddr)<<1)|cpu->f_c;
      cpu->f_c = (r&0x100)!=0;
      cpu->write(cpu, baddr, r);
      cpu->a &= r;
      FLAG_ZN(cpu->a);
      cpu->pc++;
      break;

    case 0x37: // { "RLA", AM_ZPX },  // 37 (illegal)
      BADDR_ZPX;
      r = (cpu->read(cpu, baddr)<<1)|cpu->f_c;
      cpu->f_c = (r&0x100)!=0;
      cpu->write(cpu, baddr, r);
      cpu->a &= r;
      FLAG_ZN(cpu->a);
      cpu->pc++;
      break;

    case 0x2F: // { "RLA", AM_ABS },  // 2F (illegal)
      BADDR_ABS;
      r = (cpu->read(cpu, baddr)<<1)|cpu->f_c;
      cpu->f_c = (r&0x100)!=0;
      cpu->write(cpu, baddr, r);
      cpu->a &= r;
      FLAG_ZN(cpu->a);
      cpu->pc+=2;
      break;

    case 0x3F: // { "RLA", AM_ABX },  // 3F (illegal)
      BADDR_ABX;
      r = (cpu->read(cpu, baddr)<<1)|cpu->f_c;
      cpu->f_c = (r&0x100)!=0;
      cpu->write(cpu, baddr, r);
      cpu->a &= r;
      FLAG_ZN(cpu->a);
      cpu->pc+=2;
      break;

    case 0x3B: // { "RLA", AM_ABY },  // 3B (illegal)
      BADDR_ABY;
      r = (cpu->read(cpu, baddr)<<1)|cpu->f_c;
      cpu->f_c = (r&0x100)!=0;
      cpu->write(cpu, baddr, r);
      cpu->a &= r;
      FLAG_ZN(cpu->a);
      cpu->pc+=2;
      break;

    case 0x23: // { "RLA", AM_ZIX },  // 23 (illegal)
      BADDR_ZIX;
      r = (cpu->read(cpu, baddr)<<1)|cpu->f_c;
      cpu->f_c = (r&0x100)!=0;
      cpu->write(cpu, baddr, r);
      cpu->a &= r;
      FLAG_ZN(cpu->a);
      cpu->pc++;
      break;

    case 0x33: // { "RLA", AM_ZIY },  // 33 (illegal)
      BADDR_ZIY;
      r = (cpu->read(cpu, baddr)<<1)|cpu->f_c;
      cpu->f_c = (r&0x100)!=0;
      cpu->write(cpu, baddr, r);
      cpu->a &= r;
      FLAG_ZN(cpu->a);
      cpu->pc++;
      break;

    case 0x67: // { "RRA", AM_ZP  },  // 67 (illegal)
      BADDR_ZP;
      r = cpu->read(cpu, baddr);
      v = (r>>1)|(cpu->f_c<<7);
      cpu->f_c = r&1;
      cpu->write(cpu, baddr, v);
      r = cpu->a + v + cpu->f_c;
      cpu->f_v = ((cpu->a^v)&(cpu->a^(r&0xff))&0x80) ? 1 : 0;
      FLAG_ZCN(r);
      cpu->a = r;
      cpu->pc++;
      break;

    case 0x77: // { "RRA", AM_ZPX },  // 77 (illegal)
      BADDR_ZPX;
      r = cpu->read(cpu, baddr);
      v = (r>>1)|(cpu->f_c<<7);
      cpu->f_c = r&1;
      cpu->write(cpu, baddr, v);
      r = cpu->a + v + cpu->f_c;
      cpu->f_v = ((cpu->a^v)&(cpu->a^(r&0xff))&0x80) ? 1 : 0;
      FLAG_ZCN(r);
      cpu->a = r;
      cpu->pc++;
      break;

    case 0x6F: // { "RRA", AM_ABS },  // 6F (illegal)
      BADDR_ABS;
      r = cpu->read(cpu, baddr);
      v = (r>>1)|(cpu->f_c<<7);
      cpu->f_c = r&1;
      cpu->write(cpu, baddr, v);
      r = cpu->a + v + cpu->f_c;
      cpu->f_v = ((cpu->a^v)&(cpu->a^(r&0xff))&0x80) ? 1 : 0;
      FLAG_ZCN(r);
      cpu->a = r;
      cpu->pc+=2;
      break;

    case 0x7F: // { "RRA", AM_ABX },  // 7F (illegal)
      BADDR_ABX;
      r = cpu->read(cpu, baddr);
      v = (r>>1)|(cpu->f_c<<7);
      cpu->f_c = r&1;
      cpu->write(cpu, baddr, v);
      r = cpu->a + v + cpu->f_c;
      cpu->f_v = ((cpu->a^v)&(cpu->a^(r&0xff))&0x80) ? 1 : 0;
      FLAG_ZCN(r);
      cpu->a = r;
      cpu->pc+=2;
      break;

    case 0x7B: // { "RRA", AM_ABY },  // 7B (illegal)
      BADDR_ABY;
      r = cpu->read(cpu, baddr);
      v = (r>>1)|(cpu->f_c<<7);
      cpu->f_c = r&1;
      cpu->write(cpu, baddr, v);
      r = cpu->a + v + cpu->f_c;
      cpu->f_v = ((cpu->a^v)&(cpu->a^(r&0xff))&0x80) ? 1 : 0;
      FLAG_ZCN(r);
      cpu->a = r;
      cpu->pc+=2;
      break;

    case 0x63: // { "RRA", AM_ZIX },  // 63 (illegal)
      BADDR_ZIX;
      r = cpu->read(cpu, baddr);
      v = (r>>1)|(cpu->f_c<<7);
      cpu->f_c = r&1;
      cpu->write(cpu, baddr, v);
      r = cpu->a + v + cpu->f_c;
      cpu->f_v = ((cpu->a^v)&(cpu->a^(r&0xff))&0x80) ? 1 : 0;
      FLAG_ZCN(r);
      cpu->a = r;
      cpu->pc++;
      break;

    case 0x73: // { "RRA", AM_ZIY },  // 73 (illegal)
      BADDR_ZIY;
      r = cpu->read(cpu, baddr);
      v = (r>>1)|(cpu->f_c<<7);
      cpu->f_c = r&1;
      cpu->write(cpu, baddr, v);
      r = cpu->a + v + cpu->f_c;
      cpu->f_v = ((cpu->a^v)&(cpu->a^(r&0xff))&0x80) ? 1 : 0;
      FLAG_ZCN(r);
      cpu->a = r;
      cpu->pc++;
      break;

    case 0x07: // { "SLO", AM_ZP  },  // 07 (illegal)
      BADDR_ZP;
      r = cpu->read(cpu, baddr)<<1;
      cpu->f_c = (r&0x100)!=0;
      cpu->write(cpu, baddr, r);
      cpu->a |= r;
      FLAG_ZN(cpu->a);
      cpu->pc++;
      break;

    case 0x17: // { "SLO", AM_ZPX },  // 17 (illegal)
      BADDR_ZPX;
      r = cpu->read(cpu, baddr)<<1;
      cpu->f_c = (r&0x100)!=0;
      cpu->write(cpu, baddr, r);
      cpu->a |= r;
      FLAG_ZN(cpu->a);
      cpu->pc++;
      break;

    case 0x0F: // { "SLO", AM_ABS },  // 0F (illegal)
      BADDR_ABS;
      r = cpu->read(cpu, baddr)<<1;
      cpu->f_c = (r&0x100)!=0;
      cpu->write(cpu, baddr, r);
      cpu->a |= r;
      FLAG_ZN(cpu->a);
      cpu->pc+=2;
      break;

    case 0x1F: // { "SLO", AM_ABX },  // 1F (illegal)
      BADDR_ABX;
      r = cpu->read(cpu, baddr)<<1;
      cpu->f_c = (r&0x100)!=0;
      cpu->write(cpu, baddr, r);
      cpu->a |= r;
      FLAG_ZN(cpu->a);
      cpu->pc+=2;
      break;

    case 0x1B: // { "SLO", AM_ABY },  // 1B (illegal)
      BADDR_ABY;
      r = cpu->read(cpu, baddr)<<1;
      cpu->f_c = (r&0x100)!=0;
      cpu->write(cpu, baddr, r);
      cpu->a |= r;
      FLAG_ZN(cpu->a);
      cpu->pc+=2;
      break;

    case 0x03: // { "SLO", AM_ZIX },  // 03 (illegal)
      BADDR_ZIX;
      r = cpu->read(cpu, baddr)<<1;
      cpu->f_c = (r&0x100)!=0;
      cpu->write(cpu, baddr, r);
      cpu->a |= r;
      FLAG_ZN(cpu->a);
      cpu->pc++;
      break;

    case 0x13: // { "SLO", AM_ZIY },  // 13 (illegal)
      BADDR_ZIY;
      r = cpu->read(cpu, baddr)<<1;
      cpu->f_c = (r&0x100)!=0;
      cpu->write(cpu, baddr, r);
      cpu->a |= r;
      FLAG_ZN(cpu->a);
      cpu->pc++;
      break;

    case 0x47: // { "SRE", AM_ZP  },  // 47 (illegal)
      BADDR_ZP;
      v = cpu->read(cpu, baddr);
      cpu->f_c = v&1;
      v >>= 1;
      cpu->write(cpu, baddr, v);
      cpu->a ^= v;
      FLAG_ZN(cpu->a);
      cpu->pc++;
      break;

    case 0x57: // { "SRE", AM_ZPX },  // 57 (illegal)
      BADDR_ZPX;
      v = cpu->read(cpu, baddr);
      cpu->f_c = v&1;
      v >>= 1;
      cpu->write(cpu, baddr, v);
      cpu->a ^= v;
      FLAG_ZN(cpu->a);
      cpu->pc++;
      break;

    case 0x4F: // { "SRE", AM_ABS },  // 4F (illegal)
      BADDR_ABS;
      v = cpu->read(cpu, baddr);
      cpu->f_c = v&1;
      v >>= 1;
      cpu->write(cpu, baddr, v);
      cpu->a ^= v;
      FLAG_ZN(cpu->a);
      cpu->pc+=2;
      break;

    case 0x5F: // { "SRE", AM_ABX },  // 5F (illegal)
      BADDR_ABX;
      v = cpu->read(cpu, baddr);
      cpu->f_c = v&1;
      v >>= 1;
      cpu->write(cpu, baddr, v);
      cpu->a ^= v;
      FLAG_ZN(cpu->a);
      cpu->pc+=2;
      break;

    case 0x5B: // { "SRE", AM_ABY },  // 5B (illegal)
      BADDR_ABY;
      v = cpu->read(cpu, baddr);
      cpu->f_c = v&1;
      v >>= 1;
      cpu->write(cpu, baddr, v);
      cpu->a ^= v;
      FLAG_ZN(cpu->a);
      cpu->pc+=2;
      break;

    case 0x43: // { "SRE", AM_ZIX },  // 43 (illegal)
      BADDR_ZIX;
      v = cpu->read(cpu, baddr);
      cpu->f_c = v&1;
      v >>= 1;
      cpu->write(cpu, baddr, v);
      cpu->a ^= v;
      FLAG_ZN(cpu->a);
      cpu->pc++;
      break;

    case 0x53: // { "SRE", AM_ZIY },  // 53 (illegal)
      BADDR_ZIY;
      v = cpu->read(cpu, baddr);
      cpu->f_c = v&1;
      v >>= 1;
      cpu->write(cpu, baddr, v);
      cpu->a ^= v;
      FLAG_ZN(cpu->a);
      cpu->pc++;
      break;

    case 0x9E: // { "SHX", AM_ABY },  // 9E (illegal, unstable)
      BADDR_ABY;
      v = cpu->x;

      // Instability (sometimes, the &H drops off)
      if ((cpu->cycles&7)>2) // pseudorandom 5 in 8 chance of &H
        v &= (baddr>>8)+1;

      cpu->write(cpu, baddr, v);
      cpu->pc+=2;
      break;

    case 0x9C: // { "SHY", AM_ABX },  // 9C (illegal, unstable)
      BADDR_ABX;
      v = cpu->y;

      // Instability (sometimes, the &H drops off)
      if ((cpu->cycles&7)>2) // pseudorandom 5 in 8 chance of &H
        v &= (baddr>>8)+1;

      cpu->write(cpu, baddr, v);
      cpu->pc+=2;
      break;

    case 0x8B: // { "XAA", AM_IMM },  // 8B (illegal, unstable)
      READ_IMM;
      
      cpu->a = cpu->x & v;

      // Instability
      if ((cpu->cycles&7)>5) // pseudorandom 6 in 8 chance of instability
      {
        r = (cpu->x^0xff);
        r &= cpu->cycles;
        cpu->a |= r;
      }
      FLAG_ZN(cpu->a);
      break;

    case 0x9B: // { "TAS", AM_ABY },  // 9B (illegal)
      BADDR_ABY;
      cpu->sp = cpu->x & cpu->a;
      cpu->write(cpu, baddr, cpu->sp&((baddr>>8)+1));
      cpu->pc+=2;
      break;

    default: // JAM
      cpu->pc = cpu->lastpc;
      return SDL_TRUE; // jammed
  }
  
  return SDL_FALSE;
}

