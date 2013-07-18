/*
**  Oricutron
**  Copyright (C) 2009-2012 Peter Gordon
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
#include <string.h>

#include "system.h"
#include "6502.h"
#include "via.h"
#include "8912.h"
#include "disk.h"
#include "gui.h"
#include "monitor.h"
#include "6551.h"
#include "machine.h"
#include "render_sw8.h"
#include "ula.h"

// this *should* be 16, but on AmigaOS that messes
// with the pointer colours..
#define GPAL_FIRSTPEN 20

static struct SDL_Surface *screen;

static Uint32 offset_top;
static Uint16 dpen[16];

extern SDL_bool fullscreen, hwsurface;
static SDL_bool needclr;
extern struct textzone *tz[NUM_TZ];
extern unsigned char sgpal[];
extern Uint8 oricpalette[];
extern struct guiimg gimgs[NUM_GIMG];
extern SDL_bool refreshstatus;
static Uint8 *mgimg[NUM_GIMG];
static int next_gimgcol;
static SDL_Color colours[256];

// Our "lovely" hand-coded font
extern unsigned char thefont[];

// --- printchar template function --------------------------------------------
// Print a char onto a X-bit framebuffer, X being a power of 2
//   ptr       = where to draw it
//   ch        = which char to draw
//   fcol      = foreground colour
//   bcol      = background colour
//   solidfont = use background colour

static void printchar( Uint8* ptr, unsigned char ch, Uint8 fcol, Uint8 bcol, SDL_bool solidfont )
{
  int x, y, mask;
  const Uint8 *src_byte;
  Uint8 *dst_pixel;
  Uint8 *dst_scanline;

  if( ch > 127 ) return;

  src_byte = &thefont[ch*12];
  dst_scanline = ptr;

  for( y=12; y!=0; --y, ++src_byte, dst_scanline+=screen->pitch )
  {
    dst_pixel = dst_scanline;

    for( mask=0x80, x=8; x!=0; --x, ++dst_pixel, mask>>=1 )
    {
      if( (*src_byte)&mask )
        *dst_pixel = fcol;
      else if( solidfont )
        *dst_pixel = bcol;
    }
  }
}


void render_begin_sw8( struct machine *oric )
{
  int y;
  Uint8 *dst_scanline;

  if( SDL_MUSTLOCK( screen ) )
    SDL_LockSurface( screen );

  if( oric->newpopupstr )
  {
    dst_scanline = (Uint8*)screen->pixels;
    dst_scanline += 320;

    for( y=12; y!=0; --y, dst_scanline += screen->pitch)
      memset(dst_scanline, GPAL_FIRSTPEN, 320);
    oric->newpopupstr = SDL_FALSE;
  }
  
  if( oric->newstatusstr )
  {
    refreshstatus = SDL_TRUE;
    oric->newstatusstr = SDL_FALSE;
  }
}

void render_end_sw8( struct machine *oric )
{
  int i;
  Uint32 char_pitch;
  Uint8 *dst_pixel;

  char_pitch = 8;

  if( oric->emu_mode == EM_RUNNING )
  {
    if( oric->popupstr[0] )
    {
      dst_pixel = (Uint8*)screen->pixels;
      dst_pixel += 320;

      for( i=0; oric->popupstr[i]; i++, dst_pixel += char_pitch )
        printchar( dst_pixel, oric->popupstr[i], GPAL_FIRSTPEN+1, GPAL_FIRSTPEN, SDL_TRUE );
    }
  
    if( oric->statusstr[0] )
    {
      dst_pixel = (Uint8*)screen->pixels;
      dst_pixel += 466 * screen->pitch;

      for( i=0; oric->statusstr[i]; i++, dst_pixel += char_pitch )
        printchar( dst_pixel, oric->statusstr[i], GPAL_FIRSTPEN+1, 0, SDL_FALSE );
    }
  }

  if( SDL_MUSTLOCK( screen ) )
    SDL_UnlockSurface( screen );

  SDL_Flip( screen );
}

void render_textzone_alloc_sw8( struct machine *oric, int i )
{
}

void render_textzone_free_sw8( struct machine *oric, int i )
{
}

void render_textzone_sw8( struct machine *oric, int i )
{
  int x, y, o;
  struct textzone *ptz = tz[i];
  Uint8 *dst_scanline, *dst_pixel;

  dst_scanline = (Uint8 *)screen->pixels;
  dst_scanline += screen->pitch * ptz->y + ptz->x;

  o = 0;
  for( y=ptz->h; y!=0; --y, dst_scanline+=12*screen->pitch )
  {
    dst_pixel = dst_scanline;

    for( x=ptz->w; x!=0; --x, ++o, dst_pixel+=8 )
      printchar( dst_pixel, ptz->tx[o], GPAL_FIRSTPEN+ptz->fc[o], GPAL_FIRSTPEN+ptz->bc[o], SDL_TRUE );
  }
}

// Draw a GUI image at X,Y
void render_gimg_sw8( int img_id, Sint32 xp, Sint32 yp )
{
  struct guiimg *gi = &gimgs[img_id];
  Uint8 *src_scanline, *dst_scanline;
  Sint32 i;

  src_scanline = mgimg[img_id];

  dst_scanline = (Uint8 *)screen->pixels;
  dst_scanline += screen->pitch * yp + xp;

  for( i=gi->h; i!=0; --i, src_scanline+=gi->w, dst_scanline+=screen->pitch )
    memcpy( dst_scanline, src_scanline, gi->w );
}

// Draw part of an image (xp,yp = screen location, ox, oy = offset into image, w, h = dimensions)
void render_gimgpart_sw8( int img_id, Sint32 xp, Sint32 yp, Sint32 ox, Sint32 oy, Sint32 w, Sint32 h )
{
  struct guiimg *gi = &gimgs[img_id];
  Uint8 *src_scanline, *dst_scanline;
  Sint32 i;

  src_scanline = mgimg[img_id];
  src_scanline += gi->w * oy + ox;

  dst_scanline = (Uint8 *)screen->pixels;
  dst_scanline += screen->pitch * yp + xp;

  for( i=h; i!=0; --i, src_scanline+=gi->w, dst_scanline+=screen->pitch )
    memcpy( dst_scanline, src_scanline, w );
}

// Copy the video output buffer to the SDL surface, assuming 16bpp video mode
void render_video_sw8( struct machine *oric, SDL_bool doublesize )
{
  int x, y;
  Uint8 *src_pixel;
  Sint32 dst_pitch_x2;
  Uint16 c;
  Uint8 *dst_scanline, *dst_even_scanline, *dst_odd_scanline;
  Uint16 *dst_even_pixel, *dst_odd_pixel;

  if( !oric->scr )
    return;

  if( doublesize )
  {
    if( needclr )
    {
      SDL_FillRect(screen, NULL, GPAL_FIRSTPEN);
      needclr = SDL_FALSE;
    }

    src_pixel = oric->scr;

    dst_pitch_x2 = 2 * screen->pitch;

    dst_even_scanline = ((Uint8*)screen->pixels) + offset_top;

    dst_odd_scanline = dst_even_scanline;
    dst_odd_scanline += screen->pitch;

    if( oric->scanlines )
    {
      for( y=0; y<224; y++, dst_even_scanline+=dst_pitch_x2, dst_odd_scanline+=dst_pitch_x2 )
      {
        if (!oric->vid_dirty[y])
        {
          src_pixel += 240;
          continue;
        }
        dst_even_pixel = (Uint16*)dst_even_scanline;
        dst_odd_pixel  = (Uint16*)dst_odd_scanline;

        for( x=240; x!=0; --x )
        {
          c = *(src_pixel++);
          *(dst_even_pixel++) = dpen[c];
          *(dst_odd_pixel++)  = dpen[c+8];
        }
        oric->vid_dirty[y] = SDL_FALSE;

      }
    } else {
      for( y=0; y<224; y++, dst_even_scanline+=dst_pitch_x2, dst_odd_scanline+=dst_pitch_x2 )
      {
        if (!oric->vid_dirty[y])
        {
          src_pixel += 240;
          continue;
        }
        dst_even_pixel = (Uint16*)dst_even_scanline;
        dst_odd_pixel  = (Uint16*)dst_odd_scanline;

        for( x=240; x!=0; --x ) 
        {
            c = dpen[*(src_pixel++)];
            *(dst_even_pixel++) = c;
            *(dst_odd_pixel++)  = c;
        }
        oric->vid_dirty[y] = SDL_FALSE;
      }
    }
    return;
  }

  needclr = SDL_TRUE;

  src_pixel = oric->scr;
  dst_scanline = (Uint8*)screen->pixels;

  for( y=0; y<4; y++ )
  {
    memset( dst_scanline, 0, 240 );
    dst_scanline += screen->pitch;
  }
  for( ; y<228; y++, dst_scanline+=screen->pitch )
  {
    memcpy(dst_scanline, src_pixel, 240);
    src_pixel += 240;
  }
}

void preinit_render_sw8( struct machine *oric )
{
  // Screen surface is not set yet
  screen = NULL;
}

SDL_bool render_togglefullscreen_sw8( struct machine *oric )
{
  fullscreen = SDL_TRUE;
  return SDL_TRUE;
}

static int similar_colour(int i, int r, int g, int b)
{
  return abs(r-(int)colours[i].r)+abs(g-(int)colours[i].g)+abs(b-(int)colours[i].b);
}

static int find_best_pen(Uint8 r, int g, int b)
{
  int i;
  int closest_pen=-1, closest_distance=8000;

  /* First, look for an exact match */
  for( i=0; i<next_gimgcol; i++ )
  {
    if( (colours[i].r == r) &&
        (colours[i].g == g) &&
        (colours[i].b == b) )
      return i;

      if (similar_colour(i, r, g, b) < closest_distance)
      {
        closest_pen = i;
        closest_distance = similar_colour(i, r, g, b);
      }
  }

  /* Any really close? */
  if (closest_distance < 3)
    return closest_pen;

  /* Can we make a new pen? */
  if (next_gimgcol >= 256)
    return closest_pen;

  colours[next_gimgcol].r = r;
  colours[next_gimgcol].g = g;
  colours[next_gimgcol].b = b;
  
  return next_gimgcol++;
}

