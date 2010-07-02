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

#include "system.h"

#ifndef __APPLE__
#include <GL/gl.h>
#else
#include <OpenGL/gl.h>
#endif

#include "6502.h"
#include "via.h"
#include "8912.h"
#include "disk.h"
#include "gui.h"
#include "monitor.h"
#include "machine.h"
#include "render_gl.h"

#define TEX_VIDEO   (0)

#define TEX_TZ      (TEX_VIDEO+1)
#define TEX_TZ_LAST ((NUM_TZ+TEX_TZ)-1)

#define NUM_TEXTURES (TEX_TZ_LAST+1)

struct texture
{
  int w, h;
  Uint8 *buf;
};

static GLuint tex[NUM_TEXTURES];
static struct texture tx[NUM_TEXTURES];

static struct SDL_Surface *screen;

extern SDL_bool fullscreen;
extern struct textzone *tz[NUM_TZ];

void render_begin_gl( struct machine *oric )
{
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
}

void render_end_gl( struct machine *oric )
{
  SDL_GL_SwapBuffers();
}

void render_textzone_gl( struct machine *oric, struct textzone *ptz )
{
}

void render_video_gl( struct machine *oric, SDL_bool doublesize )
{
}

void preinit_render_gl( struct machine *oric )
{
  int i;

  for( i=0; i<NUM_TEXTURES; i++ )
    tx[i].buf = NULL;
}

unsigned int rounduppow2( unsigned int x )
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

SDL_bool init_render_gl( struct machine *oric )
{
  int depth, i;
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

  SDL_WM_SetCaption( APP_NAME_FULL, APP_NAME_FULL );

  // Allocate texture buffers
  tx[TEX_VIDEO].w = 256;
  tx[TEX_VIDEO].h = 256;
  tx[TEX_VIDEO].buf = malloc( 256*256*4 );
  if( !tx[TEX_VIDEO].buf ) return SDL_FALSE;

  for( i=0; i<=NUM_TZ; i++ )
  {
    tx[i+TEX_TZ].w = rounduppow2( tz[i]->w * 8 );
    tx[i+TEX_TZ].h = rounduppow2( tz[i]->h * 12 );
    tx[i+TEX_TZ].buf = malloc( tx[i+TEX_TZ].w*tx[i+TEX_TZ].h*4 );
    if( !tx[i+TEX_TZ].buf )
    {
      printf( "Unable to allocate %dx%d texture", tx[i+TEX_TZ].w, tx[i+TEX_TZ].h );
      return SDL_FALSE;
    }
  }

  glGenTextures( NUM_TEXTURES, tex );

  for( i=0; i<NUM_TEXTURES; i++ )
  {
    glBindTexture( GL_TEXTURE_2D, tex[TEX_VIDEO] );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, tx[i].w, tx[i].h, 0, GL_RGBA, GL_UNSIGNED_BYTE, tx[i].buf );
  }

  return SDL_FALSE;
}

void shut_render_gl( struct machine *oric )
{
  int i;

  for( i=0; i<NUM_TEXTURES; i++ )
  {
    if( tx[i].buf ) free( tx[i].buf );
    tx[i].buf = NULL;
  }
}
