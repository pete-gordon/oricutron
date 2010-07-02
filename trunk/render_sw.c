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

#include "system.h"
#include "6502.h"
#include "via.h"
#include "8912.h"
#include "disk.h"
#include "gui.h"
#include "monitor.h"
#include "machine.h"
#include "render_sw.h"

static struct SDL_Surface *screen;
static Uint16 gpal[NUM_GUI_COLS];
// Cached screen->pitch
static int pixpitch;

static Uint16 pal[8]; // Palette
static Uint32 dpal[8];

extern SDL_bool fullscreen, hwsurface;
static SDL_bool needclr;
extern struct textzone *tz[NUM_TZ];
extern unsigned char sgpal[];
extern Uint8 oricpalette[];

// Our "lovely" hand-coded font
extern unsigned char thefont[];

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

void render_begin_sw( struct machine *oric )
{
  if( SDL_MUSTLOCK( screen ) )
    SDL_LockSurface( screen );
}

void render_end_sw( struct machine *oric )
{
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

// Copy the video output buffer to the SDL surface
void render_video_sw( struct machine *oric, SDL_bool doublesize )
{
  int x, y;
  Uint8 *sptr;
  Uint16 *dptr;
  Uint32 *dptr2, *dptr3, c, pp2;

  if( !oric->scr )
    return;
    
  pp2 = pixpitch/2;

  if( doublesize )
  {
    if( needclr )
    {
      dptr = (Uint16 *)screen->pixels;
      for( y=0; y<480; y++ )
      {
        for( x=0; x<640; x++ )
          *(dptr++) = gpal[4];
        dptr += pixpitch-640;
      }
      needclr = SDL_FALSE;
    }

    sptr = oric->scr;
    dptr2 = (Uint32 *)(&((Uint16 *)screen->pixels)[320-240+pixpitch*(240-226)]);
    dptr3 = dptr2+pp2;
    for( y=0; y<224; y++ )
    {
      for( x=0; x<240; x++ )
      {
        c = dpal[*(sptr++)];
        *(dptr2++) = c;
        *(dptr3++) = c;
      }
      dptr2 += pixpitch-240;
      dptr3 += pixpitch-240;
    }
    return;
  }

  needclr = SDL_TRUE;

  sptr = oric->scr;
  dptr = (Uint16 *)screen->pixels;

  for( y=0; y<4; y++ )
  {
    memset( dptr, 0, 480 );
    dptr += pixpitch;
  }
  for( ; y<228; y++ )
  {
    for( x=0; x<240; x++ )
      *(dptr++) = pal[*(sptr++)];
    dptr += pixpitch-240;
  }
}

void preinit_render_sw( struct machine *oric )
{
  screen = NULL;
}

SDL_bool init_render_sw( struct machine *oric )
{
  Sint32 surfacemode;
  int i;

  surfacemode = fullscreen ? SDL_FULLSCREEN : SDL_SWSURFACE;
  if( hwsurface ) { surfacemode &= ~SDL_SWSURFACE; surfacemode |= SDL_HWSURFACE; }

  screen = SDL_SetVideoMode( 640, 480, 16, surfacemode );
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
    pal[i] = SDL_MapRGB( screen->format, oricpalette[i*3], oricpalette[i*3+1], oricpalette[i*3+2] );

  for( i=0; i<8; i++ )
    dpal[i] = (pal[i]<<16)|pal[i];

  pixpitch = screen->pitch / 2;

  needclr = SDL_TRUE;

  return SDL_TRUE;
}

void shut_render_sw( struct machine *oric )
{
  // The surface will be freed by SDL_Quit, or a call to SDL_SetVideoMode from a different render module
}