static SDL_bool guiimg_to_img(Uint8** dst, const struct guiimg* src)
{
  size_t i;
  const Uint8 *src_pixel;
  Uint8 *dst_pixel;

  (*dst) = (Uint8 *)malloc( src->w*src->h );
  if ((*dst) == NULL)
    return SDL_FALSE;

  src_pixel = src->buf;
  dst_pixel = *dst;

  for( i=src->w*src->h; i!=0; --i, src_pixel += 3, ++dst_pixel )
    (*dst_pixel) = find_best_pen(src_pixel[0], src_pixel[1], src_pixel[2]);

  return SDL_TRUE;
}

SDL_bool init_render_sw8( struct machine *oric )
{
  int i;
  Sint32 surfacemode;

  surfacemode = SDL_FULLSCREEN|SDL_HWPALETTE;
  if( hwsurface ) { surfacemode |= SDL_HWSURFACE; }

  // Try to setup the video display
  screen = SDL_SetVideoMode( 640, 480, 8, surfacemode );
  if( !screen )
  {
    printf( "SDL video failed\n" );
    return SDL_FALSE;
  }

  SDL_WM_SetCaption( APP_NAME_FULL, APP_NAME_FULL );

  for( i=0; i<8; i++ )
  {
      colours[i].r   = oricpalette[i*3  ];
      colours[i+8].r = oricpalette[i*3  ]/2;
      colours[i].g   = oricpalette[i*3+1];
      colours[i+8].g = oricpalette[i*3+1]/2;
      colours[i].b   = oricpalette[i*3+2];
      colours[i+8].b = oricpalette[i*3+2]/2;
  }

  for( i=0; i<NUM_GUI_COLS; i++ )
  {
      colours[i+GPAL_FIRSTPEN].r = sgpal[i*3  ];
      colours[i+GPAL_FIRSTPEN].g = sgpal[i*3+1];
      colours[i+GPAL_FIRSTPEN].b = sgpal[i*3+2];
  }

  next_gimgcol = GPAL_FIRSTPEN+NUM_GUI_COLS;

  // Get the images for the GUI
  for( i=0; i<NUM_GIMG; i++ )
    if (!guiimg_to_img(mgimg + i, gimgs + i))
      return SDL_FALSE;

  SDL_SetPalette( screen, SDL_LOGPAL|SDL_PHYSPAL, colours, 0, next_gimgcol);

  // Precompute pixels pairs for efficient rendering double size
  for( i=0; i<8*2; i++ )
    dpen[i] = (i<<8)|i;

  // For the first frame rendered, we need to clean the screen
  needclr = SDL_TRUE;
  refreshstatus = SDL_TRUE;

  // Calculate the offset to render the screen
  offset_top = (240 - 226) * screen->pitch + 80;

  ula_set_dirty( oric );

  // Job done
  return SDL_TRUE;
}

void shut_render_sw8( struct machine *oric )
{
  // The surface will be freed by SDL_Quit, or a call to SDL_SetVideoMode from a different render module
}

