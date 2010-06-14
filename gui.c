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
**  GUI
*/

/*
** The GUI in Oricutron is built on the basic element of the "textzone". This
** is a simple structure containing the position, size and contents of an area
** of text, which you can render onto an SDL surface whenever you like.
*/

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

#ifdef __amigaos4__
#include <proto/dos.h>
#endif

#ifdef WIN32
#include <windows.h>
#define WANT_WMINFO
#endif

#include "system.h"
#include "6502.h"
#include "via.h"
#include "8912.h"
#include "gui.h"
#include "disk.h"
#include "machine.h"
#include "monitor.h"
#include "filereq.h"

char tapepath[4096], tapefile[512];
char diskpath[4096], diskfile[512];
//char snappath[4096], snapfile[512];
char filetmp[4096+512];

#define GIMG_W_DISK 18
#define GIMG_W_TAPE 20
#define GIMG_W_AVIR 16

// Images used in the GUI
struct guiimg gimgs[GIMG_LAST] = { { IMAGEPREFIX"statusbar.bmp",              640, 16, NULL },
                                   { IMAGEPREFIX"disk_ejected.bmp",   GIMG_W_DISK, 16, NULL },
                                   { IMAGEPREFIX"disk_idle.bmp",      GIMG_W_DISK, 16, NULL },
                                   { IMAGEPREFIX"disk_active.bmp",    GIMG_W_DISK, 16, NULL },
                                   { IMAGEPREFIX"disk_modified.bmp",  GIMG_W_DISK, 16, NULL },
                                   { IMAGEPREFIX"disk_modactive.bmp", GIMG_W_DISK, 16, NULL },
                                   { IMAGEPREFIX"tape_ejected.bmp",   GIMG_W_TAPE, 16, NULL },
                                   { IMAGEPREFIX"tape_pause.bmp",     GIMG_W_TAPE, 16, NULL },
                                   { IMAGEPREFIX"tape_play.bmp",      GIMG_W_TAPE, 16, NULL },
                                   { IMAGEPREFIX"tape_stop.bmp",      GIMG_W_TAPE, 16, NULL },
                                   { IMAGEPREFIX"avirec.bmp",         GIMG_W_AVIR, 16, NULL } };

extern SDL_bool fullscreen, hwsurface;
SDL_Surface *screen = NULL;
SDL_bool need_sdl_quit = SDL_FALSE;
SDL_bool soundavailable, soundon;
extern SDL_bool microdiscrom_valid, jasminrom_valid;

// GUI image locations
#define GIMG_POS_SBARY   (480-16)
#define GIMG_POS_TAPEX   (640-4-GIMG_W_TAPE)
#define GIMG_POS_DISKX   (GIMG_POS_TAPEX-(6+(GIMG_W_DISK*4)))
#define GIMG_POS_AVIRECX (GIMG_POS_DISKX-(6+GIMG_W_AVIR))

// Our "lovely" hand-coded font
extern unsigned char thefont[];

// Believe it or not, i have defined more than 2 shades of blue :)
#define NUM_GUI_COLS 9

// Text zone (and other gui area) colours
unsigned char sgpal[] = { 0x00, 0x00, 0x00,     // 0 = black
                          0xff, 0xff, 0xff,     // 1 = white
                          0xcc, 0xcc, 0xff,     // 2 = light blue
                          0x00, 0x00, 0xff,     // 3 = dark blue
                          0x00, 0x00, 0x40,     // 4 = very dark blue
                          0x70, 0x70, 0xff,     // 5 = mid blue
                          0x80, 0x80, 0x80,     // 6 = grey
                          0xa0, 0xa0, 0x00,     // 7 = yellow
                          0x80, 0x00, 0x00 };   // 8 = dark red

// GUI colours in display format
Uint16 gpal[NUM_GUI_COLS];

// All the textzones. Spaces in this array
// are reserved in gui.h
struct textzone *tz[TZ_LAST];

// Temporary string buffer
char vsptmp[VSPTMPSIZE];

// Cached screen->pitch
int pixpitch;

// FPS calculation vars
extern Uint32 frametimeave;
SDL_bool showfps=SDL_TRUE,warpspeed=SDL_FALSE;

// Current menu, and highlighted item number
struct osdmenu *cmenu = NULL;
int cmenuitem = 0;

// Menufunctions
void toggletapenoise( struct machine *oric, struct osdmenuitem *mitem, int dummy );
void togglesound( struct machine *oric, struct osdmenuitem *mitem, int dummy );
void inserttape( struct machine *oric, struct osdmenuitem *mitem, int dummy );
void insertdisk( struct machine *oric, struct osdmenuitem *mitem, int drive );
void resetoric( struct machine *oric, struct osdmenuitem *mitem, int dummy );
void toggletapeturbo( struct machine *oric, struct osdmenuitem *mitem, int dummy );
void toggleautowind( struct machine *oric, struct osdmenuitem *mitem, int dummy );
void toggleautoinsrt( struct machine *oric, struct osdmenuitem *mitem, int dummy );
void togglesymbolsauto( struct machine *oric, struct osdmenuitem *mitem, int dummy );
void togglecasesyms( struct machine *oric, struct osdmenuitem *mitem, int dummy );
void togglevsynchack( struct machine *oric, struct osdmenuitem *mitem, int dummy );
//void savesnap( struct machine *oric, struct osdmenuitem *mitem, int dummy );

