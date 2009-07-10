/* General Instruments AY-8912 */

#define TONETIME     8
#define NOISETIME    8
#define ENVTIME     32

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL/SDL.h>

#include "6502.h"
#include "8912.h"
#include "via.h"
#include "gui.h"
#include "disk.h"
#include "machine.h"

#define AUDIOBUFFERS 4

static Sint16 sndbuf[AUDIOBUFFERS][AUDIO_BUFLEN];
volatile Sint32 currsndbuf=0, currsndpos=0;
volatile Sint32 rendsndbuf=1, rendsndpos=0;
volatile Sint32 nextsndbuf=2, nextsndpos=0;
volatile SDL_bool swapaudio=SDL_FALSE;
extern SDL_bool soundavailable, soundon, warpspeed;

Sint32 voltab[] = { 0, /*62/4,*/161/4,265/4,377/4,580/4,774/4,1155/4,1575/4,2260/4,3088/4,4570/4,6233/4,9330/4,13187/4,21220/4,32767/4 };

//                           0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
unsigned char eshape0[] = { 15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 128+15 };
unsigned char eshape4[] = {  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15, 0, 128+16 };
unsigned char eshape8[] = { 15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 128+0 };
unsigned char eshapeA[] = { 15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15, 128+0 };
unsigned char eshapeB[] = { 15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 15, 128+16 };
unsigned char eshapeC[] = {  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15, 128+0 };
unsigned char eshapeD[] = {  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15, 128+15 };
unsigned char eshapeE[] = {  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 128+0 };

unsigned char *eshapes[] = { eshape0, // 0000
                             eshape0, // 0001
                             eshape0, // 0010
                             eshape0, // 0011
                             eshape4, // 0100
                             eshape4, // 0101
                             eshape4, // 0110
                             eshape4, // 0111
                             eshape8, // 1000
                             eshape0, // 1001
                             eshapeA, // 1010
                             eshapeB, // 1011
                             eshapeC, // 1100
                             eshapeD, // 1101
                             eshapeE, // 1110
                             eshape4 };//1111


//                                FE           FD           FB           F7           EF           DF           BF           7F
static unsigned short keytab[] = { '7'        , 'n'        , '5'        , 'v'        , SDLK_RCTRL , '1'        , 'x'        , '3'        ,
                                   'j'        , 't'        , 'r'        , 'f'        , 0          , SDLK_ESCAPE, 'q'        , 'd'        ,
                                   'm'        , '6'        , 'b'        , '4'        , SDLK_LCTRL , 'z'        , '2'        , 'c'        ,
                                   'k'        , '9'        , ';'        , '-'        , '#'        , '\\'       , 0          , '\''       ,
                                   SDLK_SPACE , ','        , '.'        , SDLK_UP    , SDLK_LSHIFT, SDLK_LEFT  , SDLK_DOWN  , SDLK_RIGHT ,
                                   'u'        , 'i'        , 'o'        , 'p'        , SDLK_LALT  , SDLK_BACKSPACE, ']'     , '['        ,
                                   'y'        , 'h'        , 'g'        , 'e'        , SDLK_RALT  , 'a'        , 's'        , 'w'        ,
                                   '8'        , 'l'        , '0'        , '/'        , SDLK_RSHIFT, SDLK_RETURN, 0          , SDLK_EQUALS };

Sint16 lpbuf[2];

Sint16 lowpass( Sint16 input )
{
  Sint16 output;

  output = lpbuf[0]/4+input/2+lpbuf[1]/4;
  lpbuf[1] = lpbuf[0];
  lpbuf[0] = input;
  return output;
}

static Uint32 ayrand( struct ay8912 *ay )
{
  Uint32 rbit = (ay->rndrack&1) ^ ((ay->rndrack>>2)&1);
  ay->rndrack = (ay->rndrack>>1)|(rbit<<16);
  return rbit ? 0 : 0xffff;
}

void ay_callback( void *dummy, Sint8 *stream, int length )
{
  Sint16 *out;
  Sint32 i, j;
  SDL_Event     event;
  SDL_UserEvent userevent;
  
  out = (Sint16 *)stream;
  for( i=0,j=0; i<AUDIO_BUFLEN; i++ )
  {
    out[j++] = sndbuf[currsndbuf][i];
    out[j++] = sndbuf[currsndbuf][i];
  }

  userevent.type  = SDL_USEREVENT;
  userevent.code  = 0;
  userevent.data1 = NULL;
  userevent.data2 = NULL;
  
  event.type = SDL_USEREVENT;
  event.user = userevent;
  
  SDL_PushEvent( &event );

  swapaudio = SDL_TRUE;
}

