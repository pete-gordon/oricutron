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
**  6502 emulation header
*/

// Set to TRUE to count all CPU cycles since the emulation was started
#define CYCLECOUNT 1

// 6502 flag bits
#define FB_C 0
#define FF_C (1<<FB_C)
#define FB_Z 1
#define FF_Z (1<<FB_Z)
#define FB_I 2
#define FF_I (1<<FB_I)
#define FB_D 3
#define FF_D (1<<FB_D)
#define FB_B 4
#define FF_B (1<<FB_B)
#define FB_R 5
#define FF_R (1<<FB_R)
#define FB_V 6
#define FF_V (1<<FB_V)
#define FB_N 7
#define FF_N (1<<FB_N)

// IRQ sources
#define IRQB_VIA 0
#define IRQF_VIA (1<<IRQB_VIA)
#define IRQB_DISK 1
#define IRQF_DISK (1<<IRQB_DISK)

// Memory access breakpoints
#define MBPB_READ 0
#define MBPF_READ (1<<MBPB_READ)
#define MBPB_WRITE 1
#define MBPF_WRITE (1<<MBPB_WRITE)
#define MBPB_CHANGE 2
#define MBPF_CHANGE (1<<MBPB_CHANGE)

struct membreakpoint
{
  unsigned char  flags;
  unsigned char  lastval;
  unsigned short addr;
};

struct m6502
{
  int rastercycles;
  unsigned int icycles;
#if CYCLECOUNT
  int cycles;
#endif
  unsigned char a, x, y, sp;
  unsigned char f_c, f_z, f_i, f_d, f_b, f_v, f_n;
  unsigned short pc;
  SDL_bool nmi, irql;
  int irq;
  void (*write)(struct m6502 *,unsigned short, unsigned char);
  unsigned char (*read)(struct m6502 *,unsigned short);
  SDL_bool anybp, anymbp;
  int breakpoints[16];
  struct membreakpoint membreakpoints[16];
  void *userdata;
};

void m6502_init( struct m6502 *cpu, void *userdata, SDL_bool nukebreakpoints );
void m6502_reset( struct m6502 *cpu );
SDL_bool m6502_inst( struct m6502 *cpu, SDL_bool dobp, char *bpmsg );
