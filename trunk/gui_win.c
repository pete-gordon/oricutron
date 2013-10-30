/*
 * *  Oricutron
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
    return SDL_FALSE;
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
        return SDL_TRUE;
    }
    return SDL_FALSE;
}
