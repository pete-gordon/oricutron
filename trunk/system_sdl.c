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
**  SDL System specific stuff
*/


#define WANT_WMINFO

#include "system.h"

#if SDL_MAJOR_VERSION == 1
#ifdef __SPECIFY_SDL_DIR__
#include <SDL/SDL_endian.h>
#else
#include <SDL_endian.h>
#endif
#else
#ifdef __SPECIFY_SDL_DIR__
#include <SDL2/SDL_endian.h>
#else
#include <SDL_endian.h>
#endif
#endif

#ifdef __OPENGL_AVAILABLE__
#ifndef __APPLE__
#include <GL/gl.h>
#else
#include <OpenGL/gl.h>
#endif
#endif

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
# define RMASK 0xFF000000
# define GMASK 0x00FF0000
# define BMASK 0x0000FF00
# define AMASK 0x000000FF
#else
# define RMASK 0x000000FF
# define GMASK 0x0000FF00
# define BMASK 0x00FF0000
# define AMASK 0xFF000000
#endif

#if SDL_MAJOR_VERSION == 1
/* NOP */
#else
static SDL_bool g_fullscreen = SDL_FALSE;
static SDL_GLContext g_glcontext = NULL;
static SDL_Renderer* g_renderer = NULL;
static SDL_Texture *g_texture = NULL;
static SDL_Surface* g_screen = NULL;
static SDL_Window* g_window = NULL;
static SDL_Surface* g_icon = NULL;

static int g_bpp = 0;
static int g_width = 0;
static int g_height = 0;

static int g_lastx = SDL_WINDOWPOS_CENTERED;
static int g_lasty = SDL_WINDOWPOS_CENTERED;

static void FreeResources(void)
{
  if(g_glcontext)
  {
    SDL_GL_DeleteContext(g_glcontext);
    g_glcontext = NULL;
  }

  if(g_renderer)
  {
    SDL_DestroyRenderer(g_renderer);
    g_renderer = NULL;
  }

  if(g_texture)
  {
    SDL_DestroyTexture(g_texture);
    g_texture = NULL;
  }

  if(g_screen)
  {
    SDL_FreeSurface(g_screen);
    g_screen = NULL;
  }

  if(g_window)
  {
    SDL_GetWindowPosition(g_window, &g_lastx, &g_lasty);
    SDL_DestroyWindow(g_window);
    g_window = NULL;
  }

}
#endif

#if SDL_MAJOR_VERSION == 1
int SDL_COMPAT_GetWMInfo(SDL_SysWMinfo *info)
{
#ifdef __MORPHOS__
  return 0;
#else
  return SDL_GetWMInfo(info);
#endif
}
#else
int SDL_COMPAT_GetWMInfo(SDL_SysWMinfo *info)
{
  if(g_window)
    return SDL_GetWindowWMInfo(g_window, info);

  return -1;
}
#endif


#if SDL_MAJOR_VERSION == 1
void SDL_COMPAT_WM_SetIcon(SDL_Surface *icon, Uint8 *mask)
{
  SDL_WM_SetIcon(icon, mask);
}
void SDL_COMPAT_WM_SetCaption(const char *title, const char *icon)
{
  SDL_WM_SetCaption(title, icon);
}
#else
void SDL_COMPAT_WM_SetIcon(SDL_Surface *icon, Uint8 *mask)
{
  if(g_icon)
  {
    SDL_FreeSurface(g_icon);
    g_icon = NULL;
  }
  if(icon)
  {
    g_icon = icon;
    g_icon->refcount++;
  }
}
void SDL_COMPAT_WM_SetCaption(const char *title, const char *icon)
{
  if(g_window)
    SDL_SetWindowTitle(g_window, title);
}
#endif


#if SDL_MAJOR_VERSION == 1
SDL_bool SDL_COMPAT_IsAppActive(SDL_Event* event)
{
  SDL_ActiveEvent *act_ev = (SDL_ActiveEvent*)event;
  return (act_ev->state == SDL_APPACTIVE && act_ev->gain == 1)? SDL_TRUE : SDL_FALSE;
}
SDL_bool SDL_COMPAT_IsAppFocused(SDL_Event* event)
{
  switch( event->active.type )
  {
    case SDL_APPINPUTFOCUS:
    case SDL_APPACTIVE:
      return SDL_TRUE;
      break;
  }
  return SDL_FALSE;
}
#else
SDL_bool SDL_COMPAT_IsAppActive(SDL_Event* event)
{
  /* NOTE: not needed wit SDL 2.0
  return (event->window.event == SDL_WINDOWEVENT_FOCUS_GAINED)? SDL_TRUE : SDL_FALSE; */
  return SDL_FALSE;
}
SDL_bool SDL_COMPAT_IsAppFocused(SDL_Event* event)
{
  /* NOTE: not needed wit SDL 2.0
  switch( event->window.event  )
  {
    case SDL_WINDOWEVENT_FOCUS_GAINED:
    case SDL_WINDOWEVENT_ENTER:
      return SDL_TRUE;
      break;
  } */
  return SDL_FALSE;
}
#endif

#if SDL_MAJOR_VERSION == 1
int SDL_COMPAT_EnableKeyRepeat(int delay, int interval)
{
  return SDL_EnableKeyRepeat(delay, interval);
}
#else
int SDL_COMPAT_EnableKeyRepeat(int delay, int interval)
{
  /* This is gone in SDL2 */
  return 0;
}
#endif


