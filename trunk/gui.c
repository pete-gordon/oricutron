/*
**  Oricutron
**  Copyright (C) 2009-2014 Peter Gordon
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

#if defined(__MORPHOS__) || defined(__AROS__)
#include <proto/exec.h>
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
#include "render_sw8.h"
#include "render_gl.h"
#include "render_null.h"
#include "ula.h"
#include "tape.h"
#include "joystick.h"
#include "snapshot.h"
#include "msgbox.h"
#include "keyboard.h"

extern SDL_bool fullscreen;

char tapepath[4096], tapefile[512];
char diskpath[4096], diskfile[512];
char telediskpath[4096], telediskfile[512];
char pravdiskpath[4096], pravdiskfile[512];
extern char atmosromfile[];
extern char oric1romfile[];
extern char mdiscromfile[];
extern char jasmnromfile[];
extern char pravetzromfile[2][1024];
extern char telebankfiles[8][1024];
char snappath[4096], snapfile[512];
char mappingpath[4096], mappingfile[512];
char filetmp[4096+512];

SDL_bool refreshstatus = SDL_TRUE, refreshdisks = SDL_TRUE, refreshavi = SDL_TRUE, refreshtape = SDL_TRUE,
    refreshkeyboard = SDL_TRUE;
extern struct avi_handle *vidcap;

extern SDL_bool need_sdl_quit;
extern SDL_AudioSpec obtained;
extern Uint32 cyclespersample;


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
                                   { IMAGEPREFIX"tape_record.bmp",    GIMG_W_TAPE, 16, NULL },
                                   { IMAGEPREFIX"avirec.bmp",         GIMG_W_AVIR, 16, NULL },
                                   { IMAGEPREFIX"gfx_oric1kbd.bmp",   640, 240, NULL },
                                   { IMAGEPREFIX"gfx_atmoskbd.bmp",   640, 240, NULL },
                                   { IMAGEPREFIX"gfx_pravetzkbd.bmp", 640, 240, NULL }};

SDL_bool soundavailable, soundon;
#if defined(__linux__)
Sint16 soundsilence = 0;
#else
Sint16 soundsilence = -32768;
#endif
extern SDL_bool microdiscrom_valid, jasminrom_valid, pravetzrom_valid;

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

static char* aciabackends[ACIA_TYPE_LAST] = {
       " Serial none     ",
  "\x0e""Serial loopback ",
  "\x0e""Serial modem    ",
  "\x0e""Serial com      ",
};

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
void togglepalghost( struct machine *oric, struct osdmenuitem *mitem, int dummy );
void togglescanlines( struct machine *oric, struct osdmenuitem *mitem, int dummy );
void togglelightpen( struct machine *oric, struct osdmenuitem *mitem, int dummy );
void toggleaciabackend( struct machine *oric, struct osdmenuitem *mitem, int dummy );
void setoverclock( struct machine *oric, struct osdmenuitem *mitem, int dummy );
void savesnap( struct machine *oric, struct osdmenuitem *mitem, int dummy );
void loadsnap( struct machine *oric, struct osdmenuitem *mitem, int dummy );
void togglekeyboard( struct machine *oric, struct osdmenuitem *mitem, int dummy );
void definemapping( struct machine *oric, struct osdmenuitem *mitem, int dummy );
void togglestickykeys( struct machine *oric, struct osdmenuitem *mitem, int dummy );
void savemapping( struct machine *oric, struct osdmenuitem *mitem, int dummy );
void loadmapping( struct machine *oric, struct osdmenuitem *mitem, int dummy );
void resetmapping( struct machine *oric, struct osdmenuitem *mitem, int dummy );
#ifdef __CBCOPY__
void clipbd_copy_gui( struct machine *oric, struct osdmenuitem *mitem, int dummy );
#endif
#ifdef __CBPASTE__
void clipbd_paste_gui( struct machine *oric, struct osdmenuitem *mitem, int dummy );
#endif

// Menu definitions. Name, key name, SDL key code, function, parameter
// Keys that are also available while emulating should be marked with
// square brackets
struct osdmenuitem mainitems[] = { { "Insert tape...",         "T",    't',      inserttape,      0, 0 },
                                   { "Save tape output...",    "[F9]", SDLK_F9,  toggletapecap,   0, 0 },
                                   { "Insert disk 0...",       "0",    SDLK_0,   insertdisk,      0, 0 },
                                   { "Insert disk 1...",       "1",    SDLK_1,   insertdisk,      1, 0 },
                                   { "Insert disk 2...",       "2",    SDLK_2,   insertdisk,      2, 0 },
                                   { "Insert disk 3...",       "3",    SDLK_3,   insertdisk,      3, 0 },
                                   { OSDMENUBAR,               NULL,   0,        NULL,            0, 0 },
                                   { "Save snapshot...",       NULL,   0,        savesnap,        0, 0 },
                                   { "Load snapshot...",       NULL,   0,        loadsnap,        0, 0 },
#if defined(__CBCOPY__) || defined(__CBPASTE__)
                                   { OSDMENUBAR,               NULL,   0,        NULL,            0, 0 },
#endif
#if defined(__CBCOPY__)
                                   { "Copy to clipboard",      "[F11]",SDLK_F11, clipbd_copy_gui, 0, 0 },
#endif
#if defined(__CBPASTE__)
                                   { "Paste from clipboard",   "[F12]",SDLK_F12, clipbd_paste_gui,0, 0 },
#endif
                                   { OSDMENUBAR,               NULL,   0,        NULL,            0, 0 },
                                   { "Hardware options...",    "H",    'h',      gotomenu,        1, 0 },
                                   { "Audio options...",       "A",    'a',      gotomenu,        2, 0 },
                                   { "Video options...",       "V",    'v',      gotomenu,        4, 0 },
                                   { "Keyboard options...",    "K",    'k',      gotomenu,        7, 0 },
                                   { "Debug options...",       "D",    'd',      gotomenu,        3, 0 },
                                   { "Overclock options...",   "C",    'c',      gotomenu,        6, 0 },
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
                                   { " Pravetz 8D",            "5",    SDLK_5,   swapmach,        (0xffff<<16)|MACH_PRAVETZ, 0 },
                                   { OSDMENUBAR,               NULL,   0,        NULL,            0, 0 },
                                   { " No disk",               "X",    'x',      setdrivetype,    DRV_NONE, 0 },
                                   { " Microdisc",             "M",    'm',      setdrivetype,    DRV_MICRODISC, 0 },
                                   { " Jasmin",                "J",    'j',      setdrivetype,    DRV_JASMIN, 0 },
//                                   { " Cumana",                "C",    'c',      NULL,            0, 0 },
                                   { " Pravetz 8D disk",       "P",    'p',      setdrivetype,    DRV_PRAVETZ, 0 },
                                   { OSDMENUBAR,               NULL,   0,        NULL,            0, 0 },
                                   { " Turbo tape",            NULL,   0,        toggletapeturbo, 0, 0 },
                                   { " Autoinsert tape",       NULL,   0,        toggleautoinsrt, 0, 0 },
                                   { " Autorewind tape",       NULL,   0,        toggleautowind,  0, 0 },
                                   { OSDMENUBAR,               NULL,   0,        NULL,            0, 0 },
                                   { " VSync hack",            NULL,   0,        togglevsynchack, 0, 0 },
                                   { " Lightpen",              NULL,   0,        togglelightpen,  0, 0 },
                                   { " Serial none     ",      NULL,   0,        toggleaciabackend,   0, 0 },
//                                   { " Mouse",                 NULL,   0,        NULL,            0, 0 },
                                   { OSDMENUBAR,               NULL,   0,        NULL,            0, 0 },
                                   { "Back",                   "\x17", SDLK_BACKSPACE,gotomenu,   0, 0 },
                                   { NULL, } };

struct osdmenuitem auopitems[] = { { " Sound enabled",         NULL,   0,        togglesound,     0, 0 },
                                   { " Tape noise",            NULL,   0,        toggletapenoise, 0, 0 },
                                   { OSDMENUBAR,               NULL,   0,        NULL,            0, 0 },
                                   { "Back",                   "\x17", SDLK_BACKSPACE,gotomenu,   0, 0 },
                                   { NULL, } };

struct osdmenuitem keopitems[] = { { " Show keyboard",         NULL,   0,        togglekeyboard,  0, 0 },
                                   { " Sticky mod keys",       NULL,   0,        togglestickykeys,0, 0 },
                                   { " Define mapping",        NULL,   0,        definemapping,   0, 0 },
                                   { "Save mapping...",        NULL,   0,        savemapping,     0, 0 },
                                   { "Load mapping...",        NULL,   0,        loadmapping,     0, 0 },
                                   { "Reset mapping",          NULL,   0,        resetmapping,    0, 0 },
                                   { OSDMENUBAR,               NULL,   0,        NULL,            0, 0 },
                                   { "Back",                   "\x17", SDLK_BACKSPACE,gotomenu,   0, 0 },
                                   { NULL, } };



struct osdmenuitem dbopitems[] = { { " Autoload symbols file", NULL,   0,        togglesymbolsauto, 0, 0 },
                                   { " Case-sensitive symbols",NULL,   0,        togglecasesyms,  0, 0 },
                                   { OSDMENUBAR,               NULL,   0,        NULL,            0, 0 },
                                   { "Back",                   "\x17", SDLK_BACKSPACE,gotomenu,   0, 0 },
                                   { NULL, } };

#ifdef __OPENGL_AVAILABLE__
struct osdmenuitem vdopitems[] = { { " OpenGL rendering",      "O",    'o',      swap_render_mode, RENDERMODE_GL, 0 },
                                   { " Software rendering",    "S",    's',      swap_render_mode, RENDERMODE_SW, 0 },
                                   { OSDMENUBAR,               NULL,   0,        NULL,            0, 0 },
                                   { " Fullscreen",         "[F8]",    SDLK_F8,  togglefullscreen, 0, 0 },
                                   { " Scanlines",             "C",    'c',      togglescanlines, 0, 0 },
                                   { OSDMENUBAR,               NULL,   0,        NULL,            0, 0 },
                                   { "Back",                   "\x17", SDLK_BACKSPACE,gotomenu,   0, 0 },
                                   { NULL, } };
#else
struct osdmenuitem vdopitems[] = { { " Fullscreen",            "F",    'f',      togglefullscreen, 0, 0 },
                                   { " Scanlines",             "C",    'c',      togglescanlines, 0, 0 },
                                   { OSDMENUBAR,               NULL,   0,        NULL,            0, 0 },
                                   { "Back",                   "\x17", SDLK_BACKSPACE,gotomenu,   0, 0 },
                                   { NULL, } };
#endif

struct osdmenuitem glopitems[] = { { " OpenGL rendering",      "O",    'o',      swap_render_mode, RENDERMODE_GL, 0 },
                                   { " Software rendering",    "S",    's',      swap_render_mode, RENDERMODE_SW, 0 },
                                   { OSDMENUBAR,               NULL,   0,        NULL,            0, 0 },
                                   { " Fullscreen",            "F",    'f',      togglefullscreen, 0, 0 },
                                   { " Horizontal stretch",    "H",    'h',      togglehstretch,  0, 0 },
                                   { " Scanlines",             "C",    'c',      togglescanlines, 0, 0 },
                                   { " PAL ghosting",          "P",    'p',      togglepalghost,  0, 0 },
                                   { OSDMENUBAR,               NULL,   0,        NULL,            0, 0 },
                                   { "Back",                   "\x17", SDLK_BACKSPACE,gotomenu,   0, 0 },
                                   { NULL, } };

struct osdmenuitem aboutitems[] = { { "",                                  NULL,   0, NULL, 0, 0 },
                                    { APP_NAME_FULL,                       NULL,   0, NULL, 0, OMIF_BRIGHT|OMIF_CENTRED },
                                    { "http://code.google.com/p/oriculator/",NULL,  0, gotosite, 0, OMIF_CENTRED },
                                    { "",                                  NULL,   0, NULL, 0, 0 },
                                    { "(C)" APP_YEAR " Peter Gordon",      NULL,   0, NULL, 0, OMIF_BRIGHT|OMIF_CENTRED },
                                    { "http://www.petergordon.org.uk",     NULL,   0, gotosite, 0, OMIF_CENTRED },
                                    { "",                                  NULL,   0, NULL, 0, 0 },
                                    { "Additional programming",            NULL,   0, NULL, 0, OMIF_BRIGHT|OMIF_CENTRED },
                                    { "Francois Revol",                    NULL,   0, NULL, 0, OMIF_CENTRED },
                                    { "Stefan Haubenthal",                 NULL,   0, NULL, 0, OMIF_CENTRED },
                                    { "Alexandre Devert",                  NULL,   0, NULL, 0, OMIF_CENTRED },
                                    { "Ibisum",                            NULL,   0, NULL, 0, OMIF_CENTRED },
                                    { "Kamel Biskri",                      NULL,   0, NULL, 0, OMIF_CENTRED },
                                    { "Iss",                               NULL,   0, NULL, 0, OMIF_CENTRED },
                                    { "Patrice Torguet",                   NULL,   0, NULL, 0, OMIF_CENTRED },
                                    { "",                                  NULL,   0, NULL, 0, 0 },
                                    { OSDMENUBAR,                          NULL,   0, NULL, 0, 0 },
                                    { "Back", "\x17", SDLK_BACKSPACE, gotomenu, 0, 0 },
                                    { NULL, } };

struct osdmenuitem ovopitems[] = { { "  1mhz (None)", "1",    '1', setoverclock,  0, 0 },
                                   { "  2mhz",        "2",    '2', setoverclock,  1, 0 },
                                   { "  4mhz",        "3",    '3', setoverclock,  2, 0 },
                                   { "  8mhz",        "4",    '4', setoverclock,  3, 0 },
                                   { " 16mhz",        "5",    '5', setoverclock,  4, 0 },
                                   { " 32mhz",        "6",    '6', setoverclock,  5, 0 },
                                   { " 64mhz",        "7",    '7', setoverclock,  6, 0 },
                                   { OSDMENUBAR,     NULL,      0, NULL,          0, 0 },
                                   { "Back",         "\x17", SDLK_BACKSPACE,gotomenu,0, 0 },
                                   { NULL, } };

#define LAST_ITEM(x) ((sizeof(x)/sizeof(struct osdmenuitem))-2)

struct osdmenu menus[] = { { "Main Menu",        LAST_ITEM(mainitems)-4, mainitems },
                           { "Hardware options", LAST_ITEM(hwopitems),  hwopitems },
                           { "Audio options",    LAST_ITEM(auopitems),  auopitems },
                           { "Debug options",    LAST_ITEM(dbopitems),  dbopitems },
                           { "Video options",    LAST_ITEM(vdopitems),  vdopitems },
                           { "About Oricutron",  LAST_ITEM(aboutitems), aboutitems },
                           { "Overclock",        LAST_ITEM(ovopitems),  ovopitems },
                           { "Keyboard options", LAST_ITEM(keopitems),  keopitems }};

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
   if (oric->statusbar_mode == STATUSBARMODE_NONE)
     oric->render_clear_area( 0, GIMG_POS_SBARY, 640, 480-GIMG_POS_SBARY );
   else
     oric->render_gimg( GIMG_STATUSBAR, 0, GIMG_POS_SBARY );
}

void draw_keyboard( struct machine *oric ) {
  if (oric->show_keyboard) {
    switch( oric->type )
    {
       case MACH_PRAVETZ:
            oric->render_gimg( GIMG_PRAVETZ_KEYBOARD, 0, 480);
            break;
       case MACH_ORIC1:
       case MACH_ORIC1_16K:
            oric->render_gimg( GIMG_ORIC1_KEYBOARD, 0, 480);
            break;
       default:
           oric->render_gimg( GIMG_ATMOS_KEYBOARD, 0, 480);
           break;
    }
  }
}

// Overlay the disk icons onto the status bar
void draw_disks( struct machine *oric )
{
  Sint32 i, j;

  if( oric->statusbar_mode == STATUSBARMODE_NONE )
    return;

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
  if( oric->statusbar_mode == STATUSBARMODE_NONE)
    return;

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
  if( oric->statusbar_mode == STATUSBARMODE_NONE )
    return;

  if( oric->tapecap )
  {
    oric->render_gimg( GIMG_TAPE_RECORD, GIMG_POS_TAPEX, GIMG_POS_SBARY );
    return;
  }

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

    if(refreshkeyboard  || refreshstatus) {
        draw_keyboard( oric );
        refreshkeyboard = SDL_FALSE;
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
      if( oric->statusbar_mode == STATUSBARMODE_FULL )
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
  for( i=0; text && text[i]; i++ )
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

void tzputc( struct textzone *ptz, char c )
{
  char tmp[2];
  tmp[0] = c;
  tmp[1] = 0;
  tzstr( ptz, tmp );
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

void clear_textzone( struct machine *oric, int i )
{
  int x, y;
  struct textzone *ptz = tz[i];

  if (!ptz) return;

  for( y=1; y<(ptz->h-1); y++ )
  {
    for( x=1; x<(ptz->w-1); x++ )
    {
      ptz->tx[y*ptz->w+x] = ' ';
      ptz->fc[y*ptz->w+x] = 2;
      ptz->bc[y*ptz->w+x] = 3;
    }
  }

  ptz->modified = SDL_TRUE;
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
      tzstrpos( tz[TZ_MENU], (tz[TZ_MENU]->w-(int)strlen(cmenu->items[i].name))/2, i+1, cmenu->items[i].name );
    else
      tzstrpos( tz[TZ_MENU], 1, i+1, cmenu->items[i].name );

    // And the key (if there is one)
    if( cmenu->items[i].key )
      tzstrpos( tz[TZ_MENU], tz[TZ_MENU]->w-(1+(int)strlen(cmenu->items[i].key)), i+1, cmenu->items[i].key );
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
  i = (int)strlen( filetmp );
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
  if( !filerequester( oric, "Insert tape", tapepath, tapefile, FR_TAPELOAD ) ) return;
  oric->lasttapefile[0] = 0;
  joinpath( tapepath, tapefile );

  switch (detect_image_type(filetmp))
  {
    case IMG_SNAPSHOT:
      if (msgbox(oric, MSGBOX_YES_NO,
        "The file you selected appears to be a snapshot file.\n"
        "Would you like to load it? (All RAM and machine state will be lost)"))
      {
        load_snapshot(oric, filetmp);
        return;
      }
      break;

    case IMG_ATMOS_MICRODISC:
      if ((oric->type != MACH_ATMOS) &&
          (oric->type != MACH_ORIC1) &&
          (oric->type != MACH_PRAVETZ))
      {
        if (msgbox(oric, MSGBOX_YES_NO,
          "The file you selected appears to be a disk for an Oric Atmos with Microdisc.\n"
          "Would you like to switch to that configuration and insert it as a disk?"))
        {
          swapmach( oric, NULL, (DRV_MICRODISC<<16)|MACH_ATMOS );
          joinpath( tapepath, tapefile );
          diskimage_load( oric, filetmp, 0 );
        }
        setemumode( oric, NULL, EM_RUNNING );
        return;
      }

      if (oric->drivetype != DRV_MICRODISC)
      {
        if (msgbox(oric, MSGBOX_YES_NO,
          "The file you selected appears to be a disk for a use with the Microdisc controller.\n"
          "Would you like to switch to that configuration and insert it as a disk?"))
        {
          swapmach( oric, NULL, (DRV_MICRODISC<<16)|oric->type );
          joinpath( tapepath, tapefile );
          diskimage_load( oric, filetmp, 0 );
        }
        setemumode( oric, NULL, EM_RUNNING );
        return;
      }

      if (msgbox(oric, MSGBOX_YES_NO,
        "The file you selected appears to be a disk.\n"
        "Would you like to insert it in drive 0?"))
      {
        joinpath( tapepath, tapefile );
        diskimage_load( oric, filetmp, 0 );
      }
      setemumode( oric, NULL, EM_RUNNING );
      return;

    case IMG_ATMOS_JASMIN:
      if ((oric->type != MACH_ATMOS) &&
          (oric->type != MACH_ORIC1) &&
          (oric->type != MACH_PRAVETZ))
      {
        if (msgbox(oric, MSGBOX_YES_NO,
          "The file you selected appears to be a disk for an Oric Atmos with Jasmin.\n"
          "Would you like to switch to that configuration and insert it as a disk?"))
        {
          swapmach( oric, NULL, (DRV_JASMIN<<16)|MACH_ATMOS );
          joinpath( tapepath, tapefile );
          diskimage_load( oric, filetmp, 0 );
          oric->auto_jasmin_reset = SDL_TRUE;
        }
        setemumode( oric, NULL, EM_RUNNING );
        return;
      }

      if (oric->drivetype != DRV_JASMIN)
      {
        if (msgbox(oric, MSGBOX_YES_NO,
          "The file you selected appears to be a disk for a use with the Jasmin controller.\n"
          "Would you like to switch to that configuration and insert it as a disk?"))
        {
          swapmach( oric, NULL, (DRV_JASMIN<<16)|oric->type );
          joinpath( tapepath, tapefile );
          diskimage_load( oric, filetmp, 0 );
          oric->auto_jasmin_reset = SDL_TRUE;
        }
        setemumode( oric, NULL, EM_RUNNING );
        return;
      }

      if (msgbox(oric, MSGBOX_YES_NO,
        "The file you selected appears to be a disk.\n"
        "Would you like to insert it in drive 0?"))
      {
        joinpath( tapepath, tapefile );
        diskimage_load( oric, filetmp, 0 );
      }
      setemumode( oric, NULL, EM_RUNNING );
      return;

    case IMG_TELESTRAT_DISK:
      if (oric->type != MACH_TELESTRAT)
      {
        if (msgbox(oric, MSGBOX_YES_NO,
          "The file you selected appears to be a disk for the Telestrat.\n"
          "Would you like to switch to that configuration and insert it as a disk?"))
        {
          swapmach( oric, NULL, MACH_TELESTRAT );
          joinpath( tapepath, tapefile );
          diskimage_load( oric, filetmp, 0 );
        }
        setemumode( oric, NULL, EM_RUNNING );
        return;
      }

      if (msgbox(oric, MSGBOX_YES_NO,
        "The file you selected appears to be a disk.\n"
        "Would you like to insert it in drive 0?"))
      {
        joinpath( tapepath, tapefile );
        diskimage_load( oric, filetmp, 0 );
      }
      setemumode( oric, NULL, EM_RUNNING );
      return;

    case IMG_PRAVETZ_DISK:
      if (oric->type != MACH_PRAVETZ)
      {
        if (msgbox(oric, MSGBOX_YES_NO,
          "The file you selected appears to be a disk for the Pravetz 8D disk controller.\n"
          "Would you like to switch to that configuration and insert it as a disk?"))
        {
          swapmach( oric, NULL, (DRV_PRAVETZ<<16)|MACH_PRAVETZ );
          joinpath( tapepath, tapefile );
          diskimage_load( oric, filetmp, 0 );
          queuekeys("CALL#320\x0d");
        }
        setemumode( oric, NULL, EM_RUNNING );
        return;
      }

      if (oric->drivetype != DRV_PRAVETZ)
      {
        if (msgbox(oric, MSGBOX_YES_NO,
          "The file you selected appears to be a disk for the Pravetz disk controller.\n"
          "Would you like to switch to that configuration and insert it as a disk?"))
        {
          swapmach( oric, NULL, (DRV_PRAVETZ<<16)|MACH_PRAVETZ );
          joinpath( tapepath, tapefile );
          diskimage_load( oric, filetmp, 0 );
          queuekeys("CALL#320\x0d");
        }
        setemumode( oric, NULL, EM_RUNNING );
        return;
      }

      if (msgbox(oric, MSGBOX_YES_NO,
        "The file you selected appears to be a disk.\n"
        "Would you like to insert it in drive 0?"))
      {
        joinpath( tapepath, tapefile );
        diskimage_load( oric, filetmp, 0 );
      }
      setemumode( oric, NULL, EM_RUNNING );
      return;

    case IMG_GUESS_MICRODISC:
      if ((oric->drivetype == DRV_PRAVETZ) ||
          (oric->drivetype == DRV_NONE))
      {
        if (msgbox(oric, MSGBOX_YES_NO,
          "The file you selected appears to be a disk for a Microdisc or Jasmin system.\n"
          "Would you like to switch to Microdisc and insert it as a disk?"))
        {
          swapmach( oric, NULL, (DRV_MICRODISC<<16)|oric->type );
          joinpath( tapepath, tapefile );
          diskimage_load( oric, filetmp, 0 );
        }
        setemumode( oric, NULL, EM_RUNNING );
        return;
      }

      if (msgbox(oric, MSGBOX_YES_NO,
        "The file you selected appears to be a disk.\n"
        "Would you like to insert it in drive 0?"))
      {
        joinpath( tapepath, tapefile );
        diskimage_load( oric, filetmp, 0 );
      }
      setemumode( oric, NULL, EM_RUNNING );
      return;
  }

  tape_load_tap( oric, filetmp );
  if( oric->symbolsautoload ) mon_new_symbols( &oric->usersyms, oric, "symbols", SYM_BESTGUESS, SDL_TRUE, SDL_TRUE );
  setemumode( oric, NULL, EM_RUNNING );
}

// "insert" a disk into the virtual disk drive, via filerequester
void insertdisk( struct machine *oric, struct osdmenuitem *mitem, int drive )
{
  char *dpath, *dfile;

  if( oric->type != MACH_TELESTRAT )
  {
    switch (oric->drivetype)
    {
      case DRV_PRAVETZ:
        dpath = pravdiskpath;
        dfile = pravdiskfile;
        break;

      default:
        dpath = diskpath;
        dfile = diskfile;
        break;
    }
  } else {
    dpath = telediskpath;
    dfile = telediskfile;
  }

  if( !filerequester( oric, "Insert disk", dpath, dfile, FR_DISKLOAD ) ) return;
  joinpath( dpath, dfile );

  switch (detect_image_type(filetmp))
  {
    case IMG_SNAPSHOT:
      if (msgbox(oric, MSGBOX_YES_NO,
        "The file you selected appears to be a snapshot file.\n"
        "Would you like to load it? (All RAM and machine state will be lost)"))
      {
        load_snapshot(oric, filetmp);
        return;
      }
      break;

    case IMG_ATMOS_MICRODISC:
      if ((oric->type != MACH_ATMOS) &&
          (oric->type != MACH_ORIC1) &&
          (oric->type != MACH_PRAVETZ))
      {
        if (msgbox(oric, MSGBOX_YES_NO,
          "The file you selected appears to be for an Oric Atmos with Microdisc.\n"
          "Would you like to switch to that configuration?"))
        {
          swapmach( oric, NULL, (DRV_MICRODISC<<16)|MACH_ATMOS );
        }
        break;
      }

      if ((oric->drivetype != DRV_MICRODISC) &&
          (oric->drivetype != DRV_NONE))
      {
        if (msgbox(oric, MSGBOX_YES_NO,
          "The file you selected appears to be for a use with the Microdisc controller.\n"
          "Would you like to switch to that configuration?"))
        {
          swapmach( oric, NULL, (DRV_MICRODISC<<16)|oric->type );
        }
        break;
      }
      break;

    case IMG_ATMOS_JASMIN:
      if ((oric->type != MACH_ATMOS) &&
          (oric->type != MACH_ORIC1) &&
          (oric->type != MACH_PRAVETZ))
      {
        if (msgbox(oric, MSGBOX_YES_NO,
          "The file you selected appears to be for an Oric Atmos with Jasmin.\n"
          "Would you like to switch to that configuration?"))
        {
          swapmach( oric, NULL, (DRV_JASMIN<<16)|MACH_ATMOS );
          oric->auto_jasmin_reset = SDL_TRUE;
        }
        break;
      }

      if (oric->drivetype == DRV_NONE)
      {
        swapmach( oric, NULL, (DRV_JASMIN<<16)|oric->type);
        oric->auto_jasmin_reset = SDL_TRUE;
        break;
      }

      if (oric->drivetype != DRV_JASMIN)
      {
        if (msgbox(oric, MSGBOX_YES_NO,
          "The file you selected appears to be for a use with the Jasmin controller.\n"
          "Would you like to switch to that configuration?"))
        {
          swapmach( oric, NULL, (DRV_JASMIN<<16)|oric->type );
          oric->auto_jasmin_reset = SDL_TRUE;
        }
        break;
      }
      break;

    case IMG_TELESTRAT_DISK:
      if (oric->type != MACH_TELESTRAT)
      {
        if (msgbox(oric, MSGBOX_YES_NO,
          "The file you selected appears to be for a Telestrat.\n"
          "Would you like to switch to that configuration?"))
        {
          swapmach( oric, NULL, MACH_TELESTRAT );
        }
        break;
      }
      break;

    case IMG_PRAVETZ_DISK:
      if (oric->type != MACH_PRAVETZ)
      {
        if (msgbox(oric, MSGBOX_YES_NO,
          "The file you selected appears to be for a Pravetz 8D disk controller.\n"
          "Would you like to switch to that configuration?"))
        {
          swapmach( oric, NULL, (DRV_PRAVETZ<<16)|MACH_PRAVETZ );
          queuekeys("CALL#320\x0d");
        }
        break;
      }

      if (oric->drivetype == DRV_NONE)
      {
        swapmach( oric, NULL, (DRV_PRAVETZ<<16)|MACH_PRAVETZ );
        queuekeys("CALL#320\x0d");
        break;
      }

      if (oric->drivetype != DRV_PRAVETZ)
      {
        if (msgbox(oric, MSGBOX_YES_NO,
          "The file you selected appears to be for a use with the Pravetz disk controller.\n"
          "Would you like to switch to that configuration?"))
        {
          swapmach( oric, NULL, (DRV_PRAVETZ<<16)|MACH_PRAVETZ );
          queuekeys("CALL#320\x0d");
        }
        break;
      }
      break;

    case IMG_GUESS_MICRODISC:
      if (oric->drivetype == DRV_PRAVETZ)
      {
        if (msgbox(oric, MSGBOX_YES_NO,
          "The file you selected appears to be for a Microdisc or Jasmin system.\n"
          "Would you like to switch to Microdisc?"))
        {
          swapmach( oric, NULL, (DRV_MICRODISC<<16)|oric->type );
        }
        break;
      }
      break;

    case IMG_TAPE:
      if (msgbox(oric, MSGBOX_YES_NO,
        "This appears to be a tape image...\n"
        "Would you like to insert it as a tape?"))
      {
        oric->lasttapefile[0] = 0;
        tape_load_tap( oric, filetmp );
        if( oric->symbolsautoload ) mon_new_symbols( &oric->usersyms, oric, "symbols", SYM_BESTGUESS, SDL_TRUE, SDL_TRUE );
      }
      setemumode( oric, NULL, EM_RUNNING );
      return;
  }

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

void savesnap( struct machine *oric, struct osdmenuitem *mitem, int dummy )
{
  if( !filerequester( oric, "Save Snapshot", snappath, snapfile, FR_SNAPSHOTSAVE ) ) return;
  joinpath( snappath, snapfile );
  save_snapshot( oric, filetmp );
}

void loadsnap( struct machine *oric, struct osdmenuitem *mitem, int dummy )
{
  if( !filerequester( oric, "Load Snapshot", snappath, snapfile, FR_SNAPSHOTLOAD ) ) return;
  joinpath( snappath, snapfile );
  load_snapshot( oric, filetmp );
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
  setromon( oric );
  m6502_reset( &oric->cpu );
  via_init( &oric->via, oric, VIA_MAIN );
  via_init( &oric->tele_via, oric, VIA_TELESTRAT );
  ay_init( &oric->ay, oric );
  oric->cpu.rastercycles = oric->cyclesperraster;
  oric->frames = 0;
  if( oric->autorewind ) tape_rewind( oric );
  setemumode( oric, NULL, EM_RUNNING );
}

struct osdmenuitem *find_item_by_function(struct osdmenuitem *menu, void *function)
{
  int i;
  static struct osdmenuitem dummyitem; /* So we can always return non-NULL */

  for (i=0; menu[i].name; i++)
    if (menu[i].func == function)
      return &menu[i];

  return &dummyitem;
}

