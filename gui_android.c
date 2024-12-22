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
 */

/*
 * android helper by iss
 */


#include "system.h"
#include "6502.h"
#include "via.h"
#include "8912.h"
#include "gui.h"
#include "disk.h"
#include "monitor.h"
#include "6551.h"
#include "machine.h"

static SDL_bool initialized = SDL_FALSE;

SDL_bool init_gui_native( struct machine *oric )
{
  if(!initialized)
    initialized = SDL_TRUE;

  /* FIXME: ... */
  return initialized;
}

void shut_gui_native( struct machine *oric )
{
  /* FIXME: ... */
  initialized = SDL_FALSE;
}

void gui_open_url( const char *url )
{
  /* FIXME: ... */
  return;
}
