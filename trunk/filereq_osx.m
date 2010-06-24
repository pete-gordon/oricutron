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
**  Mac OS X file dialog
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#import <Cocoa/Cocoa.h>

#include "system.h"
#include "6502.h"
#include "via.h"
#include "8912.h"
#include "disk.h"
#include "gui.h"
#include "machine.h"
#include "monitor.h"
#include "filereq.h"


SDL_bool init_filerequester( void )
{
  return SDL_TRUE;
}

void shut_filerequester( void )
{
}

SDL_bool filerequester( struct machine *oric, char *title, char *path, char *fname, int type )
{
  SDL_bool ret;

  NSString *pat = nil;
  NSString *msg = @"Select the file to open.";
  bool dosavemode = false;

  switch( type )
  {
    case FR_DISKSAVE:
      dosavemode = true;
    case FR_DISKLOAD:
      pat = @"dsk";
      break;
    
    case FR_TAPES:
      pat = @"tap";
      break;
    
    case FR_ROMS:
      pat = @"rom";
      break;

    case FR_SNAPSHOTSAVE:
      dosavemode = true;
    case FR_SNAPSHOTLOAD:
      pat = @"sna";
      break;
 
    default:
      pat = NULL;
      break;
  }

  NSSavePanel *sp = nil;
  NSOpenPanel *op = nil;
  if (dosavemode)
    sp = [[NSSavePanel alloc] init];
  else
    sp = op = [[NSOpenPanel alloc] init];

  [sp setTitle:[NSString stringWithUTF8String:title]];

  if (pat)
    [sp setAllowedFileTypes:[NSArray arrayWithObjects:pat,nil]];

  ret = [op runModal] == NSAlertDefaultReturn;
  if (!ret)
    return ret;

  if (![[op URL] isFileURL])
    return SDL_FALSE;

  strncpy( path,  [[[op directoryURL] path] UTF8String], 4096 ); path[4095] = 0;
  strncpy( fname, [[[op URL] lastPathComponent] UTF8String],   512  ); path[511]  = 0;

  return ret;
}