// Menu definitions. Name, function, parameter
struct osdmenuitem mainitems[] = { { "Insert tape...",         inserttape,      0 },
                                   { "Insert disk 0...",       insertdisk,      0 },
                                   { "Insert disk 1...",       insertdisk,      1 },
                                   { "Insert disk 2...",       insertdisk,      2 },
                                   { "Insert disk 3...",       insertdisk,      3 },
                                   { OSDMENUBAR,               NULL,            0 },
//                                   { "Save snapshot...",       savesnap,        0 },
//                                   { "Load snapshot...",       NULL,            0 },
//                                   { OSDMENUBAR,               NULL,            0 },
                                   { "Hardware options...",    gotomenu,        1 },
                                   { "Audio options...",       gotomenu,        2 },
                                   { "Video options...",       NULL,            0 },
                                   { "Debug options...",       gotomenu,        3 },
                                   { OSDMENUBAR,               NULL,            0 },
                                   { "Reset",                  resetoric,       0 },
                                   { "Monitor",                setemumode,      EM_DEBUG },
                                   { "Back",                   setemumode,      EM_RUNNING },
                                   { OSDMENUBAR,               NULL,            0 },
                                   { "Quit",                   setemumode,      EM_PLEASEQUIT },
                                   { NULL, } };

struct osdmenuitem hwopitems[] = { { " Oric-1",                swapmach,        (0xffff<<16)|MACH_ORIC1 },
                                   { " Oric-1 16K",            swapmach,        (0xffff<<16)|MACH_ORIC1_16K },
                                   { " Atmos",                 swapmach,        (0xffff<<16)|MACH_ATMOS },
                                   { " Telestrat",             NULL,            0 },
                                   { OSDMENUBAR,               NULL,            0 },
                                   { " No disk",               setdrivetype,    DRV_NONE },
                                   { " Microdisc",             setdrivetype,    DRV_MICRODISC },
                                   { " Jasmin",                setdrivetype,    DRV_JASMIN },
                                   { OSDMENUBAR,               NULL,            0 },
                                   { " Turbo tape",            toggletapeturbo, 0 },
                                   { " Autoinsert tape",       toggleautoinsrt, 0 },
                                   { " Autorewind tape",       toggleautowind,  0 },
                                   { OSDMENUBAR,               NULL,            0 },
                                   { " VSync hack",            togglevsynchack, 0 },
                                   { OSDMENUBAR,               NULL,            0 },
                                   { "Back",                   gotomenu,        0 },
                                   { NULL, } };

struct osdmenuitem auopitems[] = { { " Sound enabled",         togglesound,     0 },
                                   { " Tape noise",            toggletapenoise, 0 },
                                   { OSDMENUBAR,               NULL,            0 },
                                   { "Back",                   gotomenu,        0 },
                                   { NULL, } };
                                  
struct osdmenuitem dbopitems[] = { { " Autoload symbols file", togglesymbolsauto, 0 },
                                   { " Case-sensitive symbols",togglecasesyms,  0 },
                                   { OSDMENUBAR,               NULL,            0 },
                                   { "Back",                   gotomenu,        0 },
                                   { NULL, } };

struct osdmenu menus[] = { { "Main Menu",         0, mainitems },
                           { "Hardware options", 13, hwopitems },
                           { "Audio options",     3, auopitems },
                           { "Debug options",     3, dbopitems } };

// Load a 24bit BMP for the GUI
SDL_bool gimg_load( struct guiimg *gi )
{
  FILE *f;
  Uint8 hdrbuf[640*3];
  Sint32 x, y;
  SDL_bool fileok;

  // Get the file
  f = fopen( gi->filename, "rb" );
  if( !f )
  {
    printf( "Unable to open '%s'\n", gi->filename );
    return SDL_FALSE;
  }

  // Read the header
  if( fread( hdrbuf, 54, 1, f ) != 1 )
  {
    printf( "Error reading '%s'\n", gi->filename );
    return SDL_FALSE;
  }

  fileok = SDL_TRUE;
  if( strncmp( (char *)hdrbuf, "BM", 2 ) != 0 ) fileok = SDL_FALSE;                                      // Must start with "BM"
  if( ((hdrbuf[21]<<24)|(hdrbuf[20]<<16)|(hdrbuf[19]<<8)|hdrbuf[18]) != gi->w ) fileok = SDL_FALSE;      // Must be the correct width
  if( ((hdrbuf[25]<<24)|(hdrbuf[24]<<16)|(hdrbuf[23]<<8)|hdrbuf[22]) != gi->h ) fileok = SDL_FALSE;      // Must be the correct height
  if( ((hdrbuf[27]<<8)|hdrbuf[26]) != 1 ) fileok = SDL_FALSE;                                            // Must contain one plane
  if( ((hdrbuf[29]<<8)|hdrbuf[28]) != 24 ) fileok = SDL_FALSE;                                           // Must be 24 bit
  if( ((hdrbuf[33]<<24)|(hdrbuf[32]<<16)|(hdrbuf[31]<<8)|hdrbuf[30]) != 0 ) fileok = SDL_FALSE;          // Must not be compressed

  if( !fileok )
  {
    printf( "'%s' needs to be a %dx%d, uncompressed, 24-bit BMP image\n", gi->filename, gi->w, gi->h );
    return SDL_FALSE;
  }

  // Get some RAM!
  if( !gi->buf )
    gi->buf = malloc( gi->w * gi->h * 2 );

  if( !gi->buf )
  {
    printf( "Out of memory\n" );
    return SDL_FALSE;
  }

  // BMPs are upside down!
  for( y=gi->h-1; y>=0; y-- )
  {
    fread( hdrbuf, ((gi->w*3)+3)&0xfffffffc, 1, f );
    for( x=0; x<gi->w; x++ )
      gi->buf[y*gi->w+x] = SDL_MapRGB( screen->format, hdrbuf[x*3+2], hdrbuf[x*3+1], hdrbuf[x*3] );
  }
  
  return SDL_TRUE;
}