struct osdmenuitem *find_item_by_function_and_arg(struct osdmenuitem *menu, void *function, int arg)
{
  int i;
  static struct osdmenuitem dummyitem; /* So we can always return non-NULL */

  for (i=0; menu[i].name; i++)
    if ((menu[i].func == function) && (menu[i].arg == arg))
      return &menu[i];

  return &dummyitem;
}

struct osdmenuitem *find_item_by_key(struct osdmenuitem *menu, int sdlkey)
{
  int i;
  static struct osdmenuitem dummyitem; /* So we can always return non-NULL */

  for (i=0; menu[i].name; i++)
    if (menu[i].sdlkey == sdlkey)
      return &menu[i];

  return &dummyitem;
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
    oric->cpu.read = oric->read_not_lightpen;
    mitem->name = " Lightpen";
    return;
  }

  oric->lightpen = SDL_TRUE;
  oric->cpu.read = lightpen_read;
  mitem->name = "\x0e""Lightpen";
}

// Toggle aux acia (Serial card)
void toggleaciabackend( struct machine *oric, struct osdmenuitem *mitem, int dummy )
{
  if( oric->aciabackend )
    oric->aciabackend = ACIA_TYPE_NONE;
  else
    oric->aciabackend = oric->aciabackendcfg;

  mitem->name = aciabackends[oric->aciabackend];

  if( oric->type==MACH_TELESTRAT )
    acia_init( &oric->tele_acia, oric );
  else
    acia_init( &oric->aux_acia, oric );
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
    find_item_by_function(vdopitems, togglefullscreen)->name = "\x0e""Fullscreen";
    find_item_by_function(glopitems, togglefullscreen)->name = "\x0e""Fullscreen";
    return;
  }

  find_item_by_function(vdopitems, togglefullscreen)->name = " Fullscreen";
  find_item_by_function(glopitems, togglefullscreen)->name = " Fullscreen";
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