#if SDL_MAJOR_VERSION == 1
int SDL_COMPAT_EnableUNICODE(int enable)
{
  return SDL_EnableUNICODE(enable);
}
#else
int SDL_COMPAT_EnableUNICODE(int enable)
{
  /* It's always enabled */
  return 0;
}
#endif

#if SDL_MAJOR_VERSION == 1
SDL_COMPAT_KEY SDL_COMPAT_GetKeysymUnicode(SDL_KEYSYM keysym)
{
  return keysym.unicode;
}
#else
SDL_COMPAT_KEY SDL_COMPAT_GetKeysymUnicode(SDL_KEYSYM keysym)
{
  return keysym.sym;
}
#endif

#if SDL_MAJOR_VERSION == 1
int SDL_COMPAT_Flip(SDL_Surface* screen)
{
  return SDL_Flip(screen);
}
#else
int SDL_COMPAT_Flip(SDL_Surface* screen)
{
  if(g_renderer)
  {
    SDL_UpdateTexture(g_texture, NULL, g_screen->pixels, g_screen->pitch);
    SDL_RenderClear(g_renderer);
    SDL_RenderCopy(g_renderer, g_texture, NULL, NULL);
    SDL_RenderPresent(g_renderer);
  }
  return 0;
}
#endif

#if SDL_MAJOR_VERSION == 1
int SDL_COMPAT_GetBitsPerPixel(void)
{
  const SDL_VideoInfo* info = SDL_GetVideoInfo();
  return (info)? info->vfmt->BitsPerPixel : 0;
}
#else
int SDL_COMPAT_GetBitsPerPixel(void)
{
  int bpp;
  SDL_DisplayMode mode;
  SDL_GetCurrentDisplayMode(0, &mode);
  bpp = SDL_BITSPERPIXEL(mode.format);
  return (bpp==24)? 32 : bpp;
}
#endif

#if SDL_MAJOR_VERSION == 1
int SDL_COMPAT_WM_ToggleFullScreen(SDL_Surface *surface)
{
  return SDL_WM_ToggleFullScreen(surface);
}
#else
int SDL_COMPAT_WM_ToggleFullScreen(SDL_Surface *surface)
{
  if(g_window)
  {
    g_fullscreen = (g_fullscreen)? SDL_FALSE : SDL_TRUE;

    if(g_fullscreen)
      SDL_SetWindowFullscreen(g_window, SDL_WINDOW_FULLSCREEN);
    else
      SDL_SetWindowSize(g_window, g_width, g_height);
  }

  return 0;
}
#endif

#if SDL_MAJOR_VERSION == 1
SDL_Surface* SDL_COMPAT_SetVideoMode(int width, int height, int bitsperpixel, Uint32 flags)
{
  return SDL_SetVideoMode(width, height, bitsperpixel, flags);
}
#else
SDL_Surface* SDL_COMPAT_SetVideoMode(int width, int height, int bitsperpixel, Uint32 flags)
{
  g_width = width;
  g_height = height;
  g_bpp = bitsperpixel;

  FreeResources();

  g_window = SDL_CreateWindow("oricutron", g_lastx, g_lasty,
                              g_width, g_height, flags);
  if(g_icon)
    SDL_SetWindowIcon(g_window, g_icon);

  if(flags & SDL_WINDOW_OPENGL)
  {
    g_screen = SDL_GetWindowSurface(g_window);
    g_glcontext = SDL_GL_CreateContext(g_window);
  }
  else
  {
    g_screen = SDL_CreateRGBSurface(0, g_width, g_height, g_bpp,
                                    RMASK, GMASK, BMASK, AMASK);
    g_renderer = SDL_CreateRenderer(g_window, -1, 0);
    g_texture = SDL_CreateTexture(g_renderer,
                                  SDL_PIXELFORMAT_ABGR8888,
                                  SDL_TEXTUREACCESS_STREAMING,
                                  g_width, g_height);
  }

  return g_screen;
}
#endif

#if SDL_MAJOR_VERSION == 1
int SDL_COMPAT_SetPalette(SDL_Surface *surface, int flags, SDL_Color *colors, int firstcolor, int ncolors)
{
  return SDL_SetPalette(surface, flags, colors, firstcolor, ncolors);
}
#else
int SDL_COMPAT_SetPalette(SDL_Surface *surface, int flags, SDL_Color *colors, int firstcolor, int ncolors)
{
  /* FIXME */
  return 0;
}
#endif

#if SDL_MAJOR_VERSION == 1
void SDL_COMPAT_SetEventFilter(SDL_EventFilter filter)
{
  SDL_SetEventFilter(filter);
}
#else
void SDL_COMPAT_SetEventFilter(SDL_EventFilter filter)
{
  SDL_SetEventFilter(filter, NULL);
}
#endif

#if SDL_MAJOR_VERSION == 1
void SDL_COMPAT_Quit(void)
{
  SDL_Quit();
}
#else
void SDL_COMPAT_Quit(void)
{
  FreeResources();
  SDL_Quit();
}
#endif

#ifdef __OPENGL_AVAILABLE__
#if SDL_MAJOR_VERSION == 1
void SDL_COMPAT_GL_SwapBuffers(void)
{
    SDL_GL_SwapBuffers();
}
#else
void SDL_COMPAT_GL_SwapBuffers(void)
{
    SDL_GL_SwapWindow(g_window);
}
#endif
#endif
