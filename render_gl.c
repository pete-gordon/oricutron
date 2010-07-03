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
**  OpenGL rendering
*/

#ifdef __OPENGL_AVAILABLE__

#include "system.h"

#ifndef __APPLE__
#include <GL/gl.h>
#else
#include <OpenGL/gl.h>
#endif

#ifndef __WIN32__
#include <SDL_endian.h>
#endif

#include "6502.h"
#include "via.h"
#include "8912.h"
#include "disk.h"
#include "gui.h"
#include "monitor.h"
#include "machine.h"
#include "render_gl.h"

#define TEX_VIDEO     (0)
#define TEX_SCANLINES (1)

#define TEX_TZ        (TEX_SCANLINES+1)
#define TEX_TZ_LAST   ((NUM_TZ+TEX_TZ)-1)

#define TEX_GIMG      (TEX_TZ_LAST+1)
#define TEX_GIMG_LAST ((NUM_GIMG+TEX_GIMG)-1)

#define NUM_TEXTURES  (TEX_GIMG_LAST+1)

extern SDL_bool refreshstatus;

struct texture
{
  int w, h;
  float tw, th;
  Uint8 *buf;
};

static GLuint tex[NUM_TEXTURES];
static struct texture tx[NUM_TEXTURES];

static struct SDL_Surface *screen;
static Uint32 gpal[NUM_GUI_COLS];

static float clrcol[3];

extern unsigned char sgpal[];
extern SDL_bool fullscreen;
extern struct textzone *tz[NUM_TZ];
extern Uint8 oricpalette[];
extern struct guiimg gimgs[NUM_GIMG];

// Our "lovely" hand-coded font
extern unsigned char thefont[];

// Print a char onto a textzone texture
//   i         = textzone number
//   x,y       = location
//   ch        = which char to draw
//   fcol      = foreground colour
//   bcol      = background colour
//   solidfont = use background colour
static void printchar( int i, int x, int y, unsigned char ch, Uint32 fcol, Uint32 bcol, SDL_bool solidfont )
{
  int px, py, c;
  unsigned char *fptr;
  struct texture *ptx = &tx[i+TEX_TZ];
  Uint32 *ptr;

  if( ch > 127 ) return;

  ptr = (Uint32 *)&ptx->buf[(y*ptx->w+x)*4];

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

    ptr += ptx->w-8;
    fptr++;
  }
}

static void update_textzone_texture( struct machine *oric, int i )
{
  int x, y, px, py, o;

  if( !tz[i] ) return;
  if( !tz[i]->modified ) return;
  if( !tx[i+TEX_TZ].buf ) return;

  o = 0;

  for( y=0, py=0; y<tz[i]->h; y++, py+=12 )
  {
    for( x=0, px=0; x<tz[i]->w; x++, o++, px+=8 )
      printchar( i, px, py, tz[i]->tx[o], gpal[tz[i]->fc[o]], gpal[tz[i]->bc[o]], SDL_TRUE );
  }
  
  glBindTexture( GL_TEXTURE_2D, tex[i+TEX_TZ] );
  glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, tx[i+TEX_TZ].w, tx[i+TEX_TZ].h, 0, GL_RGBA, GL_UNSIGNED_BYTE, tx[i+TEX_TZ].buf );

  tz[i]->modified = SDL_FALSE;
}

static void update_video_texture( struct machine *oric )
{
  int x, y, o, c;
  Uint8 *sptr;

  o = 0;
  sptr = oric->scr;

  for( y=0; y<225; y++ )
  {
    for( x=0; x<240; x++ )
    {
      c = *(sptr++) * 3;
      tx[TEX_VIDEO].buf[o++] = oricpalette[c++];
      tx[TEX_VIDEO].buf[o++] = oricpalette[c++];
      tx[TEX_VIDEO].buf[o++] = oricpalette[c++];
      tx[TEX_VIDEO].buf[o++] = 0xff;
    }
    
    // Repeat the right and bottom borders to prevent linear interpolation to
    // garbage at the edges (GL_CLAMP takes care of the left and top)
    tx[TEX_VIDEO].buf[o++] = oricpalette[c-3];
    tx[TEX_VIDEO].buf[o++] = oricpalette[c-2];
    tx[TEX_VIDEO].buf[o++] = oricpalette[c-1];
    tx[TEX_VIDEO].buf[o++] = 0xff;

    o += (tx[TEX_VIDEO].w-241) * 4;
    if( y == 223 ) { sptr -= 240; }
  }

  glBindTexture( GL_TEXTURE_2D, tex[TEX_VIDEO] );
  glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, tx[TEX_VIDEO].w, tx[TEX_VIDEO].h, 0, GL_RGBA, GL_UNSIGNED_BYTE, tx[TEX_VIDEO].buf );
}