// Toggle PAL ghosting on/off
void togglepalghost( struct machine *oric, struct osdmenuitem *mitem, int dummy )
{
  if( oric->palghost )
  {
    oric->palghost = SDL_FALSE;
    mitem->name = " PAL ghosting";
    return;
  }

  oric->palghost = SDL_TRUE;
  mitem->name = "\x0e""PAL ghosting";
}

// Toggle scanlines on/off
void togglescanlines( struct machine *oric, struct osdmenuitem *mitem, int dummy )
{
  if( oric->scanlines )
  {
    oric->scanlines = SDL_FALSE;
    find_item_by_function(vdopitems, togglescanlines)->name = " Scanlines";
    find_item_by_function(glopitems, togglescanlines)->name = " Scanlines";
    return;
  }

  oric->scanlines = SDL_TRUE;
  find_item_by_function(vdopitems, togglescanlines)->name = "\x0e""Scanlines";
  find_item_by_function(glopitems, togglescanlines)->name = "\x0e""Scanlines";
}

void setoverclock( struct machine *oric, struct osdmenuitem *mitem, int value )
{
  int i;

  /* Don't want to just modify name[0], since */
  /* string constants are supposed to be constant.. */
  char *setnames[] = { "\x0e"" 1MHz (None)", "\x0e"" 2MHz", "\x0e"" 4MHz", "\x0e"" 8MHz",
                       "\x0e""16MHz", "\x0e""32MHz", "\x0e""64MHz" };
  char *unsetnames[] = { "  1MHz (None)", "  2MHz", "  4MHz", "  8MHz",
                         " 16MHz", " 32MHz", " 64MHz" };

  oric->overclockmult  = 1<<value;
  oric->overclockshift = value;

  for( i=0; i<7; i++ )
  {
    if(i == value)
      ovopitems[i].name = setnames[i];
    else
      ovopitems[i].name = unsetnames[i];
  }
}

