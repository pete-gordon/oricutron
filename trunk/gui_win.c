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
 */

/*
 * Clipboard handler by iss
 * Original source: http://www.libsdl.org/projects/scrap
 * Original author: Sam Lantinga
 */


#include <SDL/SDL.h>
#include <SDL/SDL_syswm.h>
#include <limits.h>

#include "system.h"
#include "6502.h"
#include "via.h"
#include "8912.h"
#include "gui.h"
#include "disk.h"
#include "monitor.h"
#include "6551.h"
#include "machine.h"

static HWND SDL_Window;

static SDL_bool initialized = SDL_FALSE;

static char* text = NULL;

static void init_clipboard(void)
{
  if(initialized)
    return;
  
  /* Grab the window manager specific information */
  SDL_SysWMinfo info;
  SDL_VERSION(&info.version);
  if (SDL_GetWMInfo(&info))
  {
    /* Save the information for later use */
    SDL_Window = info.window;
      initialized = SDL_TRUE;
    }
}

char* get_clipboard_text_win(void)
{
  if (!initialized)
    return NULL;
  
  if (IsClipboardFormatAvailable(CF_TEXT) && OpenClipboard(SDL_Window))
  {
    const HANDLE hMem = GetClipboardData(CF_TEXT);
    if (hMem)
    {
      char *src = (char*)GlobalLock(hMem);
      text = strdup(src);
      GlobalUnlock(hMem);
      GlobalFree(hMem);
    }
    CloseClipboard();
  }
  
  return text;
}

static void set_clipboard_text_win(const char* text)
{
    if (!initialized)
        return;
    
    if ( text != NULL )
    {
        if ( OpenClipboard(SDL_Window) )
        {
            HANDLE hMem = GlobalAlloc((GMEM_MOVEABLE|GMEM_DDESHARE), strlen(text)+1);
            if ( hMem != NULL )
            {
                char* dst = (char*)GlobalLock(hMem);
                memcpy(dst, text, strlen(text)+1);
                GlobalUnlock(hMem);
                EmptyClipboard();
                SetClipboardData(CF_TEXT, hMem);
            }
            CloseClipboard();
        }
    }
}

SDL_bool init_gui_native( struct machine *oric )
{
  init_clipboard();
  return initialized;
}

void shut_gui_native( struct machine *oric )
{
  if(text)
  {
    free(text);
    text = 0;
  }
}

void gui_open_url( const char *url )
{
  return;
}

SDL_bool clipboard_copy( struct machine *oric )
{
    // HIRES
    if (oric->vid_addr != oric->vidbases[2])
        return SDL_FALSE;
        
    int line, col, i;
    char text[40 * 28 + 28 + 1];
    unsigned char *vidmem = (&oric->mem[oric->vid_addr]);

    for (i = 0, line = 0; line < 28; line++) {
        for (col = 0; col < 40; col++) {
            unsigned char c = vidmem[line * 40 + col];
            
            if (c > 127) {
                c -= 128;
            }
            
            if (c < ' ' || c == 127) {
                text[i++] = ' ';
            } else
                text[i++] = (char)c;
        }
        text[i++] = '\n';
    }
    text[i++] = '\0';
    //printf("%s\n", text);
    
    set_clipboard_text_win(text);
    return SDL_TRUE;
}

SDL_bool clipboard_paste( struct machine *oric )
{
  if(get_clipboard_text_win())
  {
    char* p = text;
    while(p && *p)
    {
      switch(*p)
      {
        case '\t': *p = ' '; break;
        case '\n': *p = '\r'; break;
        default:
          *p = (*p < 0x20 || 128 <= *p)? ' ' : *p;
          break;
      }
      p++;
    }
    queuekeys(text);
  }
  return SDL_TRUE;
}