// Draw a GUI image at X,Y
void gimg_draw( struct guiimg *gi, Sint32 xp, Sint32 yp )
{
  Uint16 *sptr, *dptr;
  Sint32 x, y;

  sptr = gi->buf;
  dptr = &((Uint16 *)screen->pixels)[pixpitch*yp+xp];

  for( y=0; y<gi->h; y++ )
  {
    for( x=0; x<gi->w; x++ )
      *(dptr++) = *(sptr++);
    dptr += pixpitch-gi->w;
  }
}

// Draw part of an image (xp,yp = screen location, ox, oy = offset into image, w, h = dimensions)
void gimg_drawpart( struct guiimg *gi, Sint32 xp, Sint32 yp, Sint32 ox, Sint32 oy, Sint32 w, Sint32 h )
{
  Uint16 *sptr, *dptr;
  Sint32 x, y;

  sptr = &gi->buf[oy*gi->w+ox];
  dptr = &((Uint16 *)screen->pixels)[pixpitch*yp+xp];

  for( y=0; y<h; y++ )
  {
    for( x=0; x<w; x++ )
      *(dptr++) = *(sptr++);
    sptr += gi->w-w;
    dptr += pixpitch-w;
  }
}

// Draw the statusbar at the bottom
void draw_statusbar( void )
{
  gimg_draw( &gimgs[GIMG_STATUSBAR], 0, GIMG_POS_SBARY );
}

// Overlay the disk icons onto the status bar
void draw_disks( struct machine *oric )
{
  Sint32 i, j;

  if( oric->drivetype == DRV_NONE )
  {
    gimg_drawpart( &gimgs[GIMG_STATUSBAR], GIMG_POS_DISKX, GIMG_POS_SBARY, GIMG_POS_DISKX, 0, 18*4, 16 );
    return;
  }

  for( i=0; i<4; i++ )
  {
    j = GIMG_DISK_EJECTED;
    if( oric->wddisk.disk[i] )
    {
      j = ((oric->wddisk.c_drive==i)&&(oric->wddisk.currentop!=COP_NUFFINK)) ? GIMG_DISK_ACTIVE : GIMG_DISK_IDLE;
      if( oric->wddisk.disk[i]->modified ) j+=2;
    }
    gimg_draw( &gimgs[j], GIMG_POS_DISKX+i*GIMG_W_DISK, GIMG_POS_SBARY );
  }
}

// Overlay the AVI record icon onto the status bar
void draw_avirec( SDL_bool recording )
{
  if( recording )
  {
    gimg_draw( &gimgs[GIMG_AVI_RECORD], GIMG_POS_AVIRECX, GIMG_POS_SBARY );
    return;
  }

  gimg_drawpart( &gimgs[GIMG_STATUSBAR], GIMG_POS_AVIRECX, GIMG_POS_SBARY, GIMG_POS_AVIRECX, 0, 16, 16 );
}

// Overlay the tape icon onto the status bar
void draw_tape( struct machine *oric )
{
  if( !oric->tapebuf )
  {
    gimg_draw( &gimgs[GIMG_TAPE_EJECTED], GIMG_POS_TAPEX, GIMG_POS_SBARY );
    return;
  }

  if( oric->tapemotor )
  {
    gimg_draw( &gimgs[GIMG_TAPE_PLAY], GIMG_POS_TAPEX, GIMG_POS_SBARY );
    return;
  }

  if( oric->tapeoffs >= oric->tapelen )
  {
    gimg_draw( &gimgs[GIMG_TAPE_STOP], GIMG_POS_TAPEX, GIMG_POS_SBARY );
    return;
  }

  gimg_draw( &gimgs[GIMG_TAPE_PAUSE], GIMG_POS_TAPEX, GIMG_POS_SBARY );
}

// Info popups
static int popuptime=0;
static char popupstr[40];

// Pop up some info!
void do_popup( char *str )
{
  int i;
  strncpy( popupstr, str, 40 ); popupstr[39] = 0;
  for( i=strlen(popupstr); i<39; i++ ) popupstr[i] = 32;
  popuptime = 100;
}

// Top-level rendering routine
void render( struct machine *oric )
{
  char tmp[64];
  int perc, fps; //, i;

  if( SDL_MUSTLOCK( screen ) )
    SDL_LockSurface( screen );

  switch( oric->emu_mode )
  {
    case EM_MENU:
      video_show( oric );
      if( tz[TZ_MENU] ) draw_textzone( tz[TZ_MENU] );
      break;

    case EM_RUNNING:
      video_show( oric );
      if( showfps )
      {
        fps = 100000/(frametimeave?frametimeave:1);
        if( oric->vid_freq )
          perc = 200000/(frametimeave?frametimeave:1);
        else
          perc = 166667/(frametimeave?frametimeave:1);
        sprintf( tmp, "%4d.%02d%% - %4dFPS", perc/100, perc%100, fps/100 );
        statusprintstr( 0, gpal[1], tmp );
      }
      if( popuptime > 0 )
      {
        popuptime--;
        if( popuptime == 0 )
          printstr( 320, 0, gpal[1], gpal[4], "                                        " );
        else
          printstr( 320, 0, gpal[1], gpal[4], popupstr );
      }
      break;

    case EM_DEBUG:
      mon_render( oric );
      break;
  }

  if( SDL_MUSTLOCK( screen ) )
    SDL_UnlockSurface( screen );

  SDL_Flip( screen );
}

