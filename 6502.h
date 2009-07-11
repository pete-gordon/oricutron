
#define CYCLECOUNT 1

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

#define IRQB_VIA 0
#define IRQF_VIA (1<<IRQB_VIA)
#define IRQB_DISK 1
#define IRQF_DISK (1<<IRQB_DISK)

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
  void (*write)(unsigned short, unsigned char);
  unsigned char (*read)(unsigned short);
  SDL_bool anybp;
  int breakpoints[16];
};

void m6502_init( struct m6502 *cpu );
void m6502_reset( struct m6502 *cpu );
SDL_bool m6502_inst( struct m6502 *cpu, SDL_bool dobp );
