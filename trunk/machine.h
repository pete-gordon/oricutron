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
**  Oric machine stuff
*/

enum
{
  DRV_NONE = 0,
  DRV_JASMIN,
  DRV_MICRODISC
};

enum
{
  MACH_ORIC1 = 0,
  MACH_ORIC1_16K,
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

enum
{
  TELEBANK_RAM,
  TELEBANK_ROM,
  TELEBANK_HALFNHALF
};

enum
{
  KMAP_QWERTY = 0,
  KMAP_AZERTY,
  KMAP_QWERTZ
};

struct telebankinfo
{
  unsigned char type;
  unsigned char *ptr;
};

struct machine
{
  Uint8 type;
  struct m6502 cpu;
  struct via via;
  struct ay8912 ay;
  unsigned char *mem;
  unsigned char *rom;
  int emu_mode;

  struct symboltable romsyms;
  struct symboltable *disksyms;

  struct telebankinfo tele_bank[8];
  struct symboltable  tele_banksyms[8];
  struct via          tele_via;
  int                 tele_currbank;
  unsigned char       tele_banktype;

  // Video
  int vid_start;           // Start drawing video
  int vid_end;             // Stop drawing video
  int vid_maxrast;         // Number of raster lines
  int vid_raster;          // Current rasterline
  int vid_special;         // After line 200, only process in text mode

  int vid_fg_col;
  int vid_bg_col;
  int vid_mode;
  int vid_freq;
  int vid_textattrs;
  int vid_blinkmask;
  int frames;

  int cyclesperraster;
  int vsync;

  SDL_bool vid_double;
  SDL_bool romdis;
  SDL_bool vsynchack;

  unsigned short vid_addr;
  unsigned char *vid_ch_data;
  unsigned char *vid_ch_base;

  Uint8  *scrpt;
  Uint8  *scr;

  Uint16 vidbases[4];

  int drivetype;
  struct wd17xx    wddisk;
  struct microdisc md;
  struct jasmin jasmin;
  char diskname[MAX_DRIVES][32];
  
  FILE *prf;
  int prclose, prclock;

  unsigned char tapebit, tapeout, tapeparity;
  int tapelen, tapeoffs, tapecount, tapetime, tapedupbytes;
  unsigned char *tapebuf;
  SDL_bool tapemotor, tapenoise, tapeturbo, autorewind, autoinsert;
  SDL_bool symbolsautoload, symbolscase;
  char lasttapefile[20];
  char tapename[32];
  int tapeturbo_syncstack;

  // Filename decoding patch addresses
  int pch_fd_getname_pc;
  int pch_fd_getname_addr;
  SDL_bool pch_fd_available;

  // Turbo tape patch addresses
  int pch_tt_getsync_pc;
  int pch_tt_getsync_end_pc;
  int pch_tt_getsync_loop_pc;
  int pch_tt_readbyte_pc;
  int pch_tt_readbyte_end_pc;
  int pch_tt_readbyte_storebyte_addr;
  int pch_tt_readbyte_storezero_addr;
  SDL_bool pch_tt_readbyte_setcarry;
  SDL_bool pch_tt_available;

  Sint32 keymap;

  SDL_bool hstretch, scanlines;

  int rendermode;
  void (*render_begin)(struct machine *);
  void (*render_end)(struct machine *);
  void (*render_textzone_alloc)(struct machine *, int);
  void (*render_textzone_free)(struct machine *, int);
  void (*render_textzone)(struct machine *, int);
  void (*render_gimg)(int, Sint32, Sint32);
  void (*render_gimgpart)(int, Sint32, Sint32, Sint32, Sint32, Sint32, Sint32);
  void (*render_alloc_textzone)(struct machine *, struct textzone *);
  void (*render_free_textzone)(struct machine *, struct textzone *);
  void (*render_video)(struct machine *, SDL_bool);
  SDL_bool (*render_togglefullscreen)(struct machine *oric);
  SDL_bool (*init_render)(struct machine *);
  void (*shut_render)(struct machine *);
  
  char popupstr[40];
  int popuptime;
  SDL_bool newpopupstr;
  
  char statusstr[40];
  SDL_bool newstatusstr;
  
  SDL_bool showfps;
  
  Sint32 joy_iface;
  Sint32 joymode_a, joymode_b;
  Sint32 telejoymode_a, telejoymode_b;
  Sint16 kbjoy1[6], kbjoy2[6];

  SDL_Joystick *sdljoy_a, *sdljoy_b;
};

void setemumode( struct machine *oric, struct osdmenuitem *mitem, int mode );
void video_show( struct machine *oric );
SDL_bool emu_event( SDL_Event *ev, struct machine *oric, SDL_bool *needrender );

void preinit_machine( struct machine *oric );
void load_diskroms( struct machine *oric );
SDL_bool init_machine( struct machine *oric, int type, SDL_bool nukebreakpoints );
void shut_machine( struct machine *oric );
void setdrivetype( struct machine *oric, struct osdmenuitem *mitem, int type );
void swapmach( struct machine *oric, struct osdmenuitem *mitem, int which );
SDL_bool isram( struct machine *oric, unsigned short addr );

void save_snapshot( struct machine *oric, char *filename );
