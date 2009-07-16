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
**  6502 emulation
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <SDL/SDL.h>

#include "6502.h"

// These are the default read/write routines.
// You need to overwrite these with your own ones if you want
// the CPU to do anything other than constantly execute BRK
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
void m6502_init( struct m6502 *cpu, void *userdata )
{
  int i;
  cpu->rastercycles = 0;
  cpu->icycles = 0;
#if COUNTCYCLES
  cpu->cycles = 0;
#endif
  cpu->write = nullwrite;
  cpu->read  = nullread;

  for( i=0; i<16; i++ )
    cpu->breakpoints[i] = -1;
  cpu->anybp = SDL_FALSE;
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
  cpu->nmi = SDL_FALSE;
  cpu->irql = SDL_FALSE;
  cpu->irq = 0;
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
               } else {\
                 r = cpu->a + v + cpu->f_c;\
                 cpu->f_v = (~(cpu->a^v)&(cpu->a^(r&0xff))&0x80) ? 1 : 0;\
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
                 cpu->f_c = (r>15) ? 0 : 1;\
               } else {\
                 r = (cpu->a - v) - (cpu->f_c^1);\
                 cpu->f_v = (~(cpu->a^v)&(cpu->a^(r&0xff))&0x80) ? 1 : 0;\
                 FLAG_SZCN(r);\
                 cpu->a = r;\
               }

// Macros for each addressing mode of the 6502
#define READ_IMM v=cpu->read( cpu, cpu->pc++ )
#define READ_ZP  v=cpu->read( cpu, cpu->read( cpu, cpu->pc++ ) )
#define READ_ZPX v=cpu->read( cpu, (cpu->read( cpu, cpu->pc++ ) + cpu->x)&0xff )
#define READ_ZPY v=cpu->read( cpu, (cpu->read( cpu, cpu->pc++ ) + cpu->y)&0xff )
#define READ_ABS v=cpu->read( cpu, (cpu->read( cpu, cpu->pc+1 )<<8) | cpu->read( cpu, cpu->pc ) ); cpu->pc+=2
#define READ_ABX baddr = ((cpu->read( cpu, cpu->pc+1 )<<8) | cpu->read( cpu, cpu->pc )); v = cpu->read( cpu, baddr + cpu->x ); cpu->pc+=2
#define READ_ABY baddr = ((cpu->read( cpu, cpu->pc+1 )<<8) | cpu->read( cpu, cpu->pc )); v = cpu->read( cpu, baddr + cpu->y ); cpu->pc+=2
#define READ_ZIX baddr = (unsigned char)(cpu->read( cpu, cpu->pc++ )+cpu->x); v = cpu->read( cpu, (cpu->read( cpu, baddr+1 )<<8) | cpu->read( cpu, baddr ) )
#define READ_ZIY baddr = cpu->read( cpu, cpu->pc++ ); baddr = (cpu->read( cpu, baddr+1 )<<8) | cpu->read( cpu, baddr ); v = cpu->read( cpu, baddr + cpu->y )

#define KREAD_ZP  baddr = cpu->read( cpu, cpu->pc++ ); v = cpu->read( cpu, baddr )
#define KREAD_ZPX baddr = (unsigned char)(cpu->read( cpu, cpu->pc++ )+cpu->x); v = cpu->read( cpu, baddr )
#define KREAD_ABS baddr = (cpu->read( cpu, cpu->pc+1 )<<8) | cpu->read( cpu, cpu->pc ); v=cpu->read( cpu, baddr ); cpu->pc+=2
#define KREAD_ABX baddr = ((cpu->read( cpu, cpu->pc+1 )<<8) | cpu->read( cpu, cpu->pc ))+cpu->x; v=cpu->read( cpu, baddr ); cpu->pc+=2

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

// Page check to see if a branch takes you out of the current page
#define BPAGECHECK ( (baddr&0xff00) != (cpu->pc&0xff00) )

// Macro to perform branch logic
#define BRANCH(condition) offs = (signed char)cpu->read( cpu, cpu->pc++ );\
                          if( condition )\
                          {\
                            cycleit( cpu, 1 );\
                            baddr = cpu->pc+offs;\
                            if( BPAGECHECK ) cycleit( cpu, 1 );\
                            cpu->pc = baddr;\
                          }\
                          cycleit( cpu, 2 )

// Macros to simplify pushing and popping
#define PUSHB(n) cpu->write( cpu, (cpu->sp--)+0x100, n )
#define POPB cpu->read( cpu, (++cpu->sp)+0x100 )
#define PUSHW(n) PUSHB( n>>8 ); PUSHB( n )
#define POPW(n)  n = (cpu->read(cpu,cpu->sp+0x102)<<8)|cpu->read(cpu,cpu->sp+0x101); cpu->sp+=2

