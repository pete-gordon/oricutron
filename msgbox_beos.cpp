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
**  BeOS message box
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <Alert.h>

extern "C" {
#include "system.h"
#include "6502.h"
#include "via.h"
#include "8912.h"
#include "gui.h"
#include "disk.h"
#include "monitor.h"
#include "6551.h"
#include "machine.h"
#include "msgbox.h"
}

SDL_bool init_msgbox( struct machine *oric )
{
  return SDL_TRUE;
}

void shut_msgbox( struct machine *oric )
{
}

SDL_bool msgbox( struct machine *oric, int type, char *msg )
{
  switch( type )
  {
    case MSGBOX_YES_NO:
      return ((new BAlert("Oricutron Request", msg, "Yes", "No"))->Go() == 0) ? SDL_TRUE : SDL_FALSE;
//      return (MessageBoxA( hwnd, msg, "Oriculator Request", MB_YESNO ) == IDYES);

    case MSGBOX_OK_CANCEL:
      return ((new BAlert("Oricutron Request", msg, "Ok", "Cancel"))->Go() == 0) ? SDL_TRUE : SDL_FALSE;
//      return (MessageBoxA( hwnd, msg, "Oriculator Request", MB_OKCANCEL ) == IDOK);
    
    case MSGBOX_OK:
      (new BAlert("Oricutron Request", msg, "Ok"))->Go();
      return SDL_TRUE;
  }

  return SDL_TRUE;
}
