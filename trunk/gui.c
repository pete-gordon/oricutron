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
**  GUI
*/

/*
** The GUI in Oriculator is built on the basic element of the "textzone". This
** is a simple structure containing the position, size and contents of an area
** of text, which you can render onto an SDL surface whenever you like.
*/

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

#include <SDL/SDL.h>

#include "6502.h"
#include "via.h"
#include "8912.h"
#include "gui.h"
#include "disk.h"
#include "machine.h"
#include "monitor.h"

SDL_Surface *screen = NULL;
SDL_bool need_sdl_quit = SDL_FALSE;
SDL_bool soundavailable, soundon;
extern SDL_bool microdiscrom_valid, jasminrom_valid;

char tapepath[4096], tapefile[512];
char diskpath[4096], diskfile[512];

// A directory entry in the file requester
struct frq_ent
{
  char name[512];
  char showname[40];
  SDL_bool isdir;
};

// A text input box (used in the file requester)
struct frq_textbox
{
  char *buf;
  int x, y, w;
  int maxlen, slen, cpos, vpos;
};

// The actual text box structures used by the file requester
static struct frq_textbox freqf_tbox[] = { { NULL, 7, 28, 32, 4096, 0, 0, 0 },
                                           { NULL, 7, 30, 32,  512, 0, 0, 0 } };

// File requester state
static int freqf_size=0, freqf_used=0, freqf_cgad=2;
static struct frq_ent *freqfiles=NULL;
static int freqf_clicktime=0;

// Our "lovely" hand-coded font
extern unsigned char thefont[];

// Believe it or not, i have defined more than 2 shades of blue :)
#define NUM_GUI_COLS 8

// Text zone (and other gui area) colours
unsigned char sgpal[] = { 0x00, 0x00, 0x00,     // 0 = black
                          0xff, 0xff, 0xff,     // 1 = white
                          0xcc, 0xcc, 0xff,     // 2 = light blue
                          0x00, 0x00, 0xff,     // 3 = dark blue
                          0x00, 0x00, 0x40,     // 4 = very dark blue
                          0x70, 0x70, 0xff,     // 5 = mid blue
                          0x80, 0x80, 0x80,     // 6 = grey
                          0xa0, 0xa0, 0x00 };   // 7 = yellow

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

