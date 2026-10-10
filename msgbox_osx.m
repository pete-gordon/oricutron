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
**  Mac OS X message box
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#import <Cocoa/Cocoa.h>

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

SDL_bool init_msgbox( struct machine *oric )
{
  return SDL_TRUE;
}

void shut_msgbox( struct machine *oric )
{
}

static NSModalResponse run_alert( const char *msg, NSString *button1, NSString *button2 )
{
  NSAlert *alert = [[NSAlert alloc] init];
  [alert setMessageText:@"Oricutron Request"];
  [alert setInformativeText:[NSString stringWithUTF8String:msg]];
  [alert addButtonWithTitle:button1];
  if (button2)
    [alert addButtonWithTitle:button2];
  return [alert runModal];
}

SDL_bool msgbox( struct machine *oric, int type, char *msg )
{
  switch( type )
  {
    case MSGBOX_YES_NO:
      return (run_alert(msg, @"Yes", @"No") == NSAlertFirstButtonReturn);

    case MSGBOX_OK_CANCEL:
      return (run_alert(msg, @"Ok", @"Cancel") == NSAlertFirstButtonReturn);

    case MSGBOX_OK:
      run_alert(msg, @"Ok", nil);
      return SDL_TRUE;
  }

  return SDL_TRUE;
}