void render_begin_gl( struct machine *oric )
{
  int i;
  refreshstatus = SDL_TRUE;

  glClearColor(clrcol[0], clrcol[1], clrcol[2], 1.0f);
  glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

  update_video_texture( oric );
  for( i=0; i<NUM_TZ; i++ )
    update_textzone_texture( oric, i );

  glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
  glEnable( GL_TEXTURE_2D );
  glEnable( GL_BLEND );
}

void render_end_gl( struct machine *oric )
{
  SDL_GL_SwapBuffers();
}

static unsigned int rounduppow2( unsigned int x )
{
  x--;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  x++;

  return x;
}

void render_textzone_alloc_gl( struct machine *oric, int i )
{
  render_textzone_free_gl( oric, i );

  if( !tz[i] ) return;

  tx[i+TEX_TZ].w = rounduppow2( tz[i]->w * 8 );
  tx[i+TEX_TZ].h = rounduppow2( tz[i]->h * 12 );
  tx[i+TEX_TZ].buf = malloc( tx[i+TEX_TZ].w*tx[i+TEX_TZ].h*4 );
  if( !tx[i+TEX_TZ].buf )
  {
    tx[i+TEX_TZ].w = 0;
    tx[i+TEX_TZ].h = 0;
    return;
  }

  tx[i+TEX_TZ].tw = ((float)(tz[i]->w*8)) / ((float)tx[i+TEX_TZ].w);
  tx[i+TEX_TZ].th = ((float)(tz[i]->h*12)) / ((float)tx[i+TEX_TZ].h);

  glBindTexture( GL_TEXTURE_2D, tex[i+TEX_TZ] );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
  glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, tx[i+TEX_TZ].w, tx[i+TEX_TZ].h, 0, GL_RGBA, GL_UNSIGNED_BYTE, tx[i+TEX_TZ].buf );

  tz[i]->modified = SDL_TRUE;
}

void render_textzone_free_gl( struct machine *oric, int i )
{
  if( tx[i+TEX_TZ].buf )
  {
    free( tx[i+TEX_TZ].buf );
    tx[i+TEX_TZ].buf = NULL;
  }

  tx[i+TEX_TZ].w = 0;
  tx[i+TEX_TZ].h = 0;
}

void render_textzone_gl( struct machine *oric, int i )
{
  if( !tz[i] ) return;
  if( !tx[i+TEX_TZ].buf ) return;

  glBindTexture( GL_TEXTURE_2D, tex[i+TEX_TZ] );
  glBegin( GL_QUADS );
    glTexCoord2f(            0.0f,            0.0f ); glVertex3f(            tz[i]->x,             tz[i]->y, 0.0f );
    glTexCoord2f( tx[i+TEX_TZ].tw,            0.0f ); glVertex3f( tz[i]->x+tz[i]->w*8,             tz[i]->y, 0.0f );
    glTexCoord2f( tx[i+TEX_TZ].tw, tx[i+TEX_TZ].th ); glVertex3f( tz[i]->x+tz[i]->w*8, tz[i]->y+tz[i]->h*12, 0.0f );
    glTexCoord2f(            0.0f, tx[i+TEX_TZ].th ); glVertex3f(            tz[i]->x, tz[i]->y+tz[i]->h*12, 0.0f );
  glEnd();
}

void render_gimg_gl( int i, Sint32 xp, Sint32 yp )
{
  glBindTexture( GL_TEXTURE_2D, tex[i+TEX_GIMG] );
  glBegin( GL_QUADS );
    glTexCoord2f(              0.0f,              0.0f ); glVertex3f(            xp,            yp, 0.0f );
    glTexCoord2f( tx[i+TEX_GIMG].tw,              0.0f ); glVertex3f( xp+gimgs[i].w,            yp, 0.0f );
    glTexCoord2f( tx[i+TEX_GIMG].tw, tx[i+TEX_GIMG].th ); glVertex3f( xp+gimgs[i].w, yp+gimgs[i].h, 0.0f );
    glTexCoord2f(              0.0f, tx[i+TEX_GIMG].th ); glVertex3f(            xp, yp+gimgs[i].h, 0.0f );
  glEnd();
}