// Menu definitions. Name, function, parameter
struct osdmenuitem mainitems[] = { { "Insert tape...",         inserttape,      0 },
                                   { "Insert disk 0...",       insertdisk,      0 },
                                   { "Insert disk 1...",       insertdisk,      1 },
                                   { "Insert disk 2...",       insertdisk,      2 },
                                   { "Insert disk 3...",       insertdisk,      3 },
                                   { OSDMENUBAR,               NULL,            0 },
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

struct osdmenu menus[] = { { "Main Menu",        12, mainitems },
                           { "Hardware options", 13, hwopitems },
                           { "Audio options",     3, auopitems },
                           { "Debug options",     3, dbopitems } };

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
      if( showfps )
      {
        if( ( warpspeed ) || ( frametimeave > 20 ) )
        {
          fps = 100000/(frametimeave?frametimeave:1);
          perc = 200000/(frametimeave?frametimeave:1);
          sprintf( tmp, "%4d.%02d%% - %4dFPS - %3d ms/frame ", perc/100, perc%100, fps/100, frametimeave );
        } else {
          sprintf( tmp, " 100.00%% -   50FPS - %3d ms/frame", frametimeave );
        }
      }
      video_show( oric );
      if( showfps ) printstr( 0, 0, gpal[1], gpal[4], tmp );
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

// Render the filerequester
void filereq_render( struct machine *oric )
{
  if( SDL_MUSTLOCK( screen ) )
    SDL_LockSurface( screen );

  video_show( oric );
  draw_textzone( tz[TZ_FILEREQ] );

  if( SDL_MUSTLOCK( screen ) )
    SDL_UnlockSurface( screen );

  SDL_Flip( screen );
}

// Add a file to the list of files to show in the file requester
static void filereq_addent( SDL_bool isdir, char *name, char *showname )
{
  struct frq_ent *tmpf;
  int i, j;

  if( ( !freqfiles ) || ( freqf_used >= freqf_size ) )
  {
    tmpf = (struct frq_ent *)realloc( freqfiles, sizeof( struct frq_ent ) * (freqf_size+8) );
    if( !tmpf ) return;

    freqfiles   = tmpf;
    freqf_size += 8;
  }

  j = freqf_used;
  if( isdir )
  {
    for( j=0; j<freqf_used; j++ )
      if( !freqfiles[j].isdir ) break;

    if( j < freqf_used )
    {
      for( i=freqf_used; i>j; i-- )
        freqfiles[i] = freqfiles[i-1];
    }
  }

  freqfiles[j].isdir = isdir;
  strncpy( freqfiles[j].name, name, 512 );
  strncpy( freqfiles[j].showname, showname, 38 );
  freqfiles[j].name[511] = 0;
  freqfiles[j].showname[37] = 0;
  freqf_used++;
}

// Scan a directory to show in the filerequester
static SDL_bool filereq_scan( char *path )
{
  DIR *dh;
  struct dirent *de;
  struct stat sb;
  char *odir, *tpath;

  tpath = path;
#ifndef __amigaos4__
  if( !path[0] )
    tpath = ".";
#endif

  freqf_used = 0;

  dh = opendir( tpath );
  if( !dh ) return SDL_FALSE;

  odir = getcwd( NULL, 0 );
  chdir( tpath );

  filereq_addent( SDL_TRUE, "", "[Parent]" );

  while( ( de = readdir( dh ) ) )
  {
    if( ( strcmp( de->d_name, "." ) == 0 ) ||
        ( strcmp( de->d_name, ".." ) == 0 ) )
      continue;
    stat( de->d_name, &sb );
    filereq_addent( S_ISDIR(sb.st_mode), de->d_name, de->d_name );
  }

  closedir( dh );

  chdir( odir );
  free( odir );
  return SDL_TRUE;
}

// Render the file list into the filerequester textzone
static void filereq_showfiles( int offset, int cfile )
{
  int i, j, o;
  struct textzone *ptz;

  if( freqf_cgad != 2 )
    cfile = -1;

  ptz = tz[TZ_FILEREQ];
  for( i=0; i<26; i++ )
  {
    if( (i+offset) < freqf_used )
      tzsetcol( ptz, freqfiles[i+offset].isdir ? 1 : 0, (i+offset)==cfile ? 7 : 6 );
    else
      tzsetcol( ptz, 0, 6 );

    o = (i+1)*ptz->w+1;
    for( j=0; j<38; j++, o++ )
    {
      ptz->fc[o] = ptz->cfc;
      ptz->bc[o] = ptz->cbc;
      ptz->tx[o] = 32;
    }

    if( (i+offset) >= freqf_used )
      continue;

    tzstrpos( ptz, 1, i+1, freqfiles[i+offset].showname );
  }
}

// Set a filerequester textbox contents tothe specified string
static void filereq_settbox( struct frq_textbox *tb, char *buf )
{
  tb->buf  = buf;
  tb->slen = strlen( buf );
  tb->cpos = tb->slen;
  tb->vpos = tb->slen > (tb->w-1) ? tb->slen-(tb->w-1) : 0;
}

// Draw a textbox into the filerequester textzone
static void filereq_drawtbox( struct frq_textbox *tb, SDL_bool active )
{
  struct textzone *ptz = tz[TZ_FILEREQ];
  int i, j, o;

  tzsetcol( ptz, 0, active ? 7 : 6 );
  o = (tb->y*ptz->w)+tb->x;
  for( i=0,j=tb->vpos; i<tb->w; i++, o++, j++ )
  {
    if( ( active ) && ( j==tb->cpos ) )
    {
      ptz->fc[o] = ptz->cbc;
      ptz->bc[o] = ptz->cfc;
    } else {
      ptz->fc[o] = ptz->cfc;
      ptz->bc[o] = ptz->cbc;
    }
    if( j < tb->slen )
      ptz->tx[o] = tb->buf[j];
    else
      ptz->tx[o] = 32;
  }
}

// This sets the file textbox to the currently highlighted file
// It also stores the filename in "fname"
static void filereq_setfiletbox( int cfile, char *fname )
{
  if( ( cfile < 0 ) || ( cfile >= freqf_used ) ||
      ( freqfiles[cfile].isdir ) )
  {
    fname[0] = 0;
    freqf_tbox[1].cpos=0;
    freqf_tbox[1].vpos=0;
    freqf_tbox[1].slen=0;
    return;
  }

  strncpy( fname, freqfiles[cfile].name, 512 );
  fname[511] = 0;
  filereq_settbox( &freqf_tbox[1], fname );
}

// This routine displays a file requester and waits for the user to select a file
//   title = title at the top of the requester
//   path  = initial path to show
//   fname = initial contents of the filename textbox
// It returns true if the user selected a file, although the file is not
// guarranteed to exist.
SDL_bool filerequester( struct machine *oric, char *title, char *path, char *fname )
{
  SDL_Event event;
  struct textzone *ptz;
  struct frq_textbox *tb;
  int top=0,cfile=0,i,mx,my,mclick,key;

  ptz = tz[TZ_FILEREQ];

  filereq_settbox( &freqf_tbox[0], path );
  filereq_settbox( &freqf_tbox[1], fname );

  tzsettitle( ptz, title );
  tzsetcol( ptz, 2, 3 );
  tzstrpos( ptz, 1, 28, "Path:" );
  tzstrpos( ptz, 1, 30, "File:" );

  if( !filereq_scan( path ) )
  {
    if( path[0] )
    {
      path[0] = 0;
      filereq_settbox( &freqf_tbox[0], path );
      if( !filereq_scan( path ) ) return SDL_FALSE;
    } else {
      return SDL_FALSE;
    }
  }
  filereq_showfiles( top, cfile );
  filereq_drawtbox( &freqf_tbox[0], freqf_cgad==0 );
  filereq_drawtbox( &freqf_tbox[1], freqf_cgad==1 );
  filereq_render( oric );

  for( ;; )
  {
    if( !SDL_WaitEvent( &event ) ) return SDL_FALSE;

    mx = -1;
    my = -1;
    key = -1;
    mclick = 0;
    switch( event.type )
    {
      case SDL_KEYUP:
      case SDL_KEYDOWN:
        key = event.key.keysym.unicode;
        if( !key ) key = event.key.keysym.sym;
        break;

      case SDL_MOUSEMOTION:
        mx = (event.motion.x - ptz->x)/8;
        my = (event.motion.y - ptz->y)/12;
        break;

      case SDL_MOUSEBUTTONDOWN:
        if( event.button.button == SDL_BUTTON_LEFT )
        {
          mx = (event.button.x - ptz->x)/8;
          my = (event.button.y - ptz->y)/12;
          mclick = SDL_GetTicks();
        }
        break;
    }

    switch( event.type )
    {
      case SDL_MOUSEBUTTONDOWN:
        if( ( mx < 1 ) || ( mx > 38 ) )
          break;
       
        if( ( my == 28 ) || ( my == 30 ) )
        {
          freqf_cgad = (my-28)/2;
          filereq_showfiles( top, cfile );
          filereq_drawtbox( &freqf_tbox[0], freqf_cgad==0 );
          filereq_drawtbox( &freqf_tbox[1], freqf_cgad==1 );
          filereq_render( oric );
          break;
        }

        if( ( my < 1 ) || ( my > 26 ) )
          break;

        freqf_cgad = 2;

        i = (my-1)-top;
        if( i >= freqf_used ) i = freqf_used-1;

        if( cfile != i )
        {
          cfile = i;
          filereq_setfiletbox( cfile, fname );
          filereq_showfiles( top, cfile );
          filereq_drawtbox( &freqf_tbox[1], 0 );
          filereq_render( oric );
          freqf_clicktime = mclick;
          break;
        }

        if( ( freqf_clicktime == 0 ) ||
            ( (mclick-freqf_clicktime) >= 2000 ) )
        {
          freqf_clicktime = mclick;
          break;
        }

        freqf_clicktime = 0;

        key = SDLK_RETURN;

      case SDL_KEYUP:
        switch( key )
        {
          case SDLK_ESCAPE:
            return SDL_FALSE;

          case SDLK_RETURN:
            switch( freqf_cgad )
            {
              case 0:
                cfile = top = 0;
                if( !filereq_scan( path ) )
                {
                  if( path[0] )
                  {
                    path[0] = 0;
                    filereq_settbox( &freqf_tbox[0], path );
                    if( !filereq_scan( path ) ) return SDL_FALSE;
                  } else {
                    return SDL_FALSE;
                  }
                }
                filereq_showfiles( top, cfile );
                filereq_drawtbox( &freqf_tbox[0], freqf_cgad==0 );
                filereq_drawtbox( &freqf_tbox[1], freqf_cgad==1 );
                filereq_render( oric );
                break;
              
              case 1:
                return SDL_TRUE;
                
              case 2:
                if( freqf_used <= 0 ) break;
                if( cfile<=0 ) // Parent
                {
                  i = strlen( path )-1;
                  if( i<=0 ) break;
                  if( path[i] == '/' ) i--;
                  while( i > -1 )
                  {
                    if( path[i] == '/' ) break;
                    i--;
                  }
                  if( i==-1 ) i++;
                  path[i] = 0;
                } else if( freqfiles[cfile].isdir ) {
                  i = strlen( path )-1;
                  if( i < 0 )
                  {
                    i++;
                  } else {
                    if( path[i] != '/' )
                      i++;
                  }
                  if( i > 0 ) path[i++] = '/';
                  strncpy( &path[i], freqfiles[cfile].name, 4096-i );
                  path[4095] = 0;
                } else {
                  return SDL_TRUE;
                }
                cfile = top = 0;
                filereq_settbox( &freqf_tbox[0], path );
                if( !filereq_scan( path ) )
                {
                  if( path[0] )
                  {
                    path[0] = 0;
                    filereq_settbox( &freqf_tbox[0], path );
                    if( !filereq_scan( path ) ) return SDL_FALSE;
                  } else {
                    return SDL_FALSE;
                  }
                }
                filereq_showfiles( top, cfile );
                filereq_drawtbox( &freqf_tbox[0], freqf_cgad==0 );
                filereq_drawtbox( &freqf_tbox[1], freqf_cgad==1 );
                filereq_render( oric );
                break;
            }
            break;
        }
        break;

      case SDL_KEYDOWN:
        switch( key )
        {          
          case SDLK_TAB:
            freqf_cgad = (freqf_cgad+1)%3;
            filereq_showfiles( top, cfile );
            filereq_drawtbox( &freqf_tbox[0], freqf_cgad==0 );
            filereq_drawtbox( &freqf_tbox[1], freqf_cgad==1 );
            filereq_render( oric );
            break;
          
          case SDLK_UP:
            if( ( freqf_cgad != 2 ) || ( cfile <= 0 ) ) break;
            cfile--;
            if( cfile < top ) top = cfile;
            filereq_setfiletbox( cfile, fname );
            filereq_showfiles( top, cfile );
            filereq_drawtbox( &freqf_tbox[1], SDL_FALSE );
            filereq_render( oric );
            break;
          
          case SDLK_DOWN:
            if( ( freqf_cgad != 2 ) || ( cfile >= (freqf_used-1) ) ) break;
            cfile++;
            if( cfile > (top+25) ) top = cfile-25;
            filereq_setfiletbox( cfile, fname );
            filereq_showfiles( top, cfile );
            filereq_drawtbox( &freqf_tbox[1], SDL_FALSE );
            filereq_render( oric );
            break;
          
          case SDLK_BACKSPACE:
            if( freqf_cgad == 2 ) break;
            tb = &freqf_tbox[freqf_cgad];
            if( tb->cpos < 1 ) break;
            for( i=tb->cpos-1; i<tb->slen; i++ )
              tb->buf[i] = tb->buf[i+1];
            tb->slen--;
            if( tb->slen < 0 )
            {
              tb->slen = 0;
              tb->cpos = 0;
            }

          case SDLK_LEFT:
            if( freqf_cgad == 2 ) break;
            tb = &freqf_tbox[freqf_cgad]; 
            if( tb->cpos > 0 ) tb->cpos--;
            if( tb->cpos < tb->vpos ) tb->vpos = tb->cpos;
            if( tb->cpos >= (tb->vpos+tb->w) ) tb->vpos = tb->cpos-(tb->w-1);
            if( tb->vpos < 0 ) tb->vpos = 0;
            filereq_drawtbox( tb, SDL_TRUE );
            filereq_render( oric );
            break;

          case SDLK_RIGHT:
            if( freqf_cgad == 2 ) break;
            tb = &freqf_tbox[freqf_cgad]; 
            if( tb->cpos < tb->slen ) tb->cpos++;
            if( tb->cpos < tb->vpos ) tb->vpos = tb->cpos;
            if( tb->cpos >= (tb->vpos+tb->w) ) tb->vpos = tb->cpos-(tb->w-1);
            if( tb->vpos < 0 ) tb->vpos = 0;
            filereq_drawtbox( tb, SDL_TRUE );
            filereq_render( oric );
            break;          

          case SDLK_DELETE:
            if( freqf_cgad == 2 ) break;
            tb = &freqf_tbox[freqf_cgad];
            if( tb->cpos >= tb->slen ) break;
            for( i=tb->cpos; i<tb->slen; i++ )
              tb->buf[i] = tb->buf[i+1];
            tb->slen--;
            if( tb->slen < 0 )
            {
              tb->slen = 0;
              tb->cpos = 0;
            }
            filereq_drawtbox( tb, SDL_TRUE );
            filereq_render( oric );
            break;
          
          default:
            if( freqf_cgad == 2 ) break;

            if( ( key > 31 ) && ( key < 127 ) )
            {
              tb = &freqf_tbox[freqf_cgad];
              if( tb->slen >= (tb->maxlen-1) ) break;
              for( i=tb->slen; i>=tb->cpos; i-- )
                tb->buf[i] = tb->buf[i-1];
              tb->buf[tb->cpos] = key;
              tb->slen++;
              tb->cpos++;
              if( tb->cpos < tb->vpos ) tb->vpos = tb->cpos;
              if( tb->cpos >= (tb->vpos+tb->w) ) tb->vpos = tb->cpos-(tb->w-1);
              if( tb->vpos < 0 ) tb->vpos = 0;
              filereq_drawtbox( tb, SDL_TRUE );
              filereq_render( oric );
            }
            break;
        }
        break;
      
      case SDL_QUIT:
        setemumode( oric, NULL, EM_PLEASEQUIT );
        return SDL_FALSE;
    }
  }
}

/***************** These functions can be used directly from the menu system ***************/

// "insert" a tape into the virtual tape drive, via filerequester
void inserttape( struct machine *oric, struct osdmenuitem *mitem, int dummy )
{
  char *odir;

  if( !filerequester( oric, "Insert tape", tapepath, tapefile ) ) return;

  odir = getcwd( NULL, 0 );
  chdir( tapepath );
  tape_load_tap( oric, tapefile );
  if( oric->symbolsautoload ) mon_new_symbols( "symbols" );
  chdir( odir );
  free( odir );
  setemumode( oric, NULL, EM_RUNNING );
}

// "insert" a disk into the virtual disk drive, via filerequester
void insertdisk( struct machine *oric, struct osdmenuitem *mitem, int drive )
{
  char *odir;

  if( !filerequester( oric, "Insert disk", diskpath, diskfile ) ) return;

  odir = getcwd( NULL, 0 );
  chdir( diskpath );
  diskimage_load( oric, diskfile, drive );
  chdir( odir );
  free( odir );

  if( oric->drivetype == DRV_NONE )
  {
    swapmach( oric, NULL, (DRV_MICRODISC<<16)|oric->type );
//    setemumode( oric, NULL, EM_DEBUG );
    return;
  }
  setemumode( oric, NULL, EM_RUNNING );
}

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

