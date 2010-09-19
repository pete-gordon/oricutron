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
**  Software rendering
*/

#include <stdlib.h>

#include "system.h"
#include "6502.h"
#include "via.h"
#include "8912.h"
#include "disk.h"
#include "gui.h"
#include "monitor.h"
#include "6551.h"
#include "machine.h"
#include "render_sw.h"

static struct SDL_Surface *screen;
static Pixel gpal[NUM_GUI_COLS];
// Cached screen->pitch
static int pixpitch;

static Pixel pal[8*2]; // Palette
static Uint32 dpal[8*2];
static Pixel *mgimg[NUM_GIMG];

extern SDL_bool fullscreen, hwsurface;
static SDL_bool needclr;
extern struct textzone *tz[NUM_TZ];
extern unsigned char sgpal[];
extern Uint8 oricpalette[];
extern struct guiimg gimgs[NUM_GIMG];
extern SDL_bool refreshstatus;

// Our "lovely" hand-coded font
extern unsigned char thefont[];

// Print a char onto a 16-bit framebuffer
//   ptr       = where to draw it
//   ch        = which char to draw
//   fcol      = foreground colour
//   bcol      = background colour
//   solidfont = use background colour
static void printchar( Pixel *ptr, unsigned char ch, Pixel fcol, Pixel bcol, SDL_bool solidfont )
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

void render_begin_sw( struct machine *oric )
{
  Pixel *dptr;

  if( SDL_MUSTLOCK( screen ) )
    SDL_LockSurface( screen );

  if( oric->newpopupstr )
  {
    int x, y;
    dptr = &((Pixel *)screen->pixels)[320];
    for( y=0; y<12; y++ )
    {
      for( x=320; x<640; x++ )
        *(dptr++) = gpal[4];
      dptr += pixpitch-320;
    }
    oric->newpopupstr = SDL_FALSE;
  }
  
  if( oric->newstatusstr )
  {
    refreshstatus = SDL_TRUE;
    oric->newstatusstr = SDL_FALSE;
  }
}

void render_end_sw( struct machine *oric )
{
  int i;

  if( oric->emu_mode == EM_RUNNING )
  {
    if( oric->popupstr[0] )
    {
      Pixel *dptr = &((Pixel *)screen->pixels)[320];
      for( i=0; oric->popupstr[i]; i++, dptr += 8 )
        printchar( dptr, oric->popupstr[i], gpal[1], gpal[4], SDL_TRUE );
    }
  
    if( oric->statusstr[0] )
    {
      Pixel *dptr = &((Pixel *)screen->pixels)[pixpitch*466];
      for( i=0; oric->statusstr[i]; i++, dptr += 8 )
        printchar( dptr, oric->statusstr[i], gpal[1], 0, SDL_FALSE );
    }
  }

  if( SDL_MUSTLOCK( screen ) )
    SDL_UnlockSurface( screen );

  SDL_Flip( screen );
}

void render_textzone_alloc_sw( struct machine *oric, int i )
{
}

void render_textzone_free_sw( struct machine *oric, int i )
{
}

