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
**  General Instruments AY-8912 emulation (including oric keyboard emulation)
*/

// Integer fraction bits to use when mapping
// clock cycles to audio samples
#define FPBITS 10

// Output audio frequency
#define AUDIO_FREQ   44100

// Audio buffer size
#define AUDIO_BUFLEN 4096

#define CYCLESPERSECOND (312*64*50)
#define CYCLESPERSAMPLE ((CYCLESPERSECOND<<FPBITS)/AUDIO_FREQ)

// GI addressing
#define AYBMB_BC1  0
#define AYBMF_BC1  (1<<AYBMB_BC1)
#define AYBMB_BDIR 1
#define AYBMF_BDIR (1<<AYBMB_BDIR)

// 8912 registers
enum
{
  AY_CHA_PER_L = 0,
  AY_CHA_PER_H,
  AY_CHB_PER_L,
  AY_CHB_PER_H,
  AY_CHC_PER_L,
  AY_CHC_PER_H,
  AY_NOISE_PER,
  AY_STATUS,
  AY_CHA_AMP,
  AY_CHB_AMP,
  AY_CHC_AMP,
  AY_ENV_PER_L,
  AY_ENV_PER_H,
  AY_ENV_CYCLE,
  AY_PORT_A,
  AY_LAST
};

struct aywrite
{
  Uint32 cycle;
  Uint8  reg;
  Uint8  val;
};

struct ay8912
{
  unsigned char bmode;
  unsigned char creg;
  unsigned char regs[AY_LAST], eregs[AY_LAST];
  SDL_bool keystates[8];
  SDL_bool soundon;
  int logged;
  Uint32 logcycle;
  struct aywrite writelog[AUDIO_BUFLEN];
  Sint32 toneon[3], noiseon[3], vol[3];
  int ct[3], ctn, cte;
  Sint32 sign[3], out[3], envpos;
  unsigned char *envtab;
  struct machine *oric;
  Uint32 currnoise, rndrack;
  Sint16 output;
};

void queuekeys( char *str );

SDL_bool ay_init( struct ay8912 *ay, struct machine *oric );
void ay_callback( void *dummy, Sint8 *stream, int length );
void ay_ticktock( struct ay8912 *ay, int cycles );
void ay_update_keybits( struct ay8912 *ay );
void ay_keypress( struct ay8912 *ay, unsigned short key, SDL_bool down );

void ay_set_bc1( struct ay8912 *ay, unsigned char state );
void ay_set_bdir( struct ay8912 *ay, unsigned char state );
void ay_set_bcmode( struct ay8912 *ay, unsigned char bc1, unsigned char bdir );