void render_gimgpart_gl( int i, Sint32 xp, Sint32 yp, Sint32 ox, Sint32 oy, Sint32 w, Sint32 h )
{
  float texl, text, texr, texb;

  texl = ((float)ox)/((float)tx[i+TEX_GIMG].w);
  text = ((float)oy)/((float)tx[i+TEX_GIMG].h);
  texr = ((float)ox+w)/((float)tx[i+TEX_GIMG].w);
  texb = ((float)oy+h)/((float)tx[i+TEX_GIMG].h);

  glBindTexture( GL_TEXTURE_2D, tex[i+TEX_GIMG] );
  glBegin( GL_QUADS );
    glTexCoord2f( texl, text ); glVertex3f(   xp,   yp, 0.0f );
    glTexCoord2f( texr, text ); glVertex3f( xp+w,   yp, 0.0f );
    glTexCoord2f( texr, texb ); glVertex3f( xp+w, yp+h, 0.0f );
    glTexCoord2f( texl, texb ); glVertex3f(   xp, yp+h, 0.0f );
  glEnd();
}

void render_video_gl( struct machine *oric, SDL_bool doublesize )
{
  float l, r;
  int y;

  glBindTexture( GL_TEXTURE_2D, tex[TEX_VIDEO] );
  glColor4ub( 255, 255, 255, 255 );
  glLoadIdentity();

  if( doublesize )
  {
    if( oric->hstretch )
    {
      l = 0.0f;
      r = 640.f;
    } else {
      l = 320.0f-240.0f;
      r = 320.0f+240.0f;
    }

    glBegin( GL_QUADS );
      glTexCoord2f(          0.0f,          0.0f ); glVertex3f( l,  14.0f, 0.0f );
      glTexCoord2f( 240.0f/256.0f,          0.0f ); glVertex3f( r,  14.0f, 0.0f );
      glTexCoord2f( 240.0f/256.0f, 224.0f/256.0f ); glVertex3f( r, 462.0f, 0.0f );
      glTexCoord2f(          0.0f, 224.0f/256.0f ); glVertex3f( l, 462.0f, 0.0f );
    glEnd();

    if( !oric->scanlines ) return;

    glBindTexture( GL_TEXTURE_2D, tex[TEX_SCANLINES] );
    glBegin( GL_QUADS );
    for( y=14; y<224*2+14; y+=32 )
    {
      glTexCoord2f( 0.0f, 0.0f ); glVertex3f( l,    y, 0.0f );
      glTexCoord2f( 1.0f, 0.0f ); glVertex3f( r,    y, 0.0f );
      glTexCoord2f( 1.0f, 1.0f ); glVertex3f( r, y+32, 0.0f );
      glTexCoord2f( 0.0f, 1.0f ); glVertex3f( l, y+32, 0.0f );
    }
    glEnd();
    return;
  }

  glBegin( GL_QUADS );
    glTexCoord2f(          0.0f,          0.0f ); glVertex3f(   0.0f,   4.0f, 0.0f );
    glTexCoord2f( 240.0f/256.0f,          0.0f ); glVertex3f( 240.0f,   4.0f, 0.0f );
    glTexCoord2f( 240.0f/256.0f, 224.0f/256.0f ); glVertex3f( 240.0f, 228.0f, 0.0f );
    glTexCoord2f(          0.0f, 224.0f/256.0f ); glVertex3f(   0.0f, 228.0f, 0.0f );
  glEnd();
}

void preinit_render_gl( struct machine *oric )
{
  Sint32 i;

  screen = NULL;

  for( i=0; i<NUM_TEXTURES; i++ )
  {
    tx[i].w   = 0;
    tx[i].h   = 0;
    tx[i].buf = NULL;
  }
}