// Print a char onto a 16-bit framebuffer
//   ptr       = where to draw it
//   ch        = which char to draw
//   fcol      = foreground colour
//   bcol      = background colour
//   solidfont = use background colour
static void printchar( Uint16 *ptr, unsigned char ch, Uint16 fcol, Uint16 bcol, SDL_bool solidfont )
{
  int px, py, c;
  unsigned char *fptr;

  if( ch > 127 ) return;

  fptr = &thefont[ch*12];

  for( py=0; py<12; py++ )
  {
    for( c=0x80, px=0; px<8; px++, c>>=1 )
    {
      if( (*fptr)&c )
      {
        *(ptr++) = fcol;
      } else {
        if( solidfont )
          *(ptr++) = bcol;
        else
          ptr++;
      }
    }

    ptr += pixpitch - 8;
    fptr++;
  }
}

// Draws a box in a textzone (uses the box chars in the font)
//   ptz     = target textzone
//   x,y,w,h = location and dimensions
//   fg,bg   = colours
void makebox( struct textzone *ptz, int x, int y, int w, int h, int fg, int bg )
{
  int cx, cy, o, bo;

  o = y*ptz->w+x;
  for( cy=0; cy<h; cy++ )
  {
    for( cx=0; cx<w; cx++ )
    {
      ptz->tx[o  ] = ' ';
      ptz->fc[o  ] = fg;
      ptz->bc[o++] = bg;
    }
    ptz->tx[o-w] = 5;
    ptz->tx[o-1] = 5;
    o += ptz->w-w;
  }


  o = y*ptz->w+x;
  bo = o + (h-1)*ptz->w;
  for( cx=0; cx<(w-1); cx++ )
  {
    ptz->tx[o++] = cx==0?1:2;
    ptz->tx[bo++] = cx==0?9:2;
  }
  ptz->tx[o] = 4;
  ptz->tx[bo] = 11;
}

// Write a string to a textzone
void tzstr( struct textzone *ptz, char *text )
{
  int i, o;

  o = ptz->py*ptz->w+ptz->px;
  for( i=0; text[i]; i++ )
  {
    switch( text[i] )
    {
      case 10:
        ptz->px = 1;
        ptz->py++;
        o = ptz->py*ptz->w+1;
        break;
      
      case 13:
        break;
      
      default:
        ptz->tx[o  ] = text[i];
        ptz->fc[o  ] = ptz->cfc;
        ptz->bc[o++] = ptz->cbc;
        ptz->px++;
        if( ptz->px >= ptz->w )
        {
          ptz->px = 1;
          ptz->py++;
          o = ptz->py*ptz->w+1;
        }
        break;
    }
  }
}

// Print a formatted string into a textzone
void tzprintf( struct textzone *ptz, char *fmt, ... )
{
  va_list ap;
  va_start( ap, fmt );
  if( vsnprintf( vsptmp, VSPTMPSIZE, fmt, ap ) != -1 )
  {
    vsptmp[VSPTMPSIZE-1] = 0;
    tzstr( ptz, vsptmp );
  }
  va_end( ap );
}

// Write a string to a textzone at a specific location
void tzstrpos( struct textzone *ptz, int x, int y, char *text )
{
  ptz->px = x;
  ptz->py = y;
  tzstr( ptz, text );
}

// Print a formatted string into a textzone at a specific location
void tzprintfpos( struct textzone *ptz, int x, int y, char *fmt, ... )
{
  va_list ap;
  va_start( ap, fmt );
  if( vsnprintf( vsptmp, VSPTMPSIZE, fmt, ap ) != -1 )
  {
    vsptmp[VSPTMPSIZE-1] = 0;
    tzstrpos( ptz, x, y, vsptmp );
  }
  va_end( ap );
}

// Set the foreground and background colours for printing text into a textzone
void tzsetcol( struct textzone *ptz, int fc, int bc )
{
  ptz->cfc = fc;
  ptz->cbc = bc;
}

// Render a textzone onto the SDL surface
void draw_textzone( struct textzone *ptz )
{
  int x, y, o;
  Uint16 *sp;

  sp = &((Uint16 *)screen->pixels)[pixpitch*ptz->y+ptz->x];
  o = 0;
  for( y=0; y<ptz->h; y++ )
  {
    for( x=0; x<ptz->w; x++, o++ )
    {
      printchar( sp, ptz->tx[o], gpal[ptz->fc[o]], gpal[ptz->bc[o]], SDL_TRUE );
      sp += 8;
    }
    sp += pixpitch*12-8*ptz->w;
  }
}

// Print a string directly onto the SDL surface
//   x,y   = location
//   fc,bc = colours
//   str   = string
void printstr( int x, int y, Uint16 fc, Uint16 bc, char *str )
{
  Uint16 *ptr;
  int i;

  ptr = &((Uint16 *)screen->pixels)[pixpitch*y+x];
  for( i=0; str[i]; i++, ptr += 8)
    printchar( ptr, str[i], fc, bc, SDL_TRUE );
}

static void statusprintchar( int x, Uint16 fc, char c )
{
  int px, py, cp;
  unsigned char *fptr;
  Uint16 *ptr, *statptr;

  if( c&0x80 ) return;

  ptr = &((Uint16 *)screen->pixels)[pixpitch*(GIMG_POS_SBARY+2)+x];
  statptr = &gimgs[GIMG_STATUSBAR].buf[2*640+x];

  fptr = &thefont[c*12];

  for( py=0; py<12; py++ )
  {
    for( cp=0x80, px=0; px<8; px++, cp>>=1, ptr++, statptr++ )
    {
      if( (*fptr)&cp )
      {
        *ptr = fc;
      } else {
        *ptr = *statptr;
      }
    }

    ptr += pixpitch - 8;
    statptr += 640-8;
    fptr++;
  }
}