void render_textzone_sw( struct machine *oric, int i )
{
  int x, y, o;
  struct textzone *ptz = tz[i];
  Pixel *sp;

  sp = &((Pixel *)screen->pixels)[pixpitch*ptz->y+ptz->x];
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

// Draw a GUI image at X,Y
void render_gimg_sw( int i, Sint32 xp, Sint32 yp )
{
  struct guiimg *gi = &gimgs[i];
  Pixel *sptr, *dptr;
  Sint32 x, y;

  sptr = mgimg[i];
  dptr = &((Pixel *)screen->pixels)[pixpitch*yp+xp];

  for( y=0; y<gi->h; y++ )
  {
    for( x=0; x<gi->w; x++ )
      *(dptr++) = *(sptr++);
    dptr += pixpitch-gi->w;
  }
}

// Draw part of an image (xp,yp = screen location, ox, oy = offset into image, w, h = dimensions)
void render_gimgpart_sw( int i, Sint32 xp, Sint32 yp, Sint32 ox, Sint32 oy, Sint32 w, Sint32 h )
{
  struct guiimg *gi = &gimgs[i];
  Pixel *sptr, *dptr;
  Sint32 x, y;

  sptr = &mgimg[i][oy*gi->w+ox];
  dptr = &((Pixel *)screen->pixels)[pixpitch*yp+xp];

  for( y=0; y<h; y++ )
  {
    for( x=0; x<w; x++ )
      *(dptr++) = *(sptr++);
    sptr += gi->w-w;
    dptr += pixpitch-w;
  }
}

// Copy the video output buffer to the SDL surface
void render_video_sw( struct machine *oric, SDL_bool doublesize )
{
  int x, y;
	Uint8 *lSrcPixel;
  Pixel c, c2;
  Pixel *lDstScanLine, *lDstEvenScanLine, *lDstOddScanLine;
  Pixel *lDstPixel, *lDstEvenPixel, *lDstOddPixel;

  if( !oric->scr )
    return;

  if( doublesize )
  {
    if( needclr )
    {
      SDL_FillRect(screen, NULL, gpal[4]);
      needclr = SDL_FALSE;
    }

    lSrcPixel = oric->scr;

    lDstEvenScanLine = (Pixel*)screen->pixels;
    lDstEvenScanLine += (240 - 226) * pixpitch;
    lDstEvenScanLine += (pixpitch - 2 * 240) / 2;

    lDstOddScanLine = lDstEvenScanLine;
    lDstOddScanLine += pixpitch;

    if( oric->scanlines )
    {
      for( y=224; y!=0; --y, lDstEvenScanLine+=2*pixpitch, lDstOddScanLine+=2*pixpitch )
      {
        lDstEvenPixel = lDstEvenScanLine;
        lDstOddPixel  = lDstOddScanLine;
        for( x=240; x!=0; --x )
        {
          c2 = pal[(*lSrcPixel)+8];
          c = pal[*(lSrcPixel++)];
          *(lDstEvenPixel++) = c;
          *(lDstEvenPixel++) = c;
          *(lDstOddPixel++)  = c2;
          *(lDstOddPixel++)  = c2;
        }

      }
    } else {
      for( y=224; y!=0; --y, lDstEvenScanLine+=2*pixpitch, lDstOddScanLine+=2*pixpitch )
      {
        lDstEvenPixel = lDstEvenScanLine;
        lDstOddPixel  = lDstOddScanLine;
        for( x=240; x!=0; --x ) 
          {
            c = pal[*(lSrcPixel++)];
            *(lDstEvenPixel++) = c;
            *(lDstEvenPixel++) = c;
            *(lDstOddPixel++)  = c;
            *(lDstOddPixel++)  = c;
         }
      }
    }
    return;
  }

  needclr = SDL_TRUE;

  lSrcPixel = oric->scr;
  lDstScanLine = (Pixel*)screen->pixels;

  for( y=0; y<4; y++ )
  {
    memset( lDstScanLine, 0, 480 );
    lDstScanLine += pixpitch;
  }
  for( ; y<228; y++, lDstScanLine += pixpitch)
  {
    lDstPixel = lDstScanLine;
    for(x = 240; x != 0; --x)
      *(lDstPixel++) = pal[*(lSrcPixel++)];
  }
}

void preinit_render_sw( struct machine *oric )
{
  Sint32 i;

  screen = NULL;

  for( i=0; i<NUM_GIMG; i++ )
    mgimg[i] = NULL;
}

SDL_bool render_togglefullscreen_sw( struct machine *oric )
{
#if defined(__amigaos4__) || defined(__linux__)
  // Use SDL_WM_ToggleFullScreen on systems where it is supported
  if( SDL_WM_ToggleFullScreen( screen ) )
  {
    fullscreen = !fullscreen;
    return SDL_TRUE;
  }

  return SDL_FALSE;
#else
  oric->shut_render( oric );
  fullscreen = !fullscreen;
  if( oric->init_render( oric ) ) return SDL_TRUE;
  set_render_mode( oric, RENDERMODE_NULL );
  oric->emu_mode = EM_PLEASEQUIT; 
  return SDL_FALSE;
#endif
}

SDL_bool init_render_sw( struct machine *oric )
{
  Sint32 surfacemode;
  int i, x, y;

  surfacemode = fullscreen ? SDL_FULLSCREEN : SDL_SWSURFACE;
  if( hwsurface ) { surfacemode &= ~SDL_SWSURFACE; surfacemode |= SDL_HWSURFACE; }

  screen = SDL_SetVideoMode( 640, 480, sizeof(Pixel) * 8, surfacemode );
  if( !screen )
  {
    printf( "SDL video failed\n" );
    return SDL_FALSE;
  }

  SDL_WM_SetCaption( APP_NAME_FULL, APP_NAME_FULL );

  // Get the GUI palette
  for( i=0; i<NUM_GUI_COLS; i++ )
    gpal[i] = SDL_MapRGB( screen->format, sgpal[i*3  ], sgpal[i*3+1], sgpal[i*3+2] );

  for( i=0; i<8; i++ )
  {
    pal[i  ] = SDL_MapRGB( screen->format, oricpalette[i*3], oricpalette[i*3+1], oricpalette[i*3+2] );
    pal[i+8] = SDL_MapRGB( screen->format, oricpalette[i*3]/2, oricpalette[i*3+1]/2, oricpalette[i*3+2]/2 );
  }

  for( i=0; i<NUM_GIMG; i++ )
  {
    mgimg[i] = (Pixel *)malloc( gimgs[i].w * gimgs[i].h * sizeof(Pixel) );
    if( !mgimg[i] ) return SDL_FALSE;

    for( y=0; y<gimgs[i].h; y++ )
    {
      for( x=0; x<gimgs[i].w; x++ )
        mgimg[i][y*gimgs[i].w+x] = SDL_MapRGB( screen->format, gimgs[i].buf[(y*gimgs[i].w+x)*3], gimgs[i].buf[(y*gimgs[i].w+x)*3+1], gimgs[i].buf[(y*gimgs[i].w+x)*3+2] );
    }
  }

  for( i=0; i<8*2; i++ )
    dpal[i] = (pal[i]<<16)|pal[i];

  pixpitch = screen->pitch / sizeof(Pixel);

  needclr = SDL_TRUE;
  refreshstatus = SDL_TRUE;

  return SDL_TRUE;
}

void shut_render_sw( struct machine *oric )
{
  Sint32 i;

  for( i=0; i<NUM_GIMG; i++  )
  {
    if( mgimg[i] ) free( mgimg[i] );
    mgimg[i] = NULL;
  }

  // The surface will be freed by SDL_Quit, or a call to SDL_SetVideoMode from a different render module
}