SDL_bool init_render_gl( struct machine *oric )
{
  int depth, i, x, y;
  const SDL_VideoInfo *info = NULL;

  depth = 32;
  if( (info = SDL_GetVideoInfo()) )
    depth = info->vfmt->BitsPerPixel;

  screen = SDL_SetVideoMode( 640, 480, depth, fullscreen ? SDL_OPENGL|SDL_FULLSCREEN : SDL_OPENGL );
  if( !screen )
  {
    printf( "SDL video failed\n" );
    return SDL_FALSE;
  }

  glMatrixMode( GL_PROJECTION );
  glOrtho( 0.0f, 640.0f, 480.0f, 0.0f, 0.0f, 1.0f );
  glMatrixMode( GL_MODELVIEW );
  glLoadIdentity();
  glDisable( GL_DEPTH_TEST );

  glGenTextures( NUM_TEXTURES, tex );

  // Allocate texture buffers
  tx[TEX_VIDEO].w = 256;
  tx[TEX_VIDEO].h = 256;
  tx[TEX_VIDEO].buf = malloc( 256*256*4 );
  if( !tx[TEX_VIDEO].buf ) return SDL_FALSE;

  glBindTexture( GL_TEXTURE_2D, tex[TEX_VIDEO] );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
  glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, tx[TEX_VIDEO].w, tx[TEX_VIDEO].h, 0, GL_RGBA, GL_UNSIGNED_BYTE, tx[TEX_VIDEO].buf );

  tx[TEX_SCANLINES].w = 32;
  tx[TEX_SCANLINES].h = 32;
  tx[TEX_SCANLINES].buf = malloc( 32*32*4 );
  if( !tx[TEX_SCANLINES].buf ) return SDL_FALSE;

  for( y=0; y<32; y++ )
  {
    for( x=0; x<32*4; )
    {
      tx[TEX_SCANLINES].buf[y*32*4+x++] = 0x00;
      tx[TEX_SCANLINES].buf[y*32*4+x++] = 0x00;
      tx[TEX_SCANLINES].buf[y*32*4+x++] = 0x00;
      tx[TEX_SCANLINES].buf[y*32*4+x++] = (y&1) ? 0x50 : 0x00;
    }
  }

  glBindTexture( GL_TEXTURE_2D, tex[TEX_SCANLINES] );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
  glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, tx[TEX_SCANLINES].w, tx[TEX_SCANLINES].h, 0, GL_RGBA, GL_UNSIGNED_BYTE, tx[TEX_SCANLINES].buf );

  for( i=0; i<NUM_GIMG; i++ )
  {
    tx[i+TEX_GIMG].w   = rounduppow2( gimgs[i].w );
    tx[i+TEX_GIMG].h   = rounduppow2( gimgs[i].h );
    tx[i+TEX_GIMG].buf = malloc( tx[i+TEX_GIMG].w * tx[i+TEX_GIMG].h * 4 );
    if( !tx[i+TEX_GIMG].buf ) return SDL_FALSE;

    tx[i+TEX_GIMG].tw = ((float)gimgs[i].w) / ((float)tx[i+TEX_GIMG].w);
    tx[i+TEX_GIMG].th = ((float)gimgs[i].h) / ((float)tx[i+TEX_GIMG].h);

    for( y=0; y<gimgs[i].h; y++ )
    {
      for( x=0; x<gimgs[i].w; x++ )
      {
        tx[i+TEX_GIMG].buf[(y*tx[i+TEX_GIMG].w+x)*4  ] = gimgs[i].buf[(y*gimgs[i].w+x)*3];
        tx[i+TEX_GIMG].buf[(y*tx[i+TEX_GIMG].w+x)*4+1] = gimgs[i].buf[(y*gimgs[i].w+x)*3+1];
        tx[i+TEX_GIMG].buf[(y*tx[i+TEX_GIMG].w+x)*4+2] = gimgs[i].buf[(y*gimgs[i].w+x)*3+2];
        tx[i+TEX_GIMG].buf[(y*tx[i+TEX_GIMG].w+x)*4+3] = 0xff;
      }
    }
    glBindTexture( GL_TEXTURE_2D, tex[i+TEX_GIMG] );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, tx[i+TEX_GIMG].w, tx[i+TEX_GIMG].h, 0, GL_RGBA, GL_UNSIGNED_BYTE, tx[i+TEX_GIMG].buf );
  }

  // Get the GUI palette
  for( i=0; i<NUM_GUI_COLS; i++ )
  {
    gpal[i] = (sgpal[i*3]<<24)|(sgpal[i*3+1]<<16)|(sgpal[i*3+2]<<8)|0xff;
    gpal[i] = _BE32( gpal[i] );
  }

  for( i=0; i<NUM_TZ; i++ )
    render_textzone_alloc_gl( oric, i );

  SDL_WM_SetCaption( APP_NAME_FULL, APP_NAME_FULL );

  clrcol[0] = ((float)sgpal[4*3+0])/255.0f;
  clrcol[1] = ((float)sgpal[4*3+1])/255.0f;
  clrcol[2] = ((float)sgpal[4*3+2])/255.0f;

  return SDL_TRUE;
}

void shut_render_gl( struct machine *oric )
{
  int i;

  for( i=0; i<NUM_TEXTURES; i++ )
  {
    if( tx[i].buf ) free( tx[i].buf );
    tx[i].buf = NULL;
    tx[i].w = 0;
    tx[i].h = 0;
  }
}

#endif //__OPENGL_AVAILABLE__
