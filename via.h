/*
**  Oricutron
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

#define TAPE_0_PULSE 416
#define TAPE_1_PULSE 208

struct via
{
  // CPU accessible registers
  unsigned char ifr;
  unsigned char irb, orb, irbl;
  unsigned char ira, ora, iral;
  unsigned char ddra;
  unsigned char ddrb;
  unsigned char t1l_l;
  unsigned char t1l_h;
  unsigned short t1c;
  unsigned char t2l_l;
  unsigned char t2l_h;  
  unsigned short t2c;
  unsigned char sr;
  unsigned char acr;
  unsigned char pcr;
  unsigned char ier;

  // Pins and ports
  unsigned char ca1, ca2;
  unsigned char cb1, cb2;
  unsigned char srcount;
  unsigned char t1reload;
  Sint16 srtime;

  // Internal state stuff
  SDL_bool t1run, t2run;
  SDL_bool ca2pulse, cb2pulse;
  SDL_bool srtrigger;

  struct machine *oric;
};

#define VIA_IORB   0
#define VIA_IORA   1
#define VIA_DDRB   2
#define VIA_DDRA   3
#define VIA_T1C_L  4
#define VIA_T1C_H  5
#define VIA_T1L_L  6
#define VIA_T1L_H  7
#define VIA_T2C_L  8
#define VIA_T2C_H  9
#define VIA_SR    10
#define VIA_ACR   11
#define VIA_PCR   12
#define VIA_IFR   13
#define VIA_IER   14
#define VIA_IORA2 15

#define PCRB_CA1CON 0
#define PCRF_CA1CON (1<<PCRB_CA1CON)
#define PCRF_CA2CON (0x0e)
#define PCRB_CB1CON 4
#define PCRF_CB1CON (1<<PCRB_CB1CON)
#define PCRF_CB2CON (0xe0)

#define ACRB_PALATCH 0
#define ACRF_PALATCH (1<<ACRB_PALATCH)
#define ACRB_PBLATCH 1
#define ACRF_PBLATCH (1<<ACRB_PBLATCH)
#define ACRF_SRCON   (0x1c)
#define ACRB_T2CON   5
#define ACRF_T2CON   (1<<ACRB_T2CON)
#define ACRF_T1CON   (0xc0)

#define VIRQB_CA2 0
#define VIRQF_CA2 (1<<VIRQB_CA2)
#define VIRQB_CA1 1
#define VIRQF_CA1 (1<<VIRQB_CA1)
#define VIRQB_SR 2
#define VIRQF_SR (1<<VIRQB_SR)
#define VIRQB_CB2 3
#define VIRQF_CB2 (1<<VIRQB_CB2)
#define VIRQB_CB1 4
#define VIRQF_CB1 (1<<VIRQB_CB1)
#define VIRQB_T2 5
#define VIRQF_T2 (1<<VIRQB_T2)
#define VIRQB_T1 6
#define VIRQF_T1 (1<<VIRQB_T1)

void tape_eject( struct machine *oric );
void tape_rewind( struct machine *oric );
SDL_bool tape_load_tap( struct machine *oric, char *fname );
void tape_ticktock( struct machine *oric, int cycles );

// Init/Reset
void via_init( struct via *v, struct machine *oric );

// Move timers on etc.
void via_clock( struct via *v, unsigned int cycles );

// Write VIA from CPU
void via_write( struct via *v, int offset, unsigned char data );

// Read VIA from CPU
unsigned char via_read( struct via *v, int offset );

// Write CA1,CA2,CB1,CB2 from external device
// (data is treated as bool)
void via_write_CA1( struct via *v, unsigned char data );
void via_write_CA2( struct via *v, unsigned char data );
void via_write_CB1( struct via *v, unsigned char data );
void via_write_CB2( struct via *v, unsigned char data );

// Read ports from external device
unsigned char via_read_porta( struct via *v );
unsigned char via_read_portb( struct via *v );

// Write ports from external device
// v = VIA, mask = bits connected to writing device, data = new state for those bits
void via_write_porta( struct via *v, unsigned char mask, unsigned char data );
void via_write_portb( struct via *v, unsigned char mask, unsigned char data );

// Write to IFR from the monitor
void via_mon_write_ifr( struct via *v, unsigned char data );

// Read without side-effects for monitor
unsigned char via_mon_read( struct via *v, int offset );
