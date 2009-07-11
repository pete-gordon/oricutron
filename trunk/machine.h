
enum
{
  DRV_NONE = 0,
  DRV_JASMIN,
  DRV_MICRODISC
};

enum
{
	MACH_ORIC1 = 0,
	MACH_ATMOS,
	MACH_TELESTRAT
};

enum
{
  EM_PAUSED = 0,
  EM_RUNNING,
  EM_DEBUG,
  EM_MENU,
  EM_PLEASEQUIT
};

struct machine
{
  int type;
  struct m6502 cpu;
  struct via via;
  struct ay8912 ay;
  unsigned char *mem;
  unsigned char *rom;
  int emu_mode;

  // Video
  int vid_start;           // Start drawing video
  int vid_end;             // Stop drawing video
  int vid_maxrast;         // Number of raster lines
  int vid_raster;          // Current rasterline
  int vid_special;         // After line 200, only process in text mode

  int vid_fg_col;
  int vid_bg_col;
  int vid_mode;
  int vid_textattrs;
  int vid_blinkmask;
  int frames;

  int cyclesperraster;

  SDL_bool vid_double;
  SDL_bool romdis;

  unsigned short vid_addr;
  unsigned char *vid_ch_data;
  unsigned char *vid_ch_base;

  Uint16 pal[8]; // Palette
  Uint16 *scrpt;
  Uint16 *scr;

  int drivetype;
  struct wd17xx    wddisk;
  struct microdisc md;
  char diskname[MAX_DRIVES][16];

  unsigned char tapebit, tapeout, tapeparity;
  int tapelen, tapeoffs, tapecount, tapetime, tapedupbytes;
  unsigned char *tapebuf;
  SDL_bool tapemotor, tapenoise, tapeturbo, autorewind, autoinsert;
  char tapename[16];
};

void setemumode( struct machine *oric, struct osdmenuitem *mitem, int mode );
void video_show( struct machine *oric );
SDL_bool video_doraster( struct machine *oric );
SDL_bool emu_event( SDL_Event *ev, struct machine *oric, SDL_bool *needrender );

void preinit_machine( void );
SDL_bool init_machine( int type );
void shut_machine( void );
void setdrivetype( struct machine *oric, struct osdmenuitem *mitem, int type );
void swapmach( struct machine *oric, struct osdmenuitem *mitem, int which );