void statusprintstr( int x, Uint16 fc, char *str )
{
  int i;
  for( i=0; str[i]; i++, x+=8 )
    statusprintchar( x, fc, str[i] );
}

// Set the title of a textzone
void tzsettitle( struct textzone *ptz, char *title )
{
  int ox, oy;
  makebox( ptz, 0, 0, ptz->w, ptz->h, 2, 3 );
  if( !title ) return;

  tzsetcol( ptz, 2, 3 );
  ox = ptz->px;
  oy = ptz->py;
  ptz->px = 3;
  ptz->py = 0;
  tzstr( ptz, "[ " );
  tzstr( ptz, title );
  tzstr( ptz, " ]" );
  ptz->px = ox;
  ptz->py = oy;
}

// Allocate a textzone structure
struct textzone *alloc_textzone( int x, int y, int w, int h, char *title )
{
  struct textzone *ntz;

  ntz = malloc( sizeof( struct textzone ) + w*h*3 );
  if( !ntz ) return NULL;

  ntz->x = x;
  ntz->y = y;
  ntz->w = w;
  ntz->h = h;

  ntz->tx = (unsigned char *)(&ntz[1]);
  ntz->fc = &ntz->tx[w*h];
  ntz->bc = &ntz->fc[w*h];

  tzsettitle( ntz, title );

  ntz->px = 1;
  ntz->py = 1;

  return ntz;
}

// Free a textzone structure
void free_textzone( struct textzone *ptz )
{
  if( !ptz ) return;
  free( ptz );
}

// Draw the current menu items into the menu textzone
void drawitems( void )
{
  int i, j, o;

  // No menu, or no textzone?
  if( ( !cmenu ) || ( !tz[TZ_MENU] ) )
    return;

  // For each item...
  for( i=0; cmenu->items[i].name; i++ )
  {
    // Check if its a menu bar
    if( cmenu->items[i].name == OSDMENUBAR )
    {
      // Fill it with the menu bar char
      o = tz[TZ_MENU]->w * (i+1) + 1;
      for( j=1; j<tz[TZ_MENU]->w-1; j++, o++ )
      {
        tz[TZ_MENU]->tx[o] = 12;
        tz[TZ_MENU]->fc[o] = 2;
        tz[TZ_MENU]->bc[o] = 3;
      }
      continue;
    }

    // Check if this is the highlighted item, and set the colours accordingly
    if( i==cmenu->citem )
      tzsetcol( tz[TZ_MENU], 1, 5 );
    else
      tzsetcol( tz[TZ_MENU], 2, 3 );

    // Calculate the position in the textzone
    o = tz[TZ_MENU]->w * (i+1) + 1;

    // Fill the background colour all the way along
    for( j=1; j<tz[TZ_MENU]->w-1; j++, o++ )
      tz[TZ_MENU]->bc[o] = tz[TZ_MENU]->cbc;
    
    // Write the text for the item
    tzstrpos( tz[TZ_MENU], 1, i+1, cmenu->items[i].name );
  }
}


/***************** These functions can be used directly from the menu system ***************/
void joinpath( char *path, char *file )
{
#if defined(__amigaos4__)
  strcpy( filetmp, path );
  IDOS->AddPart( filetmp, file, 4096 );
#elif defined(WIN32)
  int i;
  strncpy( filetmp, path, 4096 ); filetmp[4095] = 0;
  i = strlen( filetmp );
  if( ( i > 0 ) && ( filetmp[i-1] != PATHSEP ) && ( filetmp[i-1] != ':' ) )
  {
    filetmp[i++] = PATHSEP;
    filetmp[i++] = 0;
  }
  strncat( filetmp, file, 4096+512 );
  filetmp[4096+511] = 0;
#else
  int i;
  strncpy( filetmp, path, 4096 ); filetmp[4095] = 0;
  i = strlen( filetmp );
  if( ( i > 0 ) && ( filetmp[i-1] != PATHSEP ) )
  {
    filetmp[i++] = PATHSEP;
    filetmp[i++] = 0;
  }
  strncat( filetmp, file, 4096+512 );
  filetmp[4096+511] = 0;
#endif
}

// "insert" a tape into the virtual tape drive, via filerequester
void inserttape( struct machine *oric, struct osdmenuitem *mitem, int dummy )
{
  if( !filerequester( oric, "Insert tape", tapepath, tapefile, FR_TAPES ) ) return;
  joinpath( tapepath, tapefile );
  tape_load_tap( oric, filetmp );
  if( oric->symbolsautoload ) mon_new_symbols( "symbols", SDL_TRUE );
  setemumode( oric, NULL, EM_RUNNING );
}

// "insert" a disk into the virtual disk drive, via filerequester
void insertdisk( struct machine *oric, struct osdmenuitem *mitem, int drive )
{
  if( !filerequester( oric, "Insert disk", diskpath, diskfile, FR_DISKLOAD ) ) return;
  joinpath( diskpath, diskfile );
  diskimage_load( oric, filetmp, drive );

  if( oric->drivetype == DRV_NONE )
  {
    swapmach( oric, NULL, (DRV_MICRODISC<<16)|oric->type );
//    setemumode( oric, NULL, EM_DEBUG );
    return;
  }
  setemumode( oric, NULL, EM_RUNNING );
}