void ay_ticktock( struct ay8912 *ay, int cycles )
{
  SDL_bool newout[3], newnoise;
  Sint32 oldrendpos, oldnextpos, endsamples;
  Sint32 i, j;
  Sint32 output;

  if( swapaudio )
  {
    currsndbuf = rendsndbuf;
    currsndpos = rendsndpos;
    rendsndbuf = nextsndbuf;
    rendsndpos = nextsndpos;
    nextsndbuf = (nextsndbuf+1)%AUDIOBUFFERS;
    nextsndpos = 0;
    swapaudio = SDL_FALSE;
  }

  for( i=0; i<3; i++ )
    newout[i] = SDL_FALSE;
  newnoise = SDL_FALSE;
  while( cycles > 0 )
  {
    newnoise = SDL_FALSE;
    if( (--ay->ctn) <= 0 )
    {
      ay->currnoise ^= ayrand( ay );
      ay->ctn += (ay->regs[AY_NOISE_PER]&0x1f)*NOISETIME;
      newnoise = SDL_TRUE;
    }

    for( i=0; i<3; i++ )
    {
      if( (--ay->ct[i]) <= 0 )
      {
        ay->ct[i] += (((ay->regs[i*2+1]&0xf)<<8)|ay->regs[i*2])*TONETIME;
        ay->sign[i] = -ay->sign[i];
        newout[i] = SDL_TRUE;
      }

      if( ( newnoise ) && ( ay->noiseon[i] ) )
        newout[i] = SDL_TRUE;        
    }

    if( (--ay->cte) <= 0 )
    {
      ay->cte += ((ay->regs[AY_ENV_PER_H]<<8)|ay->regs[AY_ENV_PER_L])*ENVTIME;
      ay->envpos++;
      if( ay->envtab[ay->envpos]&0x80 )
        ay->envpos = ay->envtab[ay->envpos]&0x7f;
      for( i=0; i<3; i++ )
      {
        if( ay->regs[AY_CHA_AMP+i]&0x10 )
        {
          ay->vol[i] = voltab[ay->envtab[ay->envpos]];
          newout[i] = SDL_TRUE;
        }
      }
    }

    output = 0;
    for( i=0; i<3; i++ )
    {
      if( newout[i] )
      {
        j = ay->sign[i]*ay->toneon[i]*ay->vol[i];
        if( ay->noiseon[i] ) j += (ay->currnoise*ay->vol[i])>>16;
        ay->out[i] = j;
        newout[i] = 0;
      }
      output += ay->out[i];
    }

    if( ( ay->oric->tapemotor ) && ( ay->oric->tapenoise ) )
      output += ay->oric->tapeout ? 8192 : -8192;

    if( output > 32767 ) output = 32767;
    if( output < -32768 ) output = -32768;
    
    oldrendpos = rendsndpos;
    oldnextpos = nextsndpos;
    
    rendsndpos += SAMPLESPERCYCLE;
    if( rendsndpos > (AUDIO_BUFLEN<<FPBITS) )
    {
      nextsndpos += rendsndpos-(AUDIO_BUFLEN<<FPBITS);
      rendsndpos = AUDIO_BUFLEN<<FPBITS;
      if( nextsndpos > (AUDIO_BUFLEN<<FPBITS ) )
        nextsndpos = AUDIO_BUFLEN<<FPBITS;
    }

    endsamples = rendsndpos>>FPBITS;
    for( i=oldrendpos>>FPBITS; i<endsamples; i++ )
      sndbuf[rendsndbuf][i] = lowpass(output);

    endsamples = nextsndpos>>FPBITS;
    for( i=oldnextpos>>FPBITS; i<endsamples; i++ )
      sndbuf[nextsndbuf][i] = lowpass(output);    

    cycles--;
  }
}

SDL_bool ay_init( struct ay8912 *ay, struct machine *oric )
{
  int i;

  for( i=0; i<8; i++ )
    ay->keystates[i] = SDL_FALSE;

  for( i=0; i<3; i++ )
  {
    ay->ct[i]      = 0;
    ay->out[i]     = 0;
    ay->sign[i]    = 1;
    ay->toneon[i]  = 0;
    ay->noiseon[i] = 0;
    ay->vol[i]     = 0;
  }
  ay->ctn = 0;
  ay->cte = 0;

  ay->envtab  = eshape0;

  ay->bmode   = 0;
  ay->creg    = 0;
  ay->oric    = oric;
  ay->soundon = soundavailable && soundon && (!warpspeed);
  ay->currnoise = 0xffff;
  ay->rndrack = 1;
  if( soundavailable )
    SDL_PauseAudio( 0 );

  return SDL_TRUE;
}

void ay_update_keybits( struct ay8912 *ay )
{
  int offset;

  offset = via_read_portb( &ay->oric->via ) & 0x7;
  if( ay->keystates[offset] & (ay->regs[AY_PORT_A]^0xff) )
    via_write_portb( &ay->oric->via, 0x08, 0x08 );
  else
    via_write_portb( &ay->oric->via, 0x08, 0x00 );
}

void ay_keypress( struct ay8912 *ay, unsigned short key, SDL_bool down )
{
  int i;

  if( key == 0 ) return;

  for( i=0; i<64; i++ )
    if( keytab[i] == key ) break;

  if( i == 64 ) return;

  if( down )
    ay->keystates[i>>3] |= (1<<(i&7));
  else
    ay->keystates[i>>3] &= ~(1<<(i&7));

  ay_update_keybits( ay );
}

