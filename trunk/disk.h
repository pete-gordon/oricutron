
// Microdisc

// Status bits (0x314)
#define MDSB_INTENA  0
#define MDSF_INTENA  (1<<MDSB_INTENA)
#define MDSB_ROMDIS  1
#define MDSF_ROMDIS  (1<<MDSB_ROMDIS)
#define MDSB_DENSITY 3
#define MDSF_DENSITY (1<<MDSB_DENSITY)
#define MDSB_SIDE    4
#define MDSF_SIDE    (1<<MDSB_SIDE)
#define MDSF_DRIVE   0x60
#define MDSB_EPROM   7                  // Write
#define MDSF_EPROM   (1<<MDSB_EPROM)
#define MDSB_INTRQ   7                  // Read
#define MDSF_INTRQ   (1<<MDSB_INTRQ)

// 0x318
#define MB_DRQ 7
#define MF_DRQ (1<<MB_DRQ)

struct wd17xx
{
  Uint8 r_status;
  Uint8 cmd;
  Uint8 r_track;
  Uint8 r_sector;
  Uint8 r_data;
  void (*setintrq)(void *);
  void (*clrintrq)(void *);
  void *intrqarg;
};

struct microdisc
{
  Uint8 status;
  Uint8 drq, eprom;
  struct wd17xx *wd;
  struct machine *oric;
};

SDL_bool disk_load_dsk( struct machine *oric, char *fname );
void microdisc_init( struct microdisc *md, struct wd17xx *wd, struct machine *oric );
unsigned char microdisc_read( struct microdisc *md, unsigned short addr );
void microdisc_write( struct microdisc *md, unsigned short addr, unsigned char data );