/*
void savesnap( struct machine *oric, struct osdmenuitem *mitem, int dummy )
{
  if( !filerequester( oric, "Save Snapshot", snappath, snapfile, FR_SNAPSHOTSAVE ) ) return;
  joinpath( snappath, snapfile );
  save_snapshot( oric, filetmp );
}
*/

// Reset the oric
void resetoric( struct machine *oric, struct osdmenuitem *mitem, int dummy )
{
  switch( oric->drivetype )
  {
    case DRV_MICRODISC:
      oric->romdis = SDL_TRUE;
      microdisc_init( &oric->md, &oric->wddisk, oric );
      break;
    
    case DRV_JASMIN:
      oric->romdis = SDL_TRUE;
      break;
    
    default:
      oric->romdis = SDL_FALSE;
      break;
  }
  m6502_reset( &oric->cpu );
  via_init( &oric->via, oric );
  ay_init( &oric->ay, oric );
  oric->cpu.rastercycles = oric->cyclesperraster;
  oric->frames = 0;
  if( oric->autorewind ) tape_rewind( oric );
  setemumode( oric, NULL, EM_RUNNING );  
}

// Turn tape noise on/off
void toggletapenoise( struct machine *oric, struct osdmenuitem *mitem, int dummy )
{
  if( oric->tapenoise )
  {
    oric->ay.tapeout = 0;
    oric->tapenoise = SDL_FALSE;
    mitem->name = " Tape noise";
    return;
  }

  oric->tapenoise = SDL_TRUE;
  mitem->name = "\x0e""Tape noise";
}

// Toggle sound on/off
void togglesound( struct machine *oric, struct osdmenuitem *mitem, int dummy )
{
  if( ( soundon ) || (!soundavailable) )
  {
    soundon = SDL_FALSE;
    oric->ay.soundon = SDL_FALSE;
    mitem->name = " Sound enabled";
    if( soundavailable ) SDL_PauseAudio( 1 );
    return;
  }

  soundon = SDL_TRUE;
  oric->ay.soundon = !warpspeed;
  mitem->name = "\x0e""Sound enabled";
  if( oric->emu_mode == EM_RUNNING ) SDL_PauseAudio( !warpspeed );
}

// Toggle turbotape on/off
void toggletapeturbo( struct machine *oric, struct osdmenuitem *mitem, int dummy )
{
  if( oric->tapeturbo )
  {
    oric->tapeturbo = SDL_FALSE;
    mitem->name = " Turbo tape";
    return;
  }

  oric->tapeturbo = SDL_TRUE;
  mitem->name = "\x0e""Turbo tape";
}

// Toggle VSync Hack
void togglevsynchack( struct machine *oric, struct osdmenuitem *mitem, int dummy )
{
  if( oric->vsynchack )
  {
    oric->vsynchack = SDL_FALSE;
    mitem->name = " VSync hack";
    return;
  }

  oric->vsynchack = SDL_TRUE;
  mitem->name = "\x0e""VSync hack";
}

// Toggle symbols autoload
void togglesymbolsauto( struct machine *oric, struct osdmenuitem *mitem, int dummy )
{
  if( oric->symbolsautoload )
  {
    oric->symbolsautoload = SDL_FALSE;
    mitem->name = " Autoload symbols file";
    return;
  }

  oric->symbolsautoload = SDL_TRUE;
  mitem->name = "\x0e""Autoload symbols file";
}

// Toggle case sensitive symbols
void togglecasesyms( struct machine *oric, struct osdmenuitem *mitem, int dummy )
{
  if( oric->symbolscase )
  {
    oric->symbolscase = SDL_FALSE;
    mitem->name = " Case-sensitive symbols";
    return;
  }

  oric->symbolscase = SDL_TRUE;
  mitem->name = "\x0e"" Case-sensitive symbols";
}

// Toggle autorewind on/off
void toggleautowind( struct machine *oric, struct osdmenuitem *mitem, int dummy )
{
  if( oric->autorewind )
  {
    oric->autorewind = SDL_FALSE;
    mitem->name = " Autorewind tape";
    return;
  }

  oric->autorewind = SDL_TRUE;
  mitem->name = "\x0e""Autorewind tape";
}

// Toggle autoinsert on/off
void toggleautoinsrt( struct machine *oric, struct osdmenuitem *mitem, int dummy )
{
  if( oric->autoinsert )
  {
    oric->autoinsert = SDL_FALSE;
    mitem->name = " Autoinsert tape";
    return;
  }

  oric->autoinsert = SDL_TRUE;
  mitem->name = "\x0e""Autoinsert tape";
}

// Go to a different menu
void gotomenu( struct machine *oric, struct osdmenuitem *mitem, int menunum )
{
  int i, w;

  if( tz[TZ_MENU] ) { free_textzone( tz[TZ_MENU] ); tz[TZ_MENU] = NULL; }

  cmenu = &menus[menunum];
  w = strlen( cmenu->title )+8;
  for( i=0; cmenu->items[i].name; i++ )
  {
    if( cmenu->items[i].name == OSDMENUBAR )
      continue;
    if( strlen( cmenu->items[i].name ) > w )
      w = strlen( cmenu->items[i].name );
  }
  w+=2; i+=2;

  tz[TZ_MENU] = alloc_textzone( 320-w*4, 240-i*6, w, i, cmenu->title );
  if( !tz[TZ_MENU] )
  {
    cmenu = NULL;
    oric->emu_mode = EM_RUNNING;
    return;
  }

  drawitems();
}