static void ay_modeset( struct ay8912 *ay )
{
  unsigned char v;
  Sint32 i, j;

  switch( ay->bmode )
  {
    case 0x01: // Read AY register
      via_write_porta( &ay->oric->via, 0xff, (ay->creg>=AY_LAST) ? 0 : ay->regs[ay->creg] );
      break;
    
    case 0x02: // Write AY register
      if( ay->creg >= AY_LAST ) break;
      v = via_read_porta( &ay->oric->via );
      ay->regs[ay->creg] = v;

      switch( ay->creg )
      {
        case AY_CHA_PER_L:          
        case AY_CHA_PER_H:
          ay->ct[0] = (((ay->regs[AY_CHA_PER_H]&0xf)<<8)|ay->regs[AY_CHA_PER_L])*TONETIME;
          break;

        case AY_CHB_PER_L:
        case AY_CHB_PER_H:
          ay->ct[1] = (((ay->regs[AY_CHB_PER_H]&0xf)<<8)|ay->regs[AY_CHB_PER_L])*TONETIME;
          break;

        case AY_CHC_PER_L:
        case AY_CHC_PER_H:
          ay->ct[2] = (((ay->regs[AY_CHC_PER_H]&0xf)<<8)|ay->regs[AY_CHC_PER_L])*TONETIME;
          break;
        
        case AY_STATUS:
          ay->toneon[0]  = (ay->regs[AY_STATUS]&0x01)?0:1;
          ay->toneon[1]  = (ay->regs[AY_STATUS]&0x02)?0:1;
          ay->toneon[2]  = (ay->regs[AY_STATUS]&0x04)?0:1;
          ay->noiseon[0] = (ay->regs[AY_STATUS]&0x08)?0:1;
          ay->noiseon[1] = (ay->regs[AY_STATUS]&0x10)?0:1;
          ay->noiseon[2] = (ay->regs[AY_STATUS]&0x20)?0:1;
          for( i=0; i<3; i++ )
          {
            j = ay->sign[i]*ay->toneon[i]*ay->vol[i];
            if( ay->noiseon[i] ) j += (ay->currnoise*ay->vol[i])>>16;
            ay->out[i] = j;
          }
          break;

        case AY_NOISE_PER:
          ay->ctn = (ay->regs[AY_NOISE_PER]&0x1f)*NOISETIME;
          break;
        
        case AY_CHA_AMP:
        case AY_CHB_AMP:
        case AY_CHC_AMP:
          i = ay->creg-AY_CHA_AMP;
          if( (v&0x10)==0 )
            ay->vol[i] = voltab[v&0xf];
          else
            ay->vol[i] = voltab[ay->envtab[ay->envpos]];
          ay->out[i] = ay->sign[i]*ay->vol[i]*ay->toneon[i];
          break;
        
        case AY_ENV_PER_L:
        case AY_ENV_PER_H:
          ay->cte = ((ay->regs[AY_ENV_PER_H]<<8)|ay->regs[AY_ENV_PER_L])*ENVTIME;
          for( i=0; i<3; i++ )
          {
            if( ay->regs[AY_CHA_AMP+i]&0x10 )
            {
              ay->vol[i] = voltab[ay->envtab[ay->envpos]];
              ay->out[i] = ay->sign[i]*ay->vol[i]*ay->toneon[i];
            }
          }
          break;

        case AY_ENV_CYCLE:
          ay->envtab = eshapes[v&0xf];
          ay->envpos = 0;
          for( i=0; i<3; i++ )
          {
            if( ay->regs[AY_CHA_AMP+i]&0x10 )
            {
              ay->vol[i] = voltab[ay->envtab[ay->envpos]];
              ay->out[i] = ay->sign[i]*ay->vol[i]*ay->toneon[i];
            }
          }
          break;

        case AY_PORT_A:
          if( ay->creg == AY_PORT_A )
            ay_update_keybits( ay );
          break;
      }
      break;
    
    case 0x03: // Set register
      ay->creg = via_read_porta( &ay->oric->via );
      break;
  }
}

void ay_set_bcmode( struct ay8912 *ay, unsigned char bc1, unsigned char bdir )
{
  ay->bmode = (bc1?AYBMF_BC1:0)|(bdir?AYBMF_BDIR:0);
  ay_modeset( ay );
}

void ay_set_bc1( struct ay8912 *ay, unsigned char state )
{
  if( state )
    ay->bmode |= AYBMF_BC1;
  else
    ay->bmode &= ~AYBMF_BC1;
  ay_modeset( ay );
}

void ay_set_bdir( struct ay8912 *ay, unsigned char state )
{
  if( state )
    ay->bmode |= AYBMF_BDIR;
  else
    ay->bmode &= ~AYBMF_BDIR;
  ay_modeset( ay );
}

