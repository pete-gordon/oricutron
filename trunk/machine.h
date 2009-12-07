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
**  Oric machine stuff
*/

#if __WIN32__
#define PATHSEP '\\'
#else
#define PATHSEP '/'
#endif

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

  Uint16 vidbases[4];

  int drivetype;
  struct wd17xx    wddisk;
  struct microdisc md;
  char diskname[MAX_DRIVES][16];

  unsigned char tapebit, tapeout, tapeparity;
  int tapelen, tapeoffs, tapecount, tapetime, tapedupbytes;
  unsigned char *tapebuf;
  SDL_bool tapemotor, tapenoise, tapeturbo, autorewind, autoinsert;
  SDL_bool symbolsautoload, symbolscase;
  char tapename[16];
};

void setemumode( struct machine *oric, struct osdmenuitem *mitem, int mode );
void video_show( struct machine *oric );
SDL_bool video_doraster( struct machine *oric );
SDL_bool emu_event( SDL_Event *ev, struct machine *oric, SDL_bool *needrender );

void preinit_machine( struct machine *oric );
SDL_bool init_machine( struct machine *oric, int type );
void shut_machine( struct machine *oric );
void setdrivetype( struct machine *oric, struct osdmenuitem *mitem, int type );
void swapmach( struct machine *oric, struct osdmenuitem *mitem, int which );
SDL_bool isram( struct machine *oric, unsigned short addr );