/************************* End of menu callable funcs *******************************/

// This is the event handler for when you are in the menus
SDL_bool menu_event( SDL_Event *ev, struct machine *oric, SDL_bool *needrender )
{
  SDL_bool done = SDL_FALSE;
  int i, x, y;

  // Wot, no menu?!
  if( ( !cmenu ) || ( !tz[TZ_MENU] ) )
    return done;

  x = -1;
  y = -1;
  switch( ev->type )
  {
    case SDL_MOUSEMOTION:
      x = (ev->motion.x - tz[TZ_MENU]->x)/8;
      y = (ev->motion.y - tz[TZ_MENU]->y)/12-1;
      break;

    case SDL_MOUSEBUTTONDOWN:
      if( ev->button.button == SDL_BUTTON_LEFT )
      {
        x = (ev->button.x - tz[TZ_MENU]->x)/8;
        y = (ev->button.y - tz[TZ_MENU]->y)/12-1;
      }
      break;
  }

  switch( ev->type )
  {
    case SDL_MOUSEMOTION:
    case SDL_MOUSEBUTTONDOWN:
      x = (ev->motion.x - tz[TZ_MENU]->x)/8;
      y = (ev->motion.y - tz[TZ_MENU]->y)/12-1;

      if( ( x < 0 ) || ( y < 0 ) ||
          ( x >= tz[TZ_MENU]->w ) )
        break;

      for( i=0; i<y; i++ )
        if( cmenu->items[i].name == NULL )
          break;

      if( ( i != y ) || ( cmenu->items[y].name == OSDMENUBAR ) || ( cmenu->items[y].func == NULL ) )
        break;

      cmenu->citem = y;

      if( ( ev->type == SDL_MOUSEBUTTONDOWN ) &&
          ( (ev->button.button==SDL_BUTTON_LEFT) ||
            (ev->button.button==SDL_BUTTON_RIGHT) ) )
        cmenu->items[cmenu->citem].func( oric, &cmenu->items[cmenu->citem], cmenu->items[cmenu->citem].arg );

      drawitems();
      *needrender = SDL_TRUE;    
      break;
    
    case SDL_KEYDOWN:
      switch( ev->key.keysym.sym )
      {
        case SDLK_UP:
          // Find the next selectable item above this one
          i = cmenu->citem-1;
          while( ( cmenu->items[i].name == OSDMENUBAR ) ||
                 ( cmenu->items[i].func == NULL ) )
          {
            if( i < 0 ) break;
            i--;
          }

          // Hit the top?
          if( ( i < 0 ) || ( cmenu->items[i].name == OSDMENUBAR ) )
          {
            // Start the search again from the last item
            for( i=0; cmenu->items[i].name; i++ ) ;
            i--;

            // Find the first selectable item from here
            while( ( cmenu->items[i].name == OSDMENUBAR ) ||
                   ( cmenu->items[i].func == NULL ) )
            {
              if( i < 0 ) break;
              i--;
            }
            if( ( i < 0 ) || ( cmenu->items[i].name == OSDMENUBAR ) ) break;
          }
          cmenu->citem = i;
          drawitems();
          *needrender = SDL_TRUE;
          break;
        
        case SDLK_DOWN:
          i = cmenu->citem+1;
          while( ( cmenu->items[i].name == OSDMENUBAR ) ||
                 ( cmenu->items[i].func == NULL ) )
          {
            if( cmenu->items[i].name == NULL ) break;
            i++;
          }

          if( ( cmenu->items[i].name == NULL ) || ( cmenu->items[i].name == OSDMENUBAR ) )
          {
            i=0;

            while( ( cmenu->items[i].name == OSDMENUBAR ) ||
                   ( cmenu->items[i].func == NULL ) )
            {
              if( cmenu->items[i].name == NULL ) break;
              i++;
            }

            if( ( cmenu->items[i].name == NULL ) || ( cmenu->items[i].name == OSDMENUBAR ) )
              break;
          }

          cmenu->citem = i;
          drawitems();
          *needrender = SDL_TRUE;
          break;
        
        default:
          break;
      }
      break;

    case SDL_KEYUP:
      switch( ev->key.keysym.sym )
      {
        case SDLK_RETURN:
          if( !cmenu->items[cmenu->citem].func ) break;
          cmenu->items[cmenu->citem].func( oric, &cmenu->items[cmenu->citem], cmenu->items[cmenu->citem].arg );
          drawitems();
          *needrender = SDL_TRUE;
          break;

        case SDLK_ESCAPE:
          setemumode( oric, NULL, EM_RUNNING );
          *needrender = SDL_TRUE;
          break;
        
        default:
          break;
      }
      break;
  }
  return done;
}

// Set things to default that can't fail
void preinit_gui( void )
{
  int i;
  for( i=0; i<TZ_LAST; i++ ) tz[i] = NULL;
  for( i=0; i<GIMG_LAST; i++ ) gimgs[i].buf = NULL;
  strcpy( tapepath, FILEPREFIX"tapes" );
  strcpy( tapefile, "" );
  strcpy( diskpath, FILEPREFIX"disks" );
  strcpy( diskfile, "" );
//  strcpy( snappath, FILEPREFIX"snapshots" );
//  strcpy( snapfile, "" );
}

