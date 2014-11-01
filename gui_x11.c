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

static Display *SDL_Display;
static Window SDL_Window;
static void (*Lock_Display)();
static void (*Unlock_Display)();

static SDL_bool initialized = SDL_FALSE;

static char* text = NULL;

static int clipboard_filter(const SDL_Event *event)
{
  /* Post all non-window manager specific events */
  if ( event->type != SDL_SYSWMEVENT )
  {
    return(1);
  }
  
  /* Handle window-manager specific clipboard events */
  switch (event->syswm.msg->event.xevent.type)
  {
    /* Copy the selection from XA_CUT_BUFFER0 to the requested property */
    case SelectionRequest:
    {
      XSelectionRequestEvent *req;
      XEvent sevent;
      int seln_format;
      unsigned long nbytes;
      unsigned long overflow;
      unsigned char *seln_data;
      
      req = &event->syswm.msg->event.xevent.xselectionrequest;
      sevent.xselection.type = SelectionNotify;
      sevent.xselection.display = req->display;
      sevent.xselection.selection = req->selection;
      sevent.xselection.target = None;
      sevent.xselection.property = None;
      sevent.xselection.requestor = req->requestor;
      sevent.xselection.time = req->time;
      if ( XGetWindowProperty(SDL_Display, DefaultRootWindow(SDL_Display),
        XA_CUT_BUFFER0, 0, INT_MAX/4, False, req->target,
        &sevent.xselection.target, &seln_format,
        &nbytes, &overflow, &seln_data) == Success )
      {
        if ( sevent.xselection.target == req->target )
        {
          if ( sevent.xselection.target == XA_STRING )
          {
            if ( seln_data[nbytes-1] == '\0' )
              --nbytes;
          }
          XChangeProperty(SDL_Display, req->requestor, req->property,
                          sevent.xselection.target, seln_format, PropModeReplace,
                          seln_data, nbytes);
          sevent.xselection.property = req->property;
        }
        XFree(seln_data);
      }
      XSendEvent(SDL_Display,req->requestor,False,0,&sevent);
      XSync(SDL_Display, False);
    }
    break;
  }
  
  /* Post the event for X11 clipboard reading above */
  return(1);
}

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
    if (info.subsystem == SDL_SYSWM_X11)
    {
      SDL_Display = info.info.x11.display;
      SDL_Window = info.info.x11.window;
      Lock_Display = info.info.x11.lock_func;
      Unlock_Display = info.info.x11.unlock_func;
      
      /* Enable the special window hook events */
      SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
      SDL_SetEventFilter(clipboard_filter);
      
      initialized = SDL_TRUE;
    }
    else
    {
      initialized = SDL_FALSE;
    }
  }
}

static char* get_clipboard_text_x11(void)
{
  if (!initialized)
    return NULL;
  
  Atom selection;
  Lock_Display();
  Window owner = XGetSelectionOwner(SDL_Display, XA_PRIMARY);
  Unlock_Display();
  if (owner == None || owner == SDL_Window)
  {
    owner = DefaultRootWindow(SDL_Display);
    selection = XA_CUT_BUFFER0;
  }
  else
  {
    int selection_response = 0;
    SDL_Event event;
    
    owner = SDL_Window;
    Lock_Display();
    selection = XInternAtom(SDL_Display, "Oricutron", False);
    XConvertSelection(SDL_Display, XA_PRIMARY, XA_STRING, selection, owner, CurrentTime);
    Unlock_Display();
    while (!selection_response)
    {
      SDL_WaitEvent(&event);
      if (event.type == SDL_SYSWMEVENT)
      {
        XEvent xevent = event.syswm.msg->event.xevent;
        
        if (xevent.type == SelectionNotify && xevent.xselection.requestor == owner)
          selection_response = 1;
      }
    }
  }
  
  Lock_Display();
  Atom seln_type;
  int seln_format;
  unsigned long nbytes;
  unsigned long overflow;
  char *src;
  if (XGetWindowProperty(SDL_Display, owner, selection, 0, 
    INT_MAX/4, False, XA_STRING, &seln_type, &seln_format, 
    &nbytes, &overflow, (unsigned char **)&src) == Success)
  {
    if (seln_type == XA_STRING)
    {
      text = strdup(src);
    }
    XFree(src);
  }
  Unlock_Display();
  
  return text;
}

static void set_clipboard_text_x11(const char* text)
{
    if (!initialized)
        return;
    
    if ( text != NULL )
    {
        Lock_Display();
        XChangeProperty(SDL_Display, DefaultRootWindow(SDL_Display),
                        (Atom)XA_CUT_BUFFER0, XA_STRING, 8, PropModeReplace, (unsigned char*)text, strlen(text)+1);
        if ( XGetSelectionOwner(SDL_Display, XA_PRIMARY) != SDL_Window )
            XSetSelectionOwner(SDL_Display, XA_PRIMARY, SDL_Window, CurrentTime);
        Unlock_Display();
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
    
    set_clipboard_text_x11(text);
    return SDL_TRUE;
}

SDL_bool clipboard_paste( struct machine *oric )
{
  if(get_clipboard_text_x11())
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
