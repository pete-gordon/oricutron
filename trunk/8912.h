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
**  General Instruments AY-8912 emulation (including oric keyboard emulation)
*/

// Integer fraction bits to use when mapping
// clock cycles to audio samples
#define FPBITS 10

// Audio buffer size
#define AUDIO_BUFLEN 4096

#define WRITELOG_SIZE (AUDIO_BUFLEN*12)
#define TAPELOG_SIZE (AUDIO_BUFLEN)

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

struct tnchange
{
  Uint32 cycle;
  Uint8  val;
};

struct ay8912
{
  Uint8           bmode, creg;
  Uint8           regs[AY_LAST], eregs[AY_LAST];
  SDL_bool        keystates[8], newnoise;
  SDL_bool        soundon;
  SDL_bool        tapenoiseon;
  Uint32          toneper[3], noiseper, envper;
  Uint16          tonebit[3], noisebit[3], vol[3], newout;
  Sint32          ct[3], ctn, cte;
  Uint32          tonepos[3], tonestep[3];
  Sint32          sign[3], out[3], envpos;
  unsigned char  *envtab;
  struct machine *oric;
  Uint32          currnoise, rndrack;
  Sint16          output;
  Sint16          tapeout;
  Uint32          ccycle, lastcyc, ccyc;
  Uint32          keybitdelay, currkeyoffs;

  SDL_bool        audiolocked;
  SDL_bool        do_logcycle_reset;
  Sint32          logged, tlogged;
  Uint32          logcycle, newlogcycle;
  struct aywrite  writelog[WRITELOG_SIZE];
  struct tnchange tapelog[TAPELOG_SIZE];
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
void ay_modeset( struct ay8912 *ay );

void ay_lockaudio( struct ay8912 *ay );
void ay_unlockaudio( struct ay8912 *ay );