    case SDL_KEYUP:
      switch( ev->key.keysym.sym )
      {
        case SDLK_UP:
          i = cmenu->citem-1;
          while( ( cmenu->items[i].name == OSDMENUBAR ) ||
                 ( cmenu->items[i].func == NULL ) )
          {
            if( i < 0 ) break;
            i--;
          }
          if( ( i < 0 ) || ( cmenu->items[i].name == OSDMENUBAR ) )
            break;
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
            break;

          cmenu->citem = i;
          drawitems();
          *needrender = SDL_TRUE;
          break;
        
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
  strcpy( tapepath, "tapes" );
  strcpy( tapefile, "" );
  strcpy( diskpath, "disks" );
  strcpy( diskfile, "" );
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

  // Go SDL!
  if( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO ) < 0 )
  {
    printf( "SDL init failed\n" );
    return SDL_FALSE;
  }
  need_sdl_quit = SDL_TRUE;

//  screen = SDL_SetVideoMode( 640, 480, 16, SDL_DOUBLEBUF | SDL_FULLSCREEN );
  screen = SDL_SetVideoMode( 640, 480, 16, SDL_DOUBLEBUF );
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
  wanted.userdata = 0;

  soundavailable = SDL_FALSE;
  soundon = SDL_FALSE;
  if( SDL_OpenAudio( &wanted, NULL ) >= 0 )
  {
    soundon = SDL_TRUE;
    soundavailable = SDL_TRUE;
  }

  SDL_WM_SetCaption( "Oriculator WIP", "Oriculator WIP" );

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
  tz[TZ_FILEREQ] = alloc_textzone( 160, 48, 40, 32, "Files" );
  if( !tz[TZ_FILEREQ] ) { printf( "Out of memory\n" ); return SDL_FALSE; }

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
