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
**  GUI
*/

// Reserve your textzones here!
enum
{
  TZ_MONITOR = 0,
  TZ_DEBUG,
  TZ_MEMWATCH,
  TZ_REGS,
  TZ_VIA,
  TZ_AY,
  TZ_DISK,
  TZ_MENU,
  TZ_FILEREQ,
  TZ_LAST
};

struct textzone
{
  int x, y, w, h;           // Dimensions
  int px, py, cfc, cbc;     // Current position and text colours
  unsigned char *tx;        // Text buffer
  unsigned char *fc, *bc;   // Colour buffers
};

// "on screen display" menus
#define OSDMENUBAR ((char *)-1)

struct osdmenuitem
{
  char *name;
  void (*func)(struct machine *,struct osdmenuitem *,int);
  int arg;
};

struct osdmenu
{
  char *title;
  int citem;
  struct osdmenuitem *items;
};

#define VSPTMPSIZE 1024

void do_popup( char *str );
void makebox( struct textzone *ptz, int x, int y, int w, int h, int fg, int bg );
void tzstr( struct textzone *ptz, char *text );
void tzstrpos( struct textzone *ptz, int x, int y, char *text );
void tzsetcol( struct textzone *ptz, int fc, int bc );
void tzprintf( struct textzone *ptz, char *fmt, ... );
void tzprintfpos( struct textzone *ptz, int x, int y, char *fmt, ... );
void draw_textzone( struct textzone *ptz );
void printstr( int x, int y, Uint16 fc, Uint16 bc, char *str );
void gotomenu( struct machine *oric, struct osdmenuitem *mitem, int menunum );
SDL_bool menu_event( SDL_Event *ev, struct machine *oric, SDL_bool *needrender );
void setmenutoggles( struct machine *oric );

void render( struct machine *oric );
void preinit_gui( void );
SDL_bool init_gui( struct machine *oric );
void shut_gui( void );
