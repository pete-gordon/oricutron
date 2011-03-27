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

#ifdef __MORPHOS__
#include <proto/openurl.h>
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
#include "monitor.h"
#include "6551.h"
#include "machine.h"
#include "filereq.h"
#include "render_sw.h"
#include "render_gl.h"
#include "render_null.h"

extern struct symboltable usersyms;
extern SDL_bool fullscreen;

char tapepath[4096], tapefile[512];
char diskpath[4096], diskfile[512];
char telediskpath[4096], telediskfile[512];
extern char atmosromfile[];
extern char oric1romfile[];
extern char mdiscromfile[];
extern char jasmnromfile[];
extern char pravzromfile[];
extern char telebankfiles[8][1024];
//char snappath[4096], snapfile[512];
char filetmp[4096+512];

SDL_bool refreshstatus = SDL_TRUE, refreshdisks = SDL_TRUE, refreshavi = SDL_TRUE, refreshtape = SDL_TRUE;
extern struct avi_handle *vidcap;


#define GIMG_W_DISK 18
#define GIMG_W_TAPE 20
#define GIMG_W_AVIR 16

// Images used in the GUI
struct guiimg gimgs[NUM_GIMG]  = { { IMAGEPREFIX"statusbar.bmp",              640, 16, NULL },
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

SDL_bool soundavailable, soundon;
extern SDL_bool microdiscrom_valid, jasminrom_valid;

// GUI image locations
#define GIMG_POS_SBARY   (480-16)
#define GIMG_POS_TAPEX   (640-4-GIMG_W_TAPE)
#define GIMG_POS_DISKX   (GIMG_POS_TAPEX-(6+(GIMG_W_DISK*4)))
#define GIMG_POS_AVIRECX (GIMG_POS_DISKX-(6+GIMG_W_AVIR))

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

// All the textzones. Spaces in this array
// are reserved in gui.h
struct textzone *tz[NUM_TZ];

// Temporary string buffer
char vsptmp[VSPTMPSIZE];

// FPS calculation vars
extern Uint32 frametimeave;
SDL_bool warpspeed=SDL_FALSE;

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
void swap_render_mode( struct machine *oric, struct osdmenuitem *mitem, int newrendermode );
void togglehstretch( struct machine *oric, struct osdmenuitem *mitem, int dummy );
void togglescanlines( struct machine *oric, struct osdmenuitem *mitem, int dummy );
void togglefullscreen( struct machine *oric, struct osdmenuitem *mitem, int dummy );
void togglelightpen( struct machine *oric, struct osdmenuitem *mitem, int dummy );
//void savesnap( struct machine *oric, struct osdmenuitem *mitem, int dummy );

// Menu definitions. Name, key name, SDL key code, function, parameter
// Keys that are also available while emulating should be marked with
// square brackets
struct osdmenuitem mainitems[] = { { "Insert tape...",         "T",    't',      inserttape,      0, 0 },
                                   { "Insert disk 0...",       "0",    SDLK_0,   insertdisk,      0, 0 },
                                   { "Insert disk 1...",       "1",    SDLK_1,   insertdisk,      1, 0 },
                                   { "Insert disk 2...",       "2",    SDLK_2,   insertdisk,      2, 0 },
                                   { "Insert disk 3...",       "3",    SDLK_3,   insertdisk,      3, 0 },
                                   { OSDMENUBAR,               NULL,   0,        NULL,            0, 0 },
//                                   { "Save snapshot...",       NULL,   0,        savesnap,        0, 0 },
//                                   { "Load snapshot...",       NULL,   0,        NULL,            0, 0 },
//                                   { OSDMENUBAR,               NULL,   0,        NULL,            0, 0 },
                                   { "Hardware options...",    "H",    'h',      gotomenu,        1, 0 },
                                   { "Audio options...",       "A",    'a',      gotomenu,        2, 0 },
#ifdef __OPENGL_AVAILABLE__
                                   { "Video options...",       "V",    'v',      gotomenu,        4, 0 },
#endif
                                   { "Debug options...",       "D",    'd',      gotomenu,        3, 0 },
                                   { OSDMENUBAR,               NULL,   0,        NULL,            0, 0 },
                                   { "Reset",                  "[F4]", SDLK_F4,  resetoric,       0, 0 },
                                   { "Monitor",                "[F2]", SDLK_F2,  setemumode,      EM_DEBUG, 0 },
                                   { "Back",                   "\x17", SDLK_BACKSPACE,setemumode, EM_RUNNING, 0 },
                                   { OSDMENUBAR,               NULL,   0,        NULL,            0, 0 },
                                   { "About",                  NULL,   0,        gotomenu,        5, 0 },
                                   { OSDMENUBAR,               NULL,   0,        NULL,            0, 0 },
                                   { "Quit",                   NULL,   0,        setemumode,      EM_PLEASEQUIT, 0 },
                                   { NULL, } };

struct osdmenuitem hwopitems[] = { { " Oric-1",                "1",    SDLK_1,   swapmach,        (0xffff<<16)|MACH_ORIC1, 0 },
                                   { " Oric-1 16K",            "2",    SDLK_2,   swapmach,        (0xffff<<16)|MACH_ORIC1_16K, 0 },
                                   { " Atmos",                 "3",    SDLK_3,   swapmach,        (0xffff<<16)|MACH_ATMOS, 0 },
                                   { " Telestrat",             "4",    SDLK_4,   swapmach,        (DRV_NONE<<16)|MACH_TELESTRAT, 0 },
                                   { " Pravetz 8D",            "5",    SDLK_5,   swapmach,        (DRV_NONE<<16)|MACH_PRAVETZ, 0 },
                                   { OSDMENUBAR,               NULL,   0,        NULL,            0, 0 },
                                   { " No disk",               "X",    'x',      setdrivetype,    DRV_NONE, 0 },
                                   { " Microdisc",             "M",    'm',      setdrivetype,    DRV_MICRODISC, 0 },
                                   { " Jasmin",                "J",    'j',      setdrivetype,    DRV_JASMIN, 0 },
                                   { " Pravetz",               "P",    'p',      NULL,            0, 0 },
                                   { OSDMENUBAR,               NULL,   0,        NULL,            0, 0 },
                                   { " Turbo tape",            NULL,   0,        toggletapeturbo, 0, 0 },
                                   { " Autoinsert tape",       NULL,   0,        toggleautoinsrt, 0, 0 },
                                   { " Autorewind tape",       NULL,   0,        toggleautowind,  0, 0 },
                                   { OSDMENUBAR,               NULL,   0,        NULL,            0, 0 },
                                   { " VSync hack",            NULL,   0,        togglevsynchack, 0, 0 },
                                   { " Lightpen",              NULL,   0,        togglelightpen,  0, 0 },
                                   { " Mouse",                 NULL,   0,        NULL,            0, 0 },
                                   { OSDMENUBAR,               NULL,   0,        NULL,            0, 0 },
                                   { "Back",                   "\x17", SDLK_BACKSPACE,gotomenu,   0, 0 },
                                   { NULL, } };

struct osdmenuitem auopitems[] = { { " Sound enabled",         NULL,   0,        togglesound,     0, 0 },
                                   { " Tape noise",            NULL,   0,        toggletapenoise, 0, 0 },
                                   { OSDMENUBAR,               NULL,   0,        NULL,            0, 0 },
                                   { "Back",                   "\x17", SDLK_BACKSPACE,gotomenu,   0, 0 },
                                   { NULL, } };
                                  
struct osdmenuitem dbopitems[] = { { " Autoload symbols file", NULL,   0,        togglesymbolsauto, 0, 0 },
                                   { " Case-sensitive symbols",NULL,   0,        togglecasesyms,  0, 0 },
                                   { OSDMENUBAR,               NULL,   0,        NULL,            0, 0 },
                                   { "Back",                   "\x17", SDLK_BACKSPACE,gotomenu,   0, 0 },
                                   { NULL, } };

struct osdmenuitem vdopitems[] = { { " OpenGL rendering",      "O",    'o',      swap_render_mode, RENDERMODE_GL, 0 },
                                   { " Software rendering",    "S",    's',      swap_render_mode, RENDERMODE_SW, 0 },
                                   { OSDMENUBAR,               NULL,   0,        NULL,            0, 0 },
                                   { " Fullscreen",            "F",    'f',      togglefullscreen, 0, 0 },
                                   { " Scanlines",             "C",    'c',      togglescanlines, 0, 0 },
                                   { OSDMENUBAR,               NULL,   0,        NULL,            0, 0 },
                                   { "Back",                   "\x17", SDLK_BACKSPACE,gotomenu,   0, 0 },
                                   { NULL, } };

struct osdmenuitem glopitems[] = { { " OpenGL rendering",      "O",    'o',      swap_render_mode, RENDERMODE_GL, 0 },
                                   { " Software rendering",    "S",    's',      swap_render_mode, RENDERMODE_SW, 0 },
                                   { OSDMENUBAR,               NULL,   0,        NULL,            0, 0 },
                                   { " Fullscreen",            "F",    'f',      togglefullscreen, 0, 0 },
                                   { " Horizontal stretch",    "H",    'h',      togglehstretch,  0, 0 },
                                   { " Scanlines",             "C",    'c',      togglescanlines, 0, 0 },
                                   { OSDMENUBAR,               NULL,   0,        NULL,            0, 0 },
                                   { "Back",                   "\x17", SDLK_BACKSPACE,gotomenu,   0, 0 },
                                   { NULL, } };

struct osdmenuitem aboutitems[] = { { "",                                  NULL,   0, NULL, 0, 0 },
                                    { APP_NAME_FULL,                       NULL,   0, NULL, 0, OMIF_BRIGHT|OMIF_CENTRED },
                                    { "http://code.google.com/p/oriculator/",NULL,  0, gotosite, 0, OMIF_CENTRED },
                                    { "",                                  NULL,   0, NULL, 0, 0 },
                                    { "(C)2010 Peter Gordon",              NULL,   0, NULL, 0, OMIF_BRIGHT|OMIF_CENTRED },
                                    { "http://www.petergordon.org.uk",     NULL,   0, gotosite, 0, OMIF_CENTRED },
                                    { "",                                  NULL,   0, NULL, 0, 0 },
                                    { "Additional programming",            NULL,   0, NULL, 0, OMIF_BRIGHT|OMIF_CENTRED },
                                    { "Francois Revol",                    NULL,   0, NULL, 0, OMIF_CENTRED },
                                    { "Stefan Haubenthal",                 NULL,   0, NULL, 0, OMIF_CENTRED },
                                    { "Alexandre Devert",                  NULL,   0, NULL, 0, OMIF_CENTRED },
                                    { "Ibisum",                            NULL,   0, NULL, 0, OMIF_CENTRED },
                                    { "Kamel Biskri",                      NULL,   0, NULL, 0, OMIF_CENTRED },
                                    { "",                                  NULL,   0, NULL, 0, 0 },
                                    { OSDMENUBAR,                          NULL,   0, NULL, 0, 0 },
                                    { "Back", "\x17", SDLK_BACKSPACE, gotomenu, 0, 0 },
                                    { NULL, } };

struct osdmenu menus[] = { { "Main Menu",         0, mainitems },
                           { "Hardware options", 13, hwopitems },
                           { "Audio options",     3, auopitems },
                           { "Debug options",     3, dbopitems },
                           { "Video options",     3, vdopitems },
                           { "About Oricutron",  15, aboutitems } };

// Load a 24bit BMP for the GUI
SDL_bool gimg_load( struct guiimg *gi )
{
  SDL_RWops *f;
  Uint8 hdrbuf[640*3];
  Sint32 x, y;
  SDL_bool fileok;

  // Get the file
  f = SDL_RWFromFile( gi->filename, "rb" );
  if( !f )
  {
    printf( "Unable to open '%s'\n", gi->filename );
    return SDL_FALSE;
  }

  // Read the header
  if( SDL_RWread( f, hdrbuf, 54, 1 ) != 1 )
  {
    printf( "Error reading '%s'\n", gi->filename );
    SDL_RWclose( f );
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
    SDL_RWclose( f );
    return SDL_FALSE;
  }

  // Get some RAM!
  if( !gi->buf )
    gi->buf = malloc( gi->w * gi->h * 3 );

  if( !gi->buf )
  {
    printf( "Out of memory\n" );
    SDL_RWclose( f );
    return SDL_FALSE;
  }

  // BMPs are upside down!
  for( y=gi->h-1; y>=0; y-- )
  {
    SDL_RWread( f, hdrbuf, ((gi->w*3)+3)&0xfffffffc, 1 );
    for( x=0; x<gi->w; x++ )
    {
      gi->buf[(y*gi->w+x)*3+0] = hdrbuf[x*3+2];
      gi->buf[(y*gi->w+x)*3+1] = hdrbuf[x*3+1];
      gi->buf[(y*gi->w+x)*3+2] = hdrbuf[x*3];
    }
  }

  SDL_RWclose( f );

  return SDL_TRUE;
}

// Draw the statusbar at the bottom
void draw_statusbar( struct machine *oric )
{
  oric->render_gimg( GIMG_STATUSBAR, 0, GIMG_POS_SBARY );
}

// Overlay the disk icons onto the status bar
void draw_disks( struct machine *oric )
{
  Sint32 i, j;

  if( oric->drivetype == DRV_NONE )
  {
    oric->render_gimgpart( GIMG_STATUSBAR, GIMG_POS_DISKX, GIMG_POS_SBARY, GIMG_POS_DISKX, 0, 18*4, 16 );
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
    oric->render_gimg( j, GIMG_POS_DISKX+i*GIMG_W_DISK, GIMG_POS_SBARY );
  }
}

// Overlay the AVI record icon onto the status bar
void draw_avirec( struct machine *oric, SDL_bool recording )
{
  if( recording )
  {
    oric->render_gimg( GIMG_AVI_RECORD, GIMG_POS_AVIRECX, GIMG_POS_SBARY );
    return;
  }

  oric->render_gimgpart( GIMG_STATUSBAR, GIMG_POS_AVIRECX, GIMG_POS_SBARY, GIMG_POS_AVIRECX, 0, 16, 16 );
}

// Overlay the tape icon onto the status bar
void draw_tape( struct machine *oric )
{
  if( !oric->tapebuf )
  {
    oric->render_gimg( GIMG_TAPE_EJECTED, GIMG_POS_TAPEX, GIMG_POS_SBARY );
    return;
  }

  if( oric->tapemotor )
  {
    oric->render_gimg( GIMG_TAPE_PLAY, GIMG_POS_TAPEX, GIMG_POS_SBARY );
    return;
  }

  if( oric->tapeoffs >= oric->tapelen )
  {
    oric->render_gimg( GIMG_TAPE_STOP, GIMG_POS_TAPEX, GIMG_POS_SBARY );
    return;
  }

  oric->render_gimg( GIMG_TAPE_PAUSE, GIMG_POS_TAPEX, GIMG_POS_SBARY );
}

// Pop up some info!
void do_popup( struct machine *oric, char *str )
{
  strncpy( oric->popupstr, str, 40 ); oric->popupstr[39] = 0;
  oric->newpopupstr = SDL_TRUE;
  oric->popuptime = 100;
}

void render_status( struct machine *oric )
{
  if( refreshstatus )
    draw_statusbar( oric );

  if( refreshdisks || refreshstatus )
  {
    draw_disks( oric );
    refreshdisks = SDL_FALSE;
  }

  if( refreshavi || refreshstatus )
  {
    draw_avirec( oric, vidcap != NULL );
    refreshavi = SDL_FALSE;
  }

  if( refreshtape || refreshstatus )
  {
    draw_tape( oric );
    refreshtape = SDL_FALSE;
  }

  refreshstatus = SDL_FALSE;
}

// Top-level rendering routine
void render( struct machine *oric )
{
  int perc, fps; //, i;

  if( oric->emu_mode == EM_DEBUG )
    mon_update( oric );

  oric->render_begin( oric );

  switch( oric->emu_mode )
  {
    case EM_MENU:
      oric->render_video( oric, SDL_TRUE );
      render_status( oric );
      if( tz[TZ_MENU] ) oric->render_textzone( oric, TZ_MENU );
      break;

    case EM_RUNNING:
      oric->render_video( oric, SDL_TRUE );
      render_status( oric );
      if( oric->showfps )
      {
        fps = 100000/(frametimeave?frametimeave:1);
        if( oric->vid_freq )
          perc = 200000/(frametimeave?frametimeave:1);
        else
          perc = 166667/(frametimeave?frametimeave:1);
        sprintf( oric->statusstr, "%4d.%02d%% - %4dFPS", perc/100, perc%100, fps/100 );
        oric->newstatusstr = SDL_TRUE;
      }
      if( oric->popuptime > 0 )
      {
        oric->popuptime--;
        if( oric->popuptime == 0 )
        {
          oric->popupstr[0] = 0;
          oric->newpopupstr = SDL_TRUE;
        }
      }
      break;

    case EM_DEBUG:
      mon_render( oric );
      break;
  }

  oric->render_end( oric );
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

  ptz->modified = SDL_TRUE;
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

  ptz->modified = SDL_TRUE;
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

SDL_bool in_textzone( struct textzone *tz, int x, int y )
{
  if( ( x >= tz->x ) && ( x < (tz->x+tz->w*8) ) &&
      ( y >= tz->y ) && ( y < (tz->y+tz->h*12) ) )
  {
    return SDL_TRUE;
  }
  
  return SDL_FALSE;
}

// Allocate a textzone structure
SDL_bool alloc_textzone( struct machine *oric, int i, int x, int y, int w, int h, char *title )
{
  struct textzone *ntz;

  ntz = malloc( sizeof( struct textzone ) + w*h*3 );
  if( !ntz ) return SDL_FALSE;

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

  ntz->modified = SDL_TRUE;

  tz[i] = ntz;
  oric->render_textzone_alloc( oric, i );

  return SDL_TRUE;
}

// Free a textzone structure
void free_textzone( struct machine *oric, int i )
{
  oric->render_textzone_free( oric, i );
  if( !tz[i] ) return;
  free( tz[i] );
  tz[i] = NULL;
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
      tzsetcol( tz[TZ_MENU], (cmenu->items[i].flags&OMIF_BRIGHT) ? 1 : 2, 3 );

    // Calculate the position in the textzone
    o = tz[TZ_MENU]->w * (i+1) + 1;

    // Fill the background colour all the way along
    for( j=1; j<tz[TZ_MENU]->w-1; j++, o++ )
      tz[TZ_MENU]->bc[o] = tz[TZ_MENU]->cbc;
    
    // Write the text for the item
    if( cmenu->items[i].flags & OMIF_CENTRED )
      tzstrpos( tz[TZ_MENU], (tz[TZ_MENU]->w-strlen(cmenu->items[i].name))/2, i+1, cmenu->items[i].name );
    else
      tzstrpos( tz[TZ_MENU], 1, i+1, cmenu->items[i].name );

    // And the key (if there is one)
    if( cmenu->items[i].key )
      tzstrpos( tz[TZ_MENU], tz[TZ_MENU]->w-(1+strlen(cmenu->items[i].key)), i+1, cmenu->items[i].key );
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
  oric->lasttapefile[0] = 0;
  joinpath( tapepath, tapefile );
  tape_load_tap( oric, filetmp );
  if( oric->symbolsautoload ) mon_new_symbols( &usersyms, oric, "symbols", SYM_BESTGUESS, SDL_TRUE, SDL_TRUE );
  setemumode( oric, NULL, EM_RUNNING );
}

// "insert" a disk into the virtual disk drive, via filerequester
void insertdisk( struct machine *oric, struct osdmenuitem *mitem, int drive )
{
  char *dpath, *dfile;

  if( oric->type != MACH_TELESTRAT )
  {
    dpath = diskpath;
    dfile = diskfile;
  } else {
    dpath = telediskpath;
    dfile = telediskfile;
  }

  if( !filerequester( oric, "Insert disk", dpath, dfile, FR_DISKLOAD ) ) return;
  joinpath( dpath, dfile );
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
  via_init( &oric->via, oric, VIA_MAIN );
  via_init( &oric->tele_via, oric, VIA_TELESTRAT );
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
  oric->tapeturbo_forceoff = SDL_FALSE;

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

// Toggle lightpen
void togglelightpen( struct machine *oric, struct osdmenuitem *mitem, int dummy )
{
  if( oric->lightpen )
  {
    oric->lightpen = SDL_FALSE;
    mitem->name = " Lightpen";
    return;
  }

  oric->lightpen = SDL_TRUE;
  mitem->name = "\x0e""Lightpen";
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
  mitem->name = "\x0e""Case-sensitive symbols";
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

// Toggle fullscreen on/off
void togglefullscreen( struct machine *oric, struct osdmenuitem *mitem, int dummy )
{
  if( !oric->render_togglefullscreen( oric ) ) return; // Failed :-(

  if( fullscreen )
  {
    vdopitems[3].name = "\x0e""Fullscreen";
    glopitems[3].name = "\x0e""Fullscreen";
    return;
  }

  vdopitems[3].name = " Fullscreen";
  glopitems[3].name = " Fullscreen";
}

// Toggle hstretch on/off
void togglehstretch( struct machine *oric, struct osdmenuitem *mitem, int dummy )
{
  if( oric->hstretch )
  {
    oric->hstretch = SDL_FALSE;
    mitem->name = " Horizontal stretch";
    return;
  }

  oric->hstretch = SDL_TRUE;
  mitem->name = "\x0e""Horizontal stretch";
}

// Toggle scanlines on/off
void togglescanlines( struct machine *oric, struct osdmenuitem *mitem, int dummy )
{
  if( oric->scanlines )
  {
    oric->scanlines = SDL_FALSE;
    vdopitems[4].name = " Scanlines";
    glopitems[5].name = " Scanlines";
    return;
  }

  oric->scanlines = SDL_TRUE;
  vdopitems[4].name = "\x0e""Scanlines";
  glopitems[5].name = "\x0e""Scanlines";
}

// Go to internet site
void gotosite( struct machine *oric, struct osdmenuitem *mitem, int dummy )
{
/* TODO: mode those to their own gui_*.c */
#ifdef __MORPHOS__
  static const struct TagItem URLTags[1] = {{TAG_DONE, (ULONG) NULL}};

  URL_OpenA(mitem->name, (struct TagItem*) URLTags);
#endif

#ifdef __amigaos4__
  char tmp[256];
  BPTR h;
  sprintf( tmp, "URL:%s", mitem->name );
  if( ( h = IDOS->Open( tmp, MODE_OLDFILE ) ) ) IDOS->Close( h );
#endif

#ifdef WIN32
   ShellExecute(NULL, "open", mitem->name,
                NULL, NULL, SW_SHOWNORMAL);
#endif

#if defined(__APPLE__) || defined(__BEOS__) || defined(__HAIKU__)
  gui_open_url( mitem->name );
#endif
}

// Go to a different menu
void gotomenu( struct machine *oric, struct osdmenuitem *mitem, int menunum )
{
  int i, w, keyw;

  if( tz[TZ_MENU] ) free_textzone( oric, TZ_MENU );

  cmenu = &menus[menunum];
  w = strlen( cmenu->title )+8;
  keyw = 0;
  for( i=0; cmenu->items[i].name; i++ )
  {
    if( cmenu->items[i].name == OSDMENUBAR )
      continue;
    if( strlen( cmenu->items[i].name ) > w )
      w = strlen( cmenu->items[i].name );
    if( cmenu->items[i].key )
    {
      if( (strlen( cmenu->items[i].key )+1) > keyw )
        keyw = strlen( cmenu->items[i].key )+1;
    }
  }

  w+=keyw+2; i+=2;

  if( !alloc_textzone( oric, TZ_MENU, 320-w*4, 240-i*6, w, i, cmenu->title ) )
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
    case SDL_ACTIVEEVENT:
      switch( ev->active.type )
      {
        case SDL_APPINPUTFOCUS:
        case SDL_APPACTIVE:
          *needrender = SDL_TRUE;
          break;
      }
      break;

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
          for( i=0; cmenu->items[i].name; i++ )
          {
            if( ( cmenu->items[i].sdlkey ) &&
                ( cmenu->items[i].sdlkey == ev->key.keysym.sym ) &&
                ( cmenu->items[i].func ) )
              break;
          }

          if( !cmenu->items[i].name ) break;

          cmenu->items[i].func( oric, &cmenu->items[i], cmenu->items[i].arg );
          drawitems();
          *needrender = SDL_TRUE;
          break;
      }
      break;
  }
  return done;
}

void set_render_mode( struct machine *oric, int whichrendermode )
{
  oric->rendermode = whichrendermode;
  switch( whichrendermode )
  {
    case RENDERMODE_SW:
      oric->render_begin          = render_begin_sw;
      oric->render_end            = render_end_sw;
      oric->render_textzone_alloc = render_textzone_alloc_sw;
      oric->render_textzone_free  = render_textzone_free_sw;
      oric->render_textzone       = render_textzone_sw;
      oric->render_gimg           = render_gimg_sw;
      oric->render_gimgpart       = render_gimgpart_sw;
      switch(oric->sw_depth)
      {
        case 16:
          oric->render_video      = render_video_sw_16bpp;
          break;
        case 32:
          oric->render_video      = render_video_sw_32bpp;
          break;
        default:
          // If we get there, it would be safe to give up
          oric->render_video      = render_video_sw_16bpp;
          break;
      }
      oric->render_togglefullscreen = render_togglefullscreen_sw;
      oric->init_render           = init_render_sw;
      oric->shut_render           = shut_render_sw;
      vdopitems[0].name = " OpenGL rendering";
      vdopitems[1].name = "\x0e""Software rendering";
      glopitems[0].name = " OpenGL rendering";
      glopitems[1].name = "\x0e""Software rendering";
      menus[4].items = vdopitems;
      break;

#ifdef __OPENGL_AVAILABLE__
    case RENDERMODE_GL:
      oric->render_begin          = render_begin_gl;
      oric->render_end            = render_end_gl;
      oric->render_textzone_alloc = render_textzone_alloc_gl;
      oric->render_textzone_free  = render_textzone_free_gl;
      oric->render_textzone       = render_textzone_gl;
      oric->render_gimg           = render_gimg_gl;
      oric->render_gimgpart       = render_gimgpart_gl;
      oric->render_video          = render_video_gl;
      oric->render_togglefullscreen = render_togglefullscreen_gl;
      oric->init_render           = init_render_gl;
      oric->shut_render           = shut_render_gl;
      vdopitems[0].name = "\x0e""OpenGL rendering";
      vdopitems[1].name = " Software rendering";
      glopitems[0].name = "\x0e""OpenGL rendering";
      glopitems[1].name = " Software rendering";
      menus[4].items = glopitems;
      break;
#endif

    default:
      oric->render_begin          = render_begin_null;
      oric->render_end            = render_end_null;
      oric->render_textzone_alloc = render_textzone_alloc_null;
      oric->render_textzone_free  = render_textzone_free_null;
      oric->render_textzone       = render_textzone_null;
      oric->render_gimg           = render_gimg_null;
      oric->render_gimgpart       = render_gimgpart_null;
      oric->render_video          = render_video_null;
      oric->render_togglefullscreen = render_togglefullscreen_null;
      oric->init_render           = init_render_null;
      oric->shut_render           = shut_render_null;
      vdopitems[0].name = " OpenGL rendering";
      vdopitems[1].name = " Software rendering";
      glopitems[0].name = " OpenGL rendering";
      glopitems[1].name = " Software rendering";
      menus[4].items = vdopitems;
      break;
  }
}

void swap_render_mode( struct machine *oric, struct osdmenuitem *mitem, int newrendermode )
{
  if( oric->rendermode == newrendermode ) return;
  oric->shut_render( oric );
  set_render_mode( oric, newrendermode );
  if( oric->init_render( oric ) )
  {
    oric->emu_mode = EM_RUNNING;
    return;
  }
  set_render_mode( oric, RENDERMODE_NULL );
  oric->emu_mode = EM_PLEASEQUIT; 
}


// Set things to default that can't fail
void preinit_gui( struct machine *oric )
{
  int i;
  for( i=0; i<NUM_TZ; i++ ) tz[i] = NULL;
  for( i=0; i<NUM_GIMG; i++ ) gimgs[i].buf = NULL;
  strcpy( tapepath, FILEPREFIX"tapes" );
  strcpy( tapefile, "" );
  strcpy( diskpath, FILEPREFIX"disks" );
  strcpy( diskfile, "" );
  strcpy( telediskpath, FILEPREFIX"teledisks" );
  strcpy( telediskfile, "" );
  strcpy( atmosromfile, ROMPREFIX"basic11b" );
  strcpy( oric1romfile, ROMPREFIX"basic10" );
  strcpy( mdiscromfile, ROMPREFIX"microdis" );
  strcpy( jasmnromfile, ROMPREFIX"jasmin" );
  strcpy( pravzromfile, ROMPREFIX"pravetzt" );
  telebankfiles[0][0] = 0;
  telebankfiles[1][0] = 0;
  telebankfiles[2][0] = 0;
  telebankfiles[3][0] = 0;
  telebankfiles[4][0] = 0;
  strcpy( telebankfiles[5], ROMPREFIX"teleass" );
  strcpy( telebankfiles[6], ROMPREFIX"hyperbas" );
  strcpy( telebankfiles[7], ROMPREFIX"telmon24" );
  set_render_mode( oric, RENDERMODE_NULL );
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
    hwopitems[11].name = "\x0e""Turbo tape";
  else
    hwopitems[11].name = " Turbo tape";

  if( oric->autoinsert )
    hwopitems[12].name = "\x0e""Autoinsert tape";
  else
    hwopitems[12].name = " Autoinsert tape";

  if( oric->autorewind )
    hwopitems[13].name = "\x0e""Autorewind tape";
  else
    hwopitems[13].name = " Autorewind tape";

  if( oric->vsynchack )
    hwopitems[15].name = "\x0e""VSync hack";
  else
    hwopitems[15].name = " VSync hack";

  if( oric->lightpen )
    hwopitems[16].name = "\x0e""Lightpen";
  else
    hwopitems[16].name = " Lightpen";

  if( oric->symbolsautoload )
    dbopitems[0].name = "\x0e""Autoload symbols file";
  else
    dbopitems[0].name = " Autoload symbols file";

  if( oric->symbolscase )
    dbopitems[1].name = "\x0e""Case-sensitive symbols";
  else
    dbopitems[1].name = " Case-sensitive symbols";

  if( fullscreen )
  {
    vdopitems[3].name = "\x0e""Fullscreen";
    glopitems[3].name = "\x0e""Fullscreen";
  } else {
    vdopitems[3].name = " Fullscreen";
    glopitems[3].name = " Fullscreen";
  }

  if( oric->hstretch )
    glopitems[4].name = "\x0e""Horizontal stretch";
  else
    glopitems[4].name = " Horizontal stretch";

  if( oric->scanlines )
  {
    vdopitems[4].name = "\x0e""Scanlines";
    glopitems[5].name = "\x0e""Scanlines";
  } else {
    vdopitems[4].name = " Scanlines";
    glopitems[5].name = " Scanlines";
  }

  hwopitems[7].func = microdiscrom_valid ? setdrivetype : NULL;
  hwopitems[8].func = jasminrom_valid ? setdrivetype : NULL;

  hwopitems[6].name = oric->drivetype==DRV_NONE      ? "\x0e""No disk"   : " No disk";
  hwopitems[7].name = oric->drivetype==DRV_MICRODISC ? "\x0e""Microdisc" : " Microdisc";
  hwopitems[8].name = oric->drivetype==DRV_JASMIN    ? "\x0e""Jasmin"    : " Jasmin";
}

// Initialise the GUI
SDL_bool init_gui( struct machine *oric, Sint32 rendermode )
{
  int i;
  SDL_AudioSpec wanted;

  for( i=0; i<NUM_GIMG; i++ )
  {
    if( !gimg_load( &gimgs[i] ) ) return SDL_FALSE;
  }

  set_render_mode( oric, rendermode );
  if( !oric->init_render( oric ) ) return SDL_FALSE;

  // Allocate all text zones
  if( !alloc_textzone( oric, TZ_MONITOR,    0, 228, 50, 21, "Monitor"              ) ) return SDL_FALSE;
  if( !alloc_textzone( oric, TZ_DEBUG,      0, 228, 50, 21, "Debug console"        ) ) return SDL_FALSE;
  if( !alloc_textzone( oric, TZ_MEMWATCH,   0, 228, 50, 21, "Memory watch"         ) ) return SDL_FALSE;
  if( !alloc_textzone( oric, TZ_REGS,     240,   0, 50, 19, "6502 Status"          ) ) return SDL_FALSE;
  if( !alloc_textzone( oric, TZ_VIA,      400, 228, 30, 21, "VIA Status"           ) ) return SDL_FALSE;
  if( !alloc_textzone( oric, TZ_VIA2,     400, 228, 30, 21, "Telestrat VIA Status" ) ) return SDL_FALSE;
  if( !alloc_textzone( oric, TZ_AY,       400, 228, 30, 21, "AY Status"            ) ) return SDL_FALSE;
  if( !alloc_textzone( oric, TZ_DISK,     400, 228, 30, 21, "Disk Status"          ) ) return SDL_FALSE;

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

  setmenutoggles( oric );
  return SDL_TRUE;
}

// Bye bye.
void shut_gui( struct machine *oric )
{
  int i;

  oric->shut_render( oric );

  for( i=0; i<NUM_TZ; i++ )
    free_textzone( oric, i );
}
