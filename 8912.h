
#define FPBITS 16
#define AUDIO_FREQ   44100
#define AUDIO_BUFLEN (AUDIO_FREQ/50)
#define SAMPLESPERCYCLE (((AUDIO_BUFLEN<<FPBITS)/(64*312))+1)  // Samples per frame / Cycles per frame (in fixed point) Rounded up a bit.

#define AYBMB_BC1  0
#define AYBMF_BC1  (1<<AYBMB_BC1)
#define AYBMB_BDIR 1
#define AYBMF_BDIR (1<<AYBMB_BDIR)

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

struct ay8912
{
  unsigned char bmode;
  unsigned char creg;
  unsigned char regs[AY_LAST];
  SDL_bool keystates[8];
  SDL_bool soundon;
  Sint32 toneon[3], noiseon[3], vol[3];
  int ct[3], ctn, cte;
  Sint32 sign[3], out[3], envpos;
  unsigned char *envtab;
  struct machine *oric;
  Uint32 currnoise, rndrack;
};

SDL_bool ay_init( struct ay8912 *ay, struct machine *oric );
void ay_callback( void *dummy, Sint8 *stream, int length );
void ay_ticktock( struct ay8912 *ay, int cycles );
void ay_update_keybits( struct ay8912 *ay );
void ay_keypress( struct ay8912 *ay, unsigned short key, SDL_bool down );

void ay_set_bc1( struct ay8912 *ay, unsigned char state );
void ay_set_bdir( struct ay8912 *ay, unsigned char state );
void ay_set_bcmode( struct ay8912 *ay, unsigned char bc1, unsigned char bdir );