// Go to internet site
void gotosite( struct machine *oric, struct osdmenuitem *mitem, int dummy )
{
/* TODO: mode those to their own gui_*.c */
#if defined(__MORPHOS__) || defined(__AROS__)
  struct Library *OpenURLBase = OpenLibrary( "openurl.library", 0 );
  static const struct TagItem URLTags[] = {{TAG_DONE, (ULONG) NULL}};

  if( OpenURLBase )
  {
    URL_OpenA(mitem->name, (struct TagItem*) URLTags);
    CloseLibrary( OpenURLBase );
  }

#elif defined(__amigaos4__)
  char tmp[256];
  BPTR h;
  sprintf( tmp, "URL:%s", mitem->name );
  if( ( h = IDOS->Open( tmp, MODE_OLDFILE ) ) ) IDOS->Close( h );

#elif defined(WIN32)
   ShellExecute(NULL, "open", mitem->name,
                NULL, NULL, SW_SHOWNORMAL);

#elif defined(__APPLE__) || defined(__BEOS__) || defined(__HAIKU__)
  gui_open_url( mitem->name );

#else
/* default: assume XDG-compliant desktop */
  char tmp[256];
  sprintf( tmp, "xdg-open '%s'", mitem->name );
  system( tmp );

#endif
}

// Go to a different menu
void gotomenu( struct machine *oric, struct osdmenuitem *mitem, int menunum )
{
  int i, w, keyw;

  if( tz[TZ_MENU] ) free_textzone( oric, TZ_MENU );

  cmenu = &menus[menunum];
  w = (int)strlen( cmenu->title )+8;
  keyw = 0;
  for( i=0; cmenu->items[i].name; i++ )
  {
    if( cmenu->items[i].name == OSDMENUBAR )
      continue;
    if( strlen( cmenu->items[i].name ) > w )
      w = (int)strlen( cmenu->items[i].name );
    if( cmenu->items[i].key )
    {
      if( (strlen( cmenu->items[i].key )+1) > keyw )
        keyw = (int)strlen( cmenu->items[i].key )+1;
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

void togglekeyboard( struct machine *oric, struct osdmenuitem *mitem, int dummy )
{
    if( oric->show_keyboard )
    {
        oric->show_keyboard = SDL_FALSE;
        mitem->name = " Show keyboard";
        oric->shut_render(oric);
        oric->init_render(oric);
        return;
    }

    oric->show_keyboard = SDL_TRUE;
    mitem->name = "\x0e""Show keyboard";
    oric->shut_render(oric);
    oric->init_render(oric);
    setemumode( oric, NULL, EM_RUNNING );
}

void definemapping( struct machine *oric, struct osdmenuitem *mitem, int dummy )
{
    if( oric->define_mapping )
    {
        oric->define_mapping = SDL_FALSE;
        mitem->name = " Define mapping";
        return;
    }

    oric->define_mapping = SDL_TRUE;
    mitem->name = "\x0e""Define mapping";
    if(!oric->show_keyboard) {
        find_item_by_function(keopitems, togglekeyboard)->name = "\x0e""Show keyboard";
        oric->show_keyboard = SDL_TRUE;
        oric->shut_render(oric);
        oric->init_render(oric);
    }
    cmenu = NULL;
    oric->emu_mode = EM_RUNNING;
    do_popup( oric, "Click on an Oric key." );
    setemumode( oric, NULL, EM_RUNNING );
}

void togglestickykeys( struct machine *oric, struct osdmenuitem *mitem, int dummy )
{
    if( oric->sticky_mod_keys )
    {
        oric->sticky_mod_keys = SDL_FALSE;
        mitem->name = " Sticky mod keys";
        release_sticky_keys();
        return;
    }

    oric->sticky_mod_keys = SDL_TRUE;
    mitem->name = "\x0e""Sticky mod keys";
    setemumode( oric, NULL, EM_RUNNING );
}


void savemapping( struct machine *oric, struct osdmenuitem *mitem, int dummy )
{
    if( !filerequester( oric, "Save Keyboard Mapping", mappingpath, mappingfile, FR_KEYMAPPINGSAVE ) ) return;
    joinpath( mappingpath, mappingfile );
    save_keyboard_mapping(oric, filetmp);
    setemumode( oric, NULL, EM_RUNNING );
}

void loadmapping( struct machine *oric, struct osdmenuitem *mitem, int dummy )
{
    if( !filerequester( oric, "Load Keyboard Mapping", mappingpath, mappingfile, FR_KEYMAPPINGLOAD ) ) return;
    joinpath( mappingpath, mappingfile );
    load_keyboard_mapping(oric, filetmp);
    setemumode( oric, NULL, EM_RUNNING );
}

void resetmapping( struct machine *oric, struct osdmenuitem *mitem, int dummy )
{
    reset_keyboard_mapping(&(oric->keyboard_mapping));
    setemumode( oric, NULL, EM_RUNNING );
}

#ifdef __CBCOPY__
void clipbd_copy_gui( struct machine *oric, struct osdmenuitem *mitem, int dummy )
{
    clipboard_copy(oric);
    setemumode( oric, NULL, EM_RUNNING );
}
#endif
#ifdef __CBPASTE__
void clipbd_paste_gui( struct machine *oric, struct osdmenuitem *mitem, int dummy )
{
    clipboard_paste(oric);
    setemumode( oric, NULL, EM_RUNNING );
}
#endif


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
        case SDLK_KP_ENTER:
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

  if (*needrender) ula_set_dirty(oric);
  return done;
}

void set_render_mode( struct machine *oric, int whichrendermode )
{
  oric->rendermode = whichrendermode;
  switch( whichrendermode )
  {
    case RENDERMODE_SW:
      if (oric->sw_depth == 8)
      {
        oric->render_begin          = render_begin_sw8;
        oric->render_end            = render_end_sw8;
        oric->render_textzone_alloc = render_textzone_alloc_sw8;
        oric->render_textzone_free  = render_textzone_free_sw8;
        oric->render_textzone       = render_textzone_sw8;
        oric->render_clear_area     = render_clear_area_sw8;
        oric->render_gimg           = render_gimg_sw8;
        oric->render_gimgpart       = render_gimgpart_sw8;
        oric->render_video          = render_video_sw8;
        oric->render_togglefullscreen = render_togglefullscreen_sw8;
        oric->init_render           = init_render_sw8;
        oric->shut_render           = shut_render_sw8;
        find_item_by_function_and_arg(vdopitems, swap_render_mode, RENDERMODE_GL)->name = " OpenGL rendering";
        find_item_by_function_and_arg(glopitems, swap_render_mode, RENDERMODE_GL)->name = " OpenGL rendering";
        find_item_by_function_and_arg(vdopitems, swap_render_mode, RENDERMODE_SW)->name = "\x0e""Software rendering";
        find_item_by_function_and_arg(glopitems, swap_render_mode, RENDERMODE_SW)->name = "\x0e""Software rendering";
        menus[4].items = vdopitems;
        break;
      }
      oric->render_begin          = render_begin_sw;
      oric->render_end            = render_end_sw;
      oric->render_textzone_alloc = render_textzone_alloc_sw;
      oric->render_textzone_free  = render_textzone_free_sw;
      oric->render_textzone       = render_textzone_sw;
      oric->render_clear_area     = render_clear_area_sw;
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
      find_item_by_function_and_arg(vdopitems, swap_render_mode, RENDERMODE_GL)->name = " OpenGL rendering";
      find_item_by_function_and_arg(glopitems, swap_render_mode, RENDERMODE_GL)->name = " OpenGL rendering";
      find_item_by_function_and_arg(vdopitems, swap_render_mode, RENDERMODE_SW)->name = "\x0e""Software rendering";
      find_item_by_function_and_arg(glopitems, swap_render_mode, RENDERMODE_SW)->name = "\x0e""Software rendering";
      menus[4].items = vdopitems;
      break;

#ifdef __OPENGL_AVAILABLE__
    case RENDERMODE_GL:
      oric->render_begin          = render_begin_gl;
      oric->render_end            = render_end_gl;
      oric->render_textzone_alloc = render_textzone_alloc_gl;
      oric->render_textzone_free  = render_textzone_free_gl;
      oric->render_textzone       = render_textzone_gl;
      oric->render_clear_area     = render_clear_area_gl;
      oric->render_gimg           = render_gimg_gl;
      oric->render_gimgpart       = render_gimgpart_gl;
      oric->render_video          = render_video_gl;
      oric->render_togglefullscreen = render_togglefullscreen_gl;
      oric->init_render           = init_render_gl;
      oric->shut_render           = shut_render_gl;
      find_item_by_function_and_arg(vdopitems, swap_render_mode, RENDERMODE_GL)->name = "\x0e""OpenGL rendering";
      find_item_by_function_and_arg(glopitems, swap_render_mode, RENDERMODE_GL)->name = "\x0e""OpenGL rendering";
      find_item_by_function_and_arg(vdopitems, swap_render_mode, RENDERMODE_SW)->name = " Software rendering";
      find_item_by_function_and_arg(glopitems, swap_render_mode, RENDERMODE_SW)->name = " Software rendering";
      menus[4].items = glopitems;
      break;
#endif

    default:
      oric->render_begin          = render_begin_null;
      oric->render_end            = render_end_null;
      oric->render_textzone_alloc = render_textzone_alloc_null;
      oric->render_textzone_free  = render_textzone_free_null;
      oric->render_textzone       = render_textzone_null;
      oric->render_clear_area     = render_clear_area_null;
      oric->render_gimg           = render_gimg_null;
      oric->render_gimgpart       = render_gimgpart_null;
      oric->render_video          = render_video_null;
      oric->render_togglefullscreen = render_togglefullscreen_null;
      oric->init_render           = init_render_null;
      oric->shut_render           = shut_render_null;
      find_item_by_function_and_arg(vdopitems, swap_render_mode, RENDERMODE_GL)->name = " OpenGL rendering";
      find_item_by_function_and_arg(glopitems, swap_render_mode, RENDERMODE_GL)->name = " OpenGL rendering";
      find_item_by_function_and_arg(vdopitems, swap_render_mode, RENDERMODE_SW)->name = " Software rendering";
      find_item_by_function_and_arg(glopitems, swap_render_mode, RENDERMODE_SW)->name = " Software rendering";
      menus[4].items = vdopitems;
      break;
  }
}

void swap_render_mode( struct machine *oric, struct osdmenuitem *mitem, int newrendermode )
{
  if( oric->rendermode == newrendermode ) return;

  shut_gui( oric );
  shut_joy( oric );
  SDL_Quit();
  need_sdl_quit = SDL_FALSE;

  // Go SDL!
  if( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_AUDIO ) < 0 )
  {
    printf( "SDL init failed\n" );
    oric->emu_mode = EM_PLEASEQUIT;
    return;
  }
  need_sdl_quit = SDL_TRUE;

#ifndef __APPLE__
  SDL_WM_SetIcon( SDL_LoadBMP( IMAGEPREFIX"winicon.bmp" ), NULL );
#endif

  if( !init_joy( oric ) )
  {
    oric->emu_mode = EM_PLEASEQUIT;
    return;
  }

  if( !init_gui( oric, newrendermode ) )
  {
    oric->emu_mode = EM_PLEASEQUIT;
    return;
  }

  if( !ay_init( &oric->ay, oric ) )
  {
    oric->emu_mode = EM_PLEASEQUIT;
    return;
  }

  if( !init_filerequester( oric ) )
  {
    oric->emu_mode = EM_PLEASEQUIT;
    return;
  }

  if( !init_msgbox( oric ) )
  {
    oric->emu_mode = EM_PLEASEQUIT;
    return;
  }

  joy_setup( oric );
  mon_warminit( oric );

  setemumode( oric, NULL, EM_RUNNING );
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
  strcpy( pravdiskpath, FILEPREFIX"pravdisks" );
  strcpy( pravdiskfile, "" );
  strcpy( atmosromfile, ROMPREFIX"basic11b" );
  strcpy( oric1romfile, ROMPREFIX"basic10" );
  strcpy( mdiscromfile, ROMPREFIX"microdis" );
  strcpy( jasmnromfile, ROMPREFIX"jasmin" );
  strcpy( pravetzromfile[0], ROMPREFIX"pravetzt" );
  strcpy( pravetzromfile[1], ROMPREFIX"8dos" );
  telebankfiles[0][0] = 0;
  telebankfiles[1][0] = 0;
  telebankfiles[2][0] = 0;
  telebankfiles[3][0] = 0;
  telebankfiles[4][0] = 0;
  strcpy( telebankfiles[5], ROMPREFIX"teleass" );
  strcpy( telebankfiles[6], ROMPREFIX"hyperbas" );
  strcpy( telebankfiles[7], ROMPREFIX"telmon24" );
  set_render_mode( oric, RENDERMODE_NULL );
  strcpy( snappath, FILEPREFIX"snapshots" );
  strcpy( snapfile, "" );
  strcpy( mappingpath, FILEPREFIX"keymap" );
  strcpy( mappingfile, "" );
}

// Ensure the sanity of toggle menuitems
void setmenutoggles( struct machine *oric )
{
  switch (oric->drivetype)
  {
    case DRV_JASMIN:
    case DRV_MICRODISC:
      find_item_by_key(mainitems, SDLK_0)->func = insertdisk;
      find_item_by_key(mainitems, SDLK_1)->func = insertdisk;
      find_item_by_key(mainitems, SDLK_2)->func = insertdisk;
      find_item_by_key(mainitems, SDLK_3)->func = insertdisk;
      break;

    case DRV_PRAVETZ:
    default:
      find_item_by_key(mainitems, SDLK_0)->func = insertdisk;
      find_item_by_key(mainitems, SDLK_1)->func = insertdisk;
      find_item_by_key(mainitems, SDLK_2)->func = NULL;
      find_item_by_key(mainitems, SDLK_3)->func = NULL;
      break;
  }

  if( soundavailable && soundon )
    find_item_by_function(auopitems, togglesound)->name = "\x0e""Sound enabled";
  else
    find_item_by_function(auopitems, togglesound)->name = " Sound enabled";

  if( oric->tapenoise )
    find_item_by_function(auopitems, toggletapenoise)->name = "\x0e""Tape noise";
  else
    find_item_by_function(auopitems, toggletapenoise)->name = " Tape noise";

  if( oric->tapeturbo )
    find_item_by_function(hwopitems, toggletapeturbo)->name = "\x0e""Turbo tape";
  else
    find_item_by_function(hwopitems, toggletapeturbo)->name = " Turbo tape";

  if( oric->autoinsert )
    find_item_by_function(hwopitems, toggleautoinsrt)->name = "\x0e""Autoinsert tape";
  else
    find_item_by_function(hwopitems, toggleautoinsrt)->name = " Autoinsert tape";

  if( oric->autorewind )
    find_item_by_function(hwopitems, toggleautowind)->name = "\x0e""Autorewind tape";
  else
    find_item_by_function(hwopitems, toggleautowind)->name = " Autorewind tape";

  if( oric->vsynchack )
    find_item_by_function(hwopitems, togglevsynchack)->name = "\x0e""VSync hack";
  else
    find_item_by_function(hwopitems, togglevsynchack)->name = " VSync hack";

  if( oric->lightpen )
    find_item_by_function(hwopitems, togglelightpen)->name = "\x0e""Lightpen";
  else
    find_item_by_function(hwopitems, togglelightpen)->name = " Lightpen";

  find_item_by_function(hwopitems, toggleaciabackend)->name = aciabackends[oric->aciabackend];

  if( oric->symbolsautoload )
    find_item_by_function(dbopitems, togglesymbolsauto)->name = "\x0e""Autoload symbols file";
  else
    find_item_by_function(dbopitems, togglesymbolsauto)->name = " Autoload symbols file";

  if( oric->symbolscase )
    find_item_by_function(dbopitems, togglecasesyms)->name = "\x0e""Case-sensitive symbols";
  else
    find_item_by_function(dbopitems, togglecasesyms)->name = " Case-sensitive symbols";

  if( fullscreen )
  {
    find_item_by_function(vdopitems, togglefullscreen)->name = "\x0e""Fullscreen";
    find_item_by_function(glopitems, togglefullscreen)->name = "\x0e""Fullscreen";
  } else {
    find_item_by_function(vdopitems, togglefullscreen)->name = " Fullscreen";
    find_item_by_function(glopitems, togglefullscreen)->name = " Fullscreen";
  }

  if( oric->hstretch )
    find_item_by_function(glopitems, togglehstretch)->name = "\x0e""Horizontal stretch";
  else
    find_item_by_function(glopitems, togglehstretch)->name = " Horizontal stretch";

  if( oric->scanlines )
  {
    find_item_by_function(vdopitems, togglescanlines)->name = "\x0e""Scanlines";
    find_item_by_function(glopitems, togglescanlines)->name = "\x0e""Scanlines";
  } else {
    find_item_by_function(vdopitems, togglescanlines)->name = " Scanlines";
    find_item_by_function(glopitems, togglescanlines)->name = " Scanlines";
  }

  if( oric->palghost )
    find_item_by_function(glopitems, togglepalghost)->name = "\x0e""PAL ghosting";
  else
    find_item_by_function(glopitems, togglepalghost)->name = " PAL ghosting";


  find_item_by_function_and_arg(hwopitems, swapmach, (0xffff<<16)|MACH_ORIC1      )->name = oric->type==MACH_ORIC1     ? "\x0e""Oric-1"     : " Oric-1";
  find_item_by_function_and_arg(hwopitems, swapmach, (0xffff<<16)|MACH_ORIC1_16K  )->name = oric->type==MACH_ORIC1_16K ? "\x0e""Oric-1 16K" : " Oric-1 16K";
  find_item_by_function_and_arg(hwopitems, swapmach, (0xffff<<16)|MACH_ATMOS      )->name = oric->type==MACH_ATMOS     ? "\x0e""Atmos"      : " Atmos";
  find_item_by_function_and_arg(hwopitems, swapmach, (DRV_NONE<<16)|MACH_TELESTRAT)->name = oric->type==MACH_TELESTRAT ? "\x0e""Telestrat"  : " Telestrat";
  find_item_by_function_and_arg(hwopitems, swapmach, (0xffff<<16)|MACH_PRAVETZ    )->name = oric->type==MACH_PRAVETZ   ? "\x0e""Pravetz 8D" : " Pravetz 8D";

  find_item_by_key(hwopitems, 'm')->func = microdiscrom_valid ? setdrivetype : NULL;
  find_item_by_key(hwopitems, 'j')->func = jasminrom_valid ? setdrivetype : NULL;
  find_item_by_key(hwopitems, 'p')->func = pravetzrom_valid ? setdrivetype : NULL;

  find_item_by_key(hwopitems, 'x')->name = oric->drivetype==DRV_NONE      ? "\x0e""No disk"        : " No disk";
  find_item_by_key(hwopitems, 'm')->name = oric->drivetype==DRV_MICRODISC ? "\x0e""Microdisc"      : " Microdisc";
  find_item_by_key(hwopitems, 'j')->name = oric->drivetype==DRV_JASMIN    ? "\x0e""Jasmin"         : " Jasmin";
  find_item_by_key(hwopitems, 'p')->name = oric->drivetype==DRV_PRAVETZ   ? "\x0e""Pravetz 8D disk": " Pravetz 8D disk";

  if(oric->show_keyboard)
     find_item_by_function(keopitems, togglekeyboard)->name = "\x0e""Show keyboard";
  else
     find_item_by_function(keopitems, togglekeyboard)->name = " Show keyboard";

  if(oric->show_keyboard)
     find_item_by_function(keopitems, togglestickykeys)->name = "\x0e""Sticky mod keys";
  else
     find_item_by_function(keopitems, togglestickykeys)->name = " Sticky mod keys";
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
  if( SDL_OpenAudio( &wanted, &obtained ) >= 0 )
  {
    soundon = SDL_TRUE;
    soundavailable = SDL_TRUE;
    soundsilence = obtained.silence * 8192;
    cyclespersample = ((CYCLESPERSECOND<<FPBITS)/obtained.freq);
  }

  setmenutoggles( oric );
#if defined(__APPLE__) || defined(__BEOS__) || defined(__HAIKU__)
  init_gui_native( oric );
#elif defined(__WIN32__) || defined(__CYGWIN__)
  init_gui_native( oric );
#elif defined(__linux__)
  init_gui_native( oric );
#endif
  return SDL_TRUE;
}

// Bye bye.
void shut_gui( struct machine *oric )
{
  int i;

  oric->shut_render( oric );

  for( i=0; i<NUM_TZ; i++ )
    free_textzone( oric, i );

#if defined(__APPLE__) || defined(__BEOS__) || defined(__HAIKU__)
    shut_gui_native( oric );
#elif defined(__WIN32__) || defined(__CYGWIN__)
    shut_gui_native( oric );
#elif defined(__linux__)
    shut_gui_native( oric );
#endif
}