// Ensure the sanity of toggle menuitems
void setmenutoggles( struct machine *oric )
{
  if( soundavailable && soundon )
    auopitems[0].name = "\x0e""Sound enabled";
  else
    auopitems[0].name = " Sound enabled";

  if( oric->tapenoise )
    auopitems[1].name = "\x0e""Tape noise";
  else
    auopitems[1].name = " Tape noise";

  if( oric->tapeturbo )
    hwopitems[9].name = "\x0e""Turbo tape";
  else
    hwopitems[9].name = " Turbo tape";

  if( oric->autoinsert )
    hwopitems[10].name = "\x0e""Autoinsert tape";
  else
    hwopitems[10].name = " Autoinsert tape";

  if( oric->autorewind )
    hwopitems[11].name = "\x0e""Autorewind tape";
  else
    hwopitems[11].name = " Autorewind tape";

  if( oric->vsynchack )
    hwopitems[13].name = "\x0e""VSync hack";
  else
    hwopitems[13].name = " VSync hack";

  if( oric->symbolsautoload )
    dbopitems[0].name = "\x0e""Autoload symbols file";
  else
    dbopitems[0].name = " Autoload symbols file";

  if( oric->symbolscase )
    dbopitems[1].name = "\x0e""Case-sensitive symbols";
  else
    dbopitems[1].name = " Case-sensitive symbols";

  hwopitems[6].func = microdiscrom_valid ? setdrivetype : NULL;
  hwopitems[7].func = jasminrom_valid ? setdrivetype : NULL;

  hwopitems[5].name = oric->drivetype==DRV_NONE      ? "\x0e""No disk"   : " No disk";
  hwopitems[6].name = oric->drivetype==DRV_MICRODISC ? "\x0e""Microdisc" : " Microdisc";
  hwopitems[7].name = oric->drivetype==DRV_JASMIN    ? "\x0e""Jasmin"    : " Jasmin";
}

// Initialise the GUI
SDL_bool init_gui( struct machine *oric )
{
  int i;
  SDL_AudioSpec wanted;
  Sint32 surfacemode;

  // Go SDL!
  if( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO ) < 0 )
  {
    printf( "SDL init failed\n" );
    return SDL_FALSE;
  }
  need_sdl_quit = SDL_TRUE;

  surfacemode = fullscreen ? SDL_FULLSCREEN : SDL_SWSURFACE;
  if( hwsurface ) { surfacemode &= ~SDL_SWSURFACE; surfacemode |= SDL_HWSURFACE; }

  SDL_WM_SetIcon( SDL_LoadBMP( IMAGEPREFIX"winicon.bmp" ), NULL );

  screen = SDL_SetVideoMode( 640, 480, 16, surfacemode );
  if( !screen )
  {
    printf( "SDL video failed\n" );
    return SDL_FALSE;
  }

  // Set up SDL audio
  wanted.freq     = AUDIO_FREQ; 
  wanted.format   = AUDIO_S16SYS; 
  wanted.channels = 2; /* 1 = mono, 2 = stereo */
  wanted.samples  = AUDIO_BUFLEN;

  wanted.callback = (void*)ay_callback;
  wanted.userdata = &oric->ay;

  soundavailable = SDL_FALSE;
  soundon = SDL_FALSE;
  if( SDL_OpenAudio( &wanted, NULL ) >= 0 )
  {
    soundon = SDL_TRUE;
    soundavailable = SDL_TRUE;
  }

//  SDL_WM_SetCaption( "Oricutron WIP", "Oricutron WIP" );
  SDL_WM_SetCaption( "Oricutron " VERSION_FULL, "Oricutron " VERSION_FULL );

  // Get the GUI palette
  for( i=0; i<NUM_GUI_COLS; i++ )
    gpal[i] = SDL_MapRGB( screen->format, sgpal[i*3  ], sgpal[i*3+1], sgpal[i*3+2] );

  pixpitch = screen->pitch / 2;

  // Allocate all text zones
  tz[TZ_MONITOR] = alloc_textzone( 0, 228, 50, 21, "Monitor" );
  if( !tz[TZ_MONITOR] ) { printf( "Out of memory\n" ); return SDL_FALSE; }
  tz[TZ_DEBUG] = alloc_textzone( 0, 228, 50, 21, "Debug console" );
  if( !tz[TZ_DEBUG] ) { printf( "Out of memory\n" ); return SDL_FALSE; }
  tz[TZ_MEMWATCH] = alloc_textzone( 0, 228, 50, 21, "Memory watch" );
  if( !tz[TZ_MEMWATCH] ) { printf( "Out of memory\n" ); return SDL_FALSE; }
  tz[TZ_REGS] = alloc_textzone( 240, 0, 50, 19, "6502 Status" );
  if( !tz[TZ_REGS] ) { printf( "Out of memory\n" ); return SDL_FALSE; }
  tz[TZ_VIA]  = alloc_textzone( 400, 228, 30, 21, "VIA Status" );
  if( !tz[TZ_VIA] ) { printf( "Out of memory\n" ); return SDL_FALSE; }
  tz[TZ_AY]   = alloc_textzone( 400, 228, 30, 21, "AY Status" );
  if( !tz[TZ_AY] ) { printf( "Out of memory\n" ); return SDL_FALSE; }
  tz[TZ_DISK]   = alloc_textzone( 400, 228, 30, 21, "Disk Status" );
  if( !tz[TZ_DISK] ) { printf( "Out of memory\n" ); return SDL_FALSE; }

  for( i=0; i<GIMG_LAST; i++ )
  {
    if( !gimg_load( &gimgs[i] ) ) return SDL_FALSE;
  }

  setmenutoggles( oric );
  return SDL_TRUE;
}

// Bye bye.
void shut_gui( void )
{
  int i;

  for( i=0; i<TZ_LAST; i++ )
    free_textzone( tz[i] );

  if( need_sdl_quit ) SDL_Quit();
}