// Merge the seperate flag stores into a 6502 status register form
#define MAKEFLAGS ((cpu->f_n<<7)|(cpu->f_v<<6)|(1<<5)|(cpu->f_b<<4)|(cpu->f_d<<3)|(cpu->f_i<<2)|(cpu->f_z<<1)|cpu->f_c)

// Set the seperate flag stores from a 6502-format mask
#define SETFLAGS(n) cpu->f_n=(n&0x80)>>7;\
                    cpu->f_v=(n&0x40)>>6;\
                    cpu->f_b=(n&0x10)>>4;\
                    cpu->f_d=(n&0x08)>>3;\
                    cpu->f_i=(n&0x04)>>2;\
                    cpu->f_z=(n&0x02)>>1;\
                    cpu->f_c=n&0x01

// Bump the CPU cycle counts 
static inline void cycleit( struct m6502 *cpu, int cycles )
{
  cpu->rastercycles -= cycles;
  cpu->icycles += cycles;
#if CYCLECOUNT
  cpu->cycles += cycles;
#endif
}

// Execute one 6502 instruction
// Returns TRUE if we're about to execute the instruction
// at an active breakpoint
SDL_bool m6502_inst( struct m6502 *cpu, SDL_bool dobp )
{
  unsigned char op, v;
  signed char offs;
  unsigned short r, t, baddr;
  int i;

  cpu->icycles = 0;

  if( ( dobp ) && ( cpu->anybp ) )
  {
    for( i=0; i<16; i++ )
    {
      if( ( cpu->breakpoints[i] != -1 ) && ( cpu->pc == cpu->breakpoints[i] ) )
        return SDL_TRUE;
    }
  }

  if( ( cpu->nmi ) ||
      ( ( (cpu->irql)||(cpu->irq) ) && ( cpu->f_i == 0 ) ) )
  {
    cycleit( cpu, 7 );
    PUSHW( cpu->pc );
    PUSHB( MAKEFLAGS );
    cpu->f_d = 0;
    if( cpu->nmi )
    {
      cpu->pc = (cpu->read( cpu, 0xfffb )<<8)|cpu->read( cpu, 0xfffa );
      cpu->nmi = SDL_FALSE;
    } else {
      cpu->pc = (cpu->read( cpu, 0xffff )<<8)|cpu->read( cpu, 0xfffe );
      cpu->irql = SDL_FALSE;
      cpu->f_i = 1;
    }
  }

  op = cpu->read( cpu, cpu->pc++ );
  switch( op )
  {
    case 0x00: // { "BRK", AM_IMP },  // 00
      PUSHW( cpu->pc );
      PUSHB( MAKEFLAGS );
      cpu->f_i = 1;
      cpu->f_d = 0;
      cpu->pc = (cpu->read( cpu, 0xffff )<<8) | cpu->read( cpu, 0xfffe );
      break;

    case 0x01: // { "ORA", AM_ZIX },  // 01
      READ_ZIX;
      DO_ORA;
      cycleit( cpu, 6 );
      break;

    case 0x05: // { "ORA", AM_ZP  },  // 05
      READ_ZP;
      DO_ORA;
      cycleit( cpu, 3 );
      break;

    case 0x06: // { "ASL", AM_ZP  },  // 06
      KREAD_ZP;
      DO_ASL(v);
      cpu->write( cpu, baddr, v );
      cycleit( cpu, 5 );
      break;

    case 0x08: // { "PHP", AM_IMP },  // 08
      PUSHB( MAKEFLAGS );
      cycleit( cpu, 3 );
      break;

    case 0x09: // { "ORA", AM_IMM },  // 09
      READ_IMM;
      DO_ORA;
      cycleit( cpu, 2 );
      break;

    case 0x0A: // { "ASL", AM_IMP },  // 0A
      DO_ASL(cpu->a);
      cycleit( cpu, 2 );
      break;

    case 0x0D: // { "ORA", AM_ABS },  // 0D
      READ_ABS;
      DO_ORA;
      cycleit( cpu, 4 );
      break;

    case 0x0E: // { "ASL", AM_ABS },  // 0E
      KREAD_ABS;
      DO_ASL(v);
      cpu->write( cpu, baddr, v );
      cycleit( cpu, 6 );
      break;

    case 0x10: // { "BPL", AM_REL },  // 10
      BRANCH( !cpu->f_n );
      break;

    case 0x11: // { "ORA", AM_ZIY },  // 11
      READ_ZIY;
      DO_ORA;
      cycleit( cpu, 5 );
      if( PAGECHECK( cpu->y ) ) cycleit( cpu, 1 );
      break;

    case 0x15: // { "ORA", AM_ZPX },  // 15
      READ_ZPX;
      DO_ORA;
      cycleit( cpu, 4 );
      break;

    case 0x16: // { "ASL", AM_ZPX },  // 16
      KREAD_ZPX;
      DO_ASL(v);
      cpu->write( cpu, baddr, v );
      cycleit( cpu, 6 );
      break;
    
    case 0x18: // { "CLC", AM_IMP },  // 18
      cpu->f_c = 0;
      cycleit( cpu, 2 );
      break;

    case 0x19: // { "ORA", AM_ABY },  // 19
      READ_ABY;
      DO_ORA;
      cycleit( cpu, 4 );
      if( PAGECHECK( cpu->y ) ) cycleit( cpu, 1 );
      break;    

    case 0x1D: // { "ORA", AM_ABX },  // 1D
      READ_ABX;
      DO_ORA;
      cycleit( cpu, 4 );
      if( PAGECHECK( cpu->x ) ) cycleit( cpu, 1 );
      break;    

    case 0x1E: // { "ASL", AM_ABX },  // 1E
      KREAD_ABX;
      DO_ASL(v);
      cpu->write( cpu, baddr, v );
      cycleit( cpu, 7 );
      break;

    case 0x20: // { "JSR", AM_ABS },  // 20
      baddr = (cpu->read( cpu, cpu->pc+1 )<<8) | cpu->read( cpu, cpu->pc );
      PUSHW( (cpu->pc+1) );
      cpu->pc = baddr;
      cycleit( cpu, 6 );
      break;

    case 0x21: // { "AND", AM_ZIX },  // 21
      READ_ZIX;
      DO_AND;
      cycleit( cpu, 6 );
      break;
    
    case 0x24: // { "BIT", AM_ZP  },  // 24
      READ_ZP;
      cpu->f_n = v&0x80 ? 1 : 0;
      cpu->f_v = v&0x40 ? 1 : 0;
      cpu->f_z = (cpu->a&v)==0;
      cycleit( cpu, 3 );
      break;

    case 0x25: // { "AND", AM_ZP  },  // 25
      READ_ZP;
      DO_AND;
      cycleit( cpu, 3 );
      break;

    case 0x26: // { "ROL", AM_ZP  },  // 26
      KREAD_ZP;
      DO_ROL(v);
      cpu->write( cpu, baddr, v );
      cycleit( cpu, 5 );
      break;

    case 0x28: // { "PLP", AM_IMP },  // 28
      v = POPB;
      SETFLAGS(v);
      cycleit( cpu, 4 );
      break;

    case 0x29: // { "AND", AM_IMM },  // 29
      READ_IMM;
      DO_AND;
      cycleit( cpu, 2 );
      break;
    
    case 0x2A: // { "ROL", AM_IMP },  // 2A
      DO_ROL(cpu->a);
      cycleit( cpu, 2 );
      break;

    case 0x2C: // { "BIT", AM_ABS },  // 2C
      READ_ABS;
      cpu->f_n = v&0x80 ? 1 : 0;
      cpu->f_v = v&0x40 ? 1 : 0;
      cpu->f_z = (cpu->a&v)==0;
      cycleit( cpu, 3 );
      break;

    case 0x2D: // { "AND", AM_ABS },  // 2D
      READ_ABS;
      DO_AND;
      cycleit( cpu, 4 );
      break;
    
    case 0x2E: // { "ROL", AM_ABS },  // 2E
      KREAD_ABS;
      DO_ROL(v);
      cpu->write( cpu, baddr, v );
      cycleit( cpu, 6 );
      break;

    case 0x30: // { "BMI", AM_REL },  // 30
      BRANCH( cpu->f_n );
      break;

    case 0x31: // { "AND", AM_ZIY },  // 31
      READ_ZIY;
      DO_AND;
      cycleit( cpu, 5 );
      if( PAGECHECK( cpu->y ) ) cycleit( cpu, 1 );
      break;

    case 0x35: // { "AND", AM_ZPX },  // 35
      READ_ZPX;
      DO_AND;
      cycleit( cpu, 4 );
      break;

    case 0x36: // { "ROL", AM_ZPX },  // 36
      KREAD_ZPX;
      DO_ROL(v);
      cpu->write( cpu, baddr, v );
      cycleit( cpu, 6 );
      break;

    case 0x38: // { "SEC", AM_IMP },  // 38
      cpu->f_c = 1;
      cycleit( cpu, 2 );
      break;

    case 0x39: // { "AND", AM_ABY },  // 39
      READ_ABY;
      DO_AND;
      cycleit( cpu, 4 );
      if( PAGECHECK( cpu->y ) ) cycleit( cpu, 1 );
      break;    

    case 0x3D: // { "AND", AM_ABX },  // 3D
      READ_ABX;
      DO_AND;
      cycleit( cpu, 4 );
      if( PAGECHECK( cpu->x ) ) cycleit( cpu, 1 );
      break;    

    case 0x3E: // { "ROL", AM_ABX },  // 3E
      KREAD_ABX;
      DO_ROL(v);
      cpu->write( cpu, baddr, v );
      cycleit( cpu, 7 );
      break;

    case 0x40: // { "RTI", AM_IMP },  // 40
      v = POPB;
      SETFLAGS(v);
      POPW( cpu->pc );
      break;

    case 0x41: // { "EOR", AM_ZIX },  // 41
      READ_ZIX;
      DO_EOR;
      cycleit( cpu, 6 );
      break;

    case 0x45: // { "EOR", AM_ZP  },  // 45
      READ_ZP;
      DO_EOR;
      cycleit( cpu, 3 );
      break;

    case 0x46: // { "LSR", AM_ZP  },  // 46
      KREAD_ZP;
      DO_LSR(v);
      cpu->write( cpu, baddr, v );
      cycleit( cpu, 5 );
      break;

    case 0x48: // { "PHA", AM_IMP },  // 48
      PUSHB( cpu->a );
      cycleit( cpu, 3 );
      break;

    case 0x49: // { "EOR", AM_IMM },  // 49
      READ_IMM;
      DO_EOR;
      cycleit( cpu, 2 );
      break;

    case 0x4A: // { "LSR", AM_IMP },  // 4A
      DO_LSR(cpu->a);
      cycleit( cpu, 2 );
      break;

    case 0x4C: // { "JMP", AM_ABS },  // 4C
      cpu->pc = (cpu->read( cpu, cpu->pc+1 )<<8)|cpu->read( cpu, cpu->pc );
      cycleit( cpu, 3 );
      break;

    case 0x4D: // { "EOR", AM_ABS },  // 4D
      READ_ABS;
      DO_EOR;
      cycleit( cpu, 4 );
      break;

    case 0x4E: // { "LSR", AM_ABS },  // 4E
      KREAD_ABS;
      DO_LSR(v);
      cpu->write( cpu, baddr, v );
      cycleit( cpu, 6 );
      break;

    case 0x50: // { "BVC", AM_REL },  // 50
      BRANCH( !cpu->f_v );
      break;

    case 0x51: // { "EOR", AM_ZIY },  // 51
      READ_ZIY;
      DO_EOR;
      cycleit( cpu, 5 );
      if( PAGECHECK( cpu->y ) ) cycleit( cpu, 1 );
      break;

    case 0x55: // { "EOR", AM_ZPX },  // 55
      READ_ZPX;
      DO_EOR;
      cycleit( cpu, 4 );
      break;

    case 0x56: // { "LSR", AM_ZPX },  // 56
      KREAD_ZPX;
      DO_LSR(v);
      cpu->write( cpu, baddr, v );
      cycleit( cpu, 6 );
      break;

    case 0x58: // { "CLI", AM_IMP },  // 58
      cpu->f_i = 0;
      cycleit( cpu, 2 );
      break;

    case 0x59: // { "EOR", AM_ABY },  // 59
      READ_ABY;
      DO_EOR;
      cycleit( cpu, 4 );
      if( PAGECHECK( cpu->y ) ) cycleit( cpu, 1 );
      break;    

    case 0x5D: // { "EOR", AM_ABX },  // 5D
      READ_ABX;
      DO_EOR;
      cycleit( cpu, 4 );
      if( PAGECHECK( cpu->x ) ) cycleit( cpu, 1 );
      break;    

    case 0x5E: // { "LSR", AM_ABX },  // 5E
      KREAD_ABX;
      DO_LSR(v);
      cpu->write( cpu, baddr, v );
      cycleit( cpu, 7 );
      break;

    case 0x60: // { "RTS", AM_IMP },  // 60
      POPW( cpu->pc );
      cpu->pc++;
      cycleit( cpu, 6 );
      break;

    case 0x61: // { "ADC", AM_ZIX },  // 61
      READ_ZIX;
      DO_ADC;
      cycleit( cpu, 6 );
      break;

    case 0x65: // { "ADC", AM_ZP  },  // 65
      READ_ZP;
      DO_ADC;
      cycleit( cpu, 3 );
      break;

    case 0x66: // { "ROR", AM_ZP  },  // 66
      KREAD_ZP;
      DO_ROR(v);
      cpu->write( cpu, baddr, v );
      cycleit( cpu, 5 );
      break;

    case 0x68: // { "PLA", AM_IMP },  // 68
      cpu->a = POPB;
      FLAG_ZN(cpu->a);
      cycleit( cpu, 4 );
      break;

    case 0x69: // { "ADC", AM_IMM },  // 69
      READ_IMM;
      DO_ADC;
      cycleit( cpu, 2 );
      break;
    
    case 0x6A: // { "ROR", AM_IMP },  // 6A
      DO_ROR(cpu->a);
      cycleit( cpu, 2 );
      break;

    case 0x6C: // { "JMP", AM_IND },  // 6C
      baddr = (cpu->read( cpu, cpu->pc+1 )<<8)|cpu->read( cpu, cpu->pc );
      cpu->pc = (cpu->read( cpu, baddr+1 )<<8)|cpu->read( cpu, baddr );
      cycleit( cpu, 5 );
      break;
      
    case 0x6D: // { "ADC", AM_ABS },  // 6D
      READ_ABS;
      DO_ADC;
      cycleit( cpu, 4 );
      break;
    
    case 0x6E: // { "ROR", AM_ABS },  // 6E
      KREAD_ABS;
      DO_ROR(v);
      cpu->write( cpu, baddr, v );
      cycleit( cpu, 6 );
      break;

    case 0x70: // { "BVS", AM_REL },  // 70
      BRANCH( cpu->f_v );
      break;
    
    case 0x71: // { "ADC", AM_ZIY },  // 71
      READ_ZIY;
      DO_ADC;
      cycleit( cpu, 5 );
      if( PAGECHECK( cpu->y ) ) cycleit( cpu, 1 );
      break;
    
    case 0x75: // { "ADC", AM_ZPX },  // 75
      READ_ZPX;
      DO_ADC;
      cycleit( cpu, 4 );
      break;
    
    case 0x76: // { "ROR", AM_ZPX },  // 76
      KREAD_ZPX;
      DO_ROR(v);
      cpu->write( cpu, baddr, v );
      cycleit( cpu, 6 );
      break;

    case 0x78: // { "SEI", AM_IMP },  // 78
      cpu->f_i = 1;
      cycleit( cpu, 2 );
      break;

    case 0x79: // { "ADC", AM_ABY },  // 79
      READ_ABY;
      DO_ADC;
      cycleit( cpu, 4 );
      if( PAGECHECK( cpu->y ) ) cycleit( cpu, 1 );
      break;    

    case 0x7D: // { "ADC", AM_ABX },  // 7D
      READ_ABX;
      DO_ADC;
      cycleit( cpu, 4 );
      if( PAGECHECK( cpu->x ) ) cycleit( cpu, 1 );
      break;
    
    case 0x7E: // { "ROR", AM_ABX },  // 7E
      KREAD_ABX;
      DO_ROR(v);
      cpu->write( cpu, baddr, v );
      cycleit( cpu, 7 );
      break;

    case 0x81: // { "STA", AM_ZIX },  // 81
      WRITE_ZIX(cpu->a);
      cycleit( cpu, 6 );
      break;

    case 0x84: // { "STY", AM_ZP  },  // 84
      WRITE_ZP(cpu->y);
      cycleit( cpu, 3 );
      break;

    case 0x85: // { "STA", AM_ZP  },  // 85
      WRITE_ZP(cpu->a);
      cycleit( cpu, 3 );
      break;

    case 0x86: // { "STX", AM_ZP  },  // 86
      WRITE_ZP(cpu->x);
      cycleit( cpu, 3 );
      break;

    case 0x88: // { "DEY", AM_IMP },  // 88
      cpu->y--;
      FLAG_ZN(cpu->y);
      cycleit( cpu, 2 );
      break;

    case 0x8A: // { "TXA", AM_IMP },  // 8A
      cpu->a = cpu->x;
      FLAG_ZN(cpu->a);
      cycleit( cpu, 2 );
      break;

    case 0x8C: // { "STY", AM_ABS },  // 8C
      WRITE_ABS(cpu->y);
      cycleit( cpu, 4 );
      break;

    case 0x8D: // { "STA", AM_ABS },  // 8D
      WRITE_ABS(cpu->a);
      cycleit( cpu, 4 );
      break;

    case 0x8E: // { "STX", AM_ABS },  // 8E
      WRITE_ABS(cpu->x);
      cycleit( cpu, 4 );
      break;

    case 0x90: // { "BCC", AM_REL },  // 90
      BRANCH( !cpu->f_c );
      break;

    case 0x91: // { "STA", AM_ZIY },  // 91
      WRITE_ZIY(cpu->a);
      cycleit( cpu, 6 );
      break;

    case 0x94: // { "STY", AM_ZPX },  // 94
      WRITE_ZPX(cpu->y);
      cycleit( cpu, 4 );
      break;

    case 0x95: // { "STA", AM_ZPX },  // 95
      WRITE_ZPX(cpu->a);
      cycleit( cpu, 4 );
      break;

    case 0x96: // { "STX", AM_ZPY },  // 96
      WRITE_ZPY(cpu->x);
      cycleit( cpu, 4 );
      break;

    case 0x98: // { "TYA", AM_IMP },  // 98
      cpu->a = cpu->y;
      FLAG_ZN(cpu->a);
      cycleit( cpu, 2 );
      break;

    case 0x99: // { "STA", AM_ABY },  // 99
      WRITE_ABY(cpu->a);
      cycleit( cpu, 5 );
      break;

    case 0x9A: // { "TXS", AM_IMP },  // 9A
      cpu->sp = cpu->x;
      FLAG_ZN(cpu->sp);
      cycleit( cpu, 2 );
      break;

    case 0x9D: // { "STA", AM_ABX },  // 9D
      WRITE_ABX(cpu->a);
      cycleit( cpu, 5 );
      break;

    case 0xA0: // { "LDY", AM_IMM },  // A0
      READ_IMM;
      cpu->y = v;
      FLAG_ZN(cpu->y);
      cycleit( cpu, 2 );
      break;

    case 0xA1: // { "LDA", AM_ZIX },  // A1
      READ_ZIX;
      cpu->a = v;
      FLAG_ZN(cpu->a);
      cycleit( cpu, 6 );
      break;

    case 0xA2: // { "LDX", AM_IMM },  // A2
      READ_IMM;
      cpu->x = v;
      FLAG_ZN(cpu->x);
      cycleit( cpu, 2 );
      break;
      
    case 0xA4: // { "LDY", AM_ZP  },  // A4
      READ_ZP;
      cpu->y = v;
      FLAG_ZN(cpu->y);
      cycleit( cpu, 3 );
      break;

    case 0xA5: // { "LDA", AM_ZP  },  // A5
      READ_ZP;
      cpu->a = v;
      FLAG_ZN(cpu->a);
      cycleit( cpu, 3 );
      break;

    case 0xA6: // { "LDX", AM_ZP  },  // A6
      READ_ZP;
      cpu->x = v;
      FLAG_ZN(cpu->x);
      cycleit( cpu, 3 );
      break;

    case 0xA8: // { "TAY", AM_IMP },  // A8
      cpu->y = cpu->a;
      FLAG_ZN(cpu->y);
      cycleit( cpu, 2 );
      break;

    case 0xA9: // { "LDA", AM_IMM },  // A9
      READ_IMM;
      cpu->a = v;
      FLAG_ZN(cpu->a);
      cycleit( cpu, 2 );
      break;

    case 0xAA: // { "TAX", AM_IMP },  // AA
      cpu->x = cpu->a;
      FLAG_ZN(cpu->x);
      cycleit( cpu, 2 );
      break;

    case 0xAC: // { "LDY", AM_ABS },  // AC
      READ_ABS;
      cpu->y = v;
      FLAG_ZN(cpu->y);
      cycleit( cpu, 4 );
      break;

    case 0xAD: // { "LDA", AM_ABS },  // AD
      READ_ABS;
      cpu->a = v;
      FLAG_ZN(cpu->a);
      cycleit( cpu, 4 );
      break;

    case 0xAE: // { "LDX", AM_ABS },  // AE
      READ_ABS;
      cpu->x = v;
      FLAG_ZN(cpu->x);
      cycleit( cpu, 4 );
      break;

    case 0xB0: // { "BCS", AM_REL },  // B0
      BRANCH( cpu->f_c );
      break;

    case 0xB1: // { "LDA", AM_ZIY },  // B1
      READ_ZIY;
      cpu->a = v;
      FLAG_ZN(cpu->a);
      cycleit( cpu, 5 );
      if( PAGECHECK(cpu->y) ) cycleit( cpu, 1 );
      break;

    case 0xB4: // { "LDY", AM_ZPX },  // B4
      READ_ZPX;
      cpu->y = v;
      FLAG_ZN(cpu->y);
      cycleit( cpu, 4 );
      break;

    case 0xB5: // { "LDA", AM_ZPX },  // B5
      READ_ZPX;
      cpu->a = v;
      FLAG_ZN(cpu->a);
      cycleit( cpu, 4 );
      break;

    case 0xB6: // { "LDX", AM_ZPY },  // B6
      READ_ZPY;
      cpu->x = v;
      FLAG_ZN(cpu->x);
      cycleit( cpu, 4 );
      break;

    case 0xB8: // { "CLV", AM_IMP },  // B8
      cpu->f_v = 0;
      cycleit( cpu, 2 );
      break;

    case 0xB9: // { "LDA", AM_ABY },  // B9
      READ_ABY;
      cpu->a = v;
      FLAG_ZN(cpu->a);
      cycleit( cpu, 4 );
      if( PAGECHECK(cpu->y) ) cycleit( cpu, 1 );
      break;

    case 0xBA: // { "TSX", AM_IMP },  // BA
      cpu->x = cpu->sp;
      FLAG_ZN(cpu->x);
      cycleit( cpu, 2 );
      break;

    case 0xBC: // { "LDY", AM_ABX },  // BC
      READ_ABX;
      cpu->y = v;
      FLAG_ZN(cpu->y);
      cycleit( cpu, 4 );
      if( PAGECHECK(cpu->x) ) cycleit( cpu, 1 );
      break;

    case 0xBD: // { "LDA", AM_ABX },  // BD
      READ_ABX;
      cpu->a = v;
      FLAG_ZN(cpu->a);
      cycleit( cpu, 4 );
      if( PAGECHECK(cpu->x) ) cycleit( cpu, 1 );
      break;

    case 0xBE: // { "LDX", AM_ABY },  // BE
      READ_ABY;
      cpu->x = v;
      FLAG_ZN(cpu->x);
      cycleit( cpu, 4 );
      if( PAGECHECK(cpu->x) ) cycleit( cpu, 1 );
      break;

    case 0xC0: // { "CPY", AM_IMM },  // C0
      READ_IMM;
      FLAG_SZCN(cpu->y-v);
      cycleit( cpu, 2 );
      break;
      
    case 0xC1: // { "CMP", AM_ZIX },  // C1
      READ_ZIX;
      FLAG_SZCN(cpu->a-v);
      cycleit( cpu, 6 );
      break;

    case 0xC4: // { "CPY", AM_ZP  },  // C4
      READ_ZP;
      FLAG_SZCN(cpu->y-v);
      cycleit( cpu, 3 );
      break;

    case 0xC5: // { "CMP", AM_ZP  },  // C5
      READ_ZP;
      FLAG_SZCN(cpu->a-v);
      cycleit( cpu, 3 );
      break;

    case 0xC6: // { "DEC", AM_ZP  },  // C6
      KREAD_ZP;
      cpu->write( cpu, baddr, --v );
      FLAG_ZN(v);
      cycleit( cpu, 5 );
      break;
     
    case 0xC8: // { "INY", AM_IMP },  // C8
      cpu->y++;
      FLAG_ZN(cpu->y);
      cycleit( cpu, 2 );
      break;

    case 0xC9: // { "CMP", AM_IMM },  // C9
      READ_IMM;
      FLAG_SZCN(cpu->a-v);
      cycleit( cpu, 2 );
      break;

    case 0xCA: // { "DEX", AM_IMP },  // CA
      cpu->x--;
      FLAG_ZN(cpu->x);
      cycleit( cpu, 2 );
      break;

    case 0xCC: // { "CPY", AM_ABS },  // CC
      READ_ABS;
      FLAG_SZCN(cpu->y-v);
      cycleit( cpu, 4 );
      break;

    case 0xCD: // { "CMP", AM_ABS },  // CD
      READ_ABS;
      FLAG_SZCN(cpu->a-v);
      cycleit( cpu, 4 );
      break;

    case 0xCE: // { "DEC", AM_ABS },  // CE
      KREAD_ABS;
      cpu->write( cpu, baddr, --v );
      FLAG_ZN(v);
      cycleit( cpu, 3 );
      break;
      
    case 0xD0: // { "BNE", AM_REL },  // D0
      BRANCH( !cpu->f_z );
      break;

    case 0xD1: // { "CMP", AM_ZIY },  // D1
      READ_ZIY;
      FLAG_SZCN(cpu->a-v);
      cycleit( cpu, 5 );
      if( PAGECHECK( cpu->y ) ) cycleit( cpu, 1 );
      break;

    case 0xD5: // { "CMP", AM_ZPX },  // D5
      READ_ZPX;
      FLAG_SZCN(cpu->a-v);
      cycleit( cpu, 4 );
      break;
    
    case 0xD6: // { "DEC", AM_ZPX },  // D6
      KREAD_ZPX;
      cpu->write( cpu, baddr, --v );
      FLAG_ZN(v);
      cycleit( cpu, 6 );
      break;

    case 0xD8: // { "CLD", AM_IMP },  // D8
      cpu->f_d = 0;
      cycleit( cpu, 2 );
      break;

    case 0xD9: // { "CMP", AM_ABY },  // D9
      READ_ABY;
      FLAG_SZCN(cpu->a-v);
      cycleit( cpu, 4 );
      if( PAGECHECK( cpu->y ) ) cycleit( cpu, 1 );
      break;

    case 0xDD: // { "CMP", AM_ABX },  // DD
      READ_ABX;
      FLAG_SZCN(cpu->a-v);
      cycleit( cpu, 4 );
      if( PAGECHECK( cpu->x ) ) cycleit( cpu, 1 );
      break;

    case 0xDE: // { "DEC", AM_ABX },  // DE
      KREAD_ABX;
      cpu->write( cpu, baddr, --v );
      FLAG_ZN(v);
      cycleit( cpu, 7 );
      break;

    case 0xE0: // { "CPX", AM_IMM },  // E0
      READ_IMM;
      FLAG_SZCN(cpu->x-v);
      cycleit( cpu, 2 );
      break;

    case 0xE1: // { "SBC", AM_ZIX },  // E1
      READ_ZIX;
      DO_SBC;
      cycleit( cpu, 6 );
      break;

    case 0xE4: // { "CPX", AM_ZP  },  // E4
      READ_ZP;
      FLAG_SZCN(cpu->x-v);
      cycleit( cpu, 3 );
      break;

    case 0xE5: // { "SBC", AM_ZP  },  // E5
      READ_ZP;
      DO_SBC;
      cycleit( cpu, 3 );
      break;

    case 0xE6: // { "INC", AM_ZP  },  // E6
      KREAD_ZP;
      cpu->write( cpu, baddr, ++v );
      FLAG_ZN(v);
      cycleit( cpu, 5 );
      break;

    case 0xE8: // { "INX", AM_IMP },  // E8
      cpu->x++;
      FLAG_ZN(cpu->x);
      cycleit( cpu, 2 );
      break;

    case 0xE9: // { "SBC", AM_IMM },  // E9
      READ_IMM;
      DO_SBC;
      cycleit( cpu, 2 );
      break;

    case 0xEA: // { "NOP", AM_IMP },  // EA
      cycleit( cpu, 2 );
      break;

    case 0xEC: // { "CPX", AM_ABS },  // EC
      READ_ABS;
      FLAG_SZCN(cpu->x-v);
      cycleit( cpu, 4 );
      break;

    case 0xED: // { "SBC", AM_ABS },  // ED
      READ_ABS;
      DO_SBC;
      cycleit( cpu, 4 );
      break;

    case 0xEE: // { "INC", AM_ABS },  // EE
      KREAD_ABS;
      cpu->write( cpu, baddr, ++v );
      FLAG_ZN(v);
      cycleit( cpu, 6 );
      break;

    case 0xF0: // { "BEQ", AM_REL },  // F0
      BRANCH( cpu->f_z );
      break;

    case 0xF1: // { "SBC", AM_ZIY },  // F1
      READ_ZIY;
      DO_SBC;
      cycleit( cpu, 5 );
      if( PAGECHECK( cpu->y ) ) cycleit( cpu, 1 );
      break;

    case 0xF5: // { "SBC", AM_ZPX },  // F5
      READ_ZPX;
      DO_SBC;
      cycleit( cpu, 4 );
      break;

    case 0xF6: // { "INC", AM_ZPX },  // F6
      KREAD_ZPX;
      cpu->write( cpu, baddr, ++v );
      FLAG_ZN(v);
      cycleit( cpu, 6 );
      break;

    case 0xF8: // { "SED", AM_IMP },  // F8
      cpu->f_d = 1;
      cycleit( cpu, 2 );
      break;

    case 0xF9: // { "SBC", AM_ABY },  // F9
      READ_ABY;
      DO_SBC;
      cycleit( cpu, 4 );
      if( PAGECHECK( cpu->y ) ) cycleit( cpu, 1 );
      break;    

    case 0xFD: // { "SBC", AM_ABX },  // FD
      READ_ABX;
      DO_SBC;
      cycleit( cpu, 4 );
      if( PAGECHECK( cpu->x ) ) cycleit( cpu, 1 );
      break;

    case 0xFE: // { "INC", AM_ABX },  // FE
      KREAD_ABX;
      cpu->write( cpu, baddr, ++v );
      FLAG_ZN(v);
      cycleit( cpu, 7 );
      break;
  }

  return SDL_FALSE;
}
